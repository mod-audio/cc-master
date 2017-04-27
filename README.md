cc-master
=========

This is the master implementation of the Control Chain protocol.

Running the control chain daemon on the MOD Duo
-----------------------------------------------

In order to debug control chain communication, you can enable debug messages in the control chain daemon running on the MOD duo:

```bash
ssh root@192.168.51.1
systemctl stop controlchaind
export LIBCONTROLCHAIN_DEBUG=1 # use 2 for verbose debug
controlchaind.run
```

Local installation
------------------

Instead of using a MOD Duo, you can also run a control chain daemon on your local machine. Before installation, make sure you have Python 3, [libserialport](https://sigrok.org/wiki/Libserialport) and the [jansson](http://www.digip.org/jansson/) library installed.

Then, follow these steps to install cc-master:

```bash
git clone https://github.com/moddevices/cc-master.git
cd cc-master
./waf configure --prefix=/usr
./waf build
./waf install
```

Running the control chain daemon
--------------------------------

After installation, follow these steps to run the control chain daemon in verbose debug message mode. Adapt the serial port device name as needed:

```
export LIBCONTROLCHAIN_DEBUG=2
controlchaind /dev/ttyACM0 -f
```
