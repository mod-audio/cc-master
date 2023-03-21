#!/bin/bash

set -e

cd $(dirname ${0})

if [ ! -e ../mod-plugin-builder ]; then
  echo "mod-plugin-builder does not exist as sibling directory"
  exit 1
fi

if ping -c 1 -W 0.05 192.168.51.1 > /dev/null; then
  TARGET=root@192.168.51.1
elif ping -c 1 -W 0.2 moddwarf.local > /dev/null; then
  TARGET=root@moddwarf.local
elif ping -c 1 -W 0.2 modduox.local > /dev/null; then
  TARGET=root@modduox.local
elif ping -c 1 -W 0.2 modduo.local > /dev/null; then
  TARGET=root@modduo.local
else
  echo "not connected"
  exit 1
fi

PLATFORM=$(curl -s ${TARGET}/system/info | jq -r .platform)
if [ -z "${PLATFORM}" ]; then
  echo "failed to query platform"
  exit 1
fi
PLATFORM="mod${PLATFORM}-new-debug"

# build dev stuff if not done yet
WORKDIR=${WORKDIR:=~/mod-workdir}
if [ ! -e ${WORKDIR}/${PLATFORM}/staging/usr/lib/libjansson.so ] || [ ! -e ${WORKDIR}/${PLATFORM}/staging/usr/lib/libserialport.so ]; then
  ../mod-plugin-builder/bootstrap.sh ${PLATFORM} dev
fi

source ../mod-plugin-builder/local.env ${PLATFORM}

rm -rf build

if [ ! -e build ]; then
  ${HOST_DIR}/usr/bin/python ./waf configure --prefix=/usr --debug --nocache
fi
${HOST_DIR}/usr/bin/python ./waf
${HOST_DIR}/usr/bin/python ./waf install --destdir=$(pwd)/destdir

# needed since ssh rsa deprecation/breakage
SSH_OPTIONS="-o PubkeyAcceptedAlgorithms=+ssh-rsa"

ssh ${SSH_OPTIONS} ${TARGET} mount / -o remount,rw
ssh ${SSH_OPTIONS} ${TARGET} systemctl stop controlchaind.service controlchaind.socket

scp ${SSH_OPTIONS} destdir/usr/bin/controlchaind ${TARGET}:/usr/bin/
scp ${SSH_OPTIONS} destdir/usr/lib/lib*.so       ${TARGET}:/usr/lib/

echo "all ok"
