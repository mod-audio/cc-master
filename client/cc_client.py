#!/usr/bin/env python
# -*- coding: utf-8 -*-

import socket, json
from threading import Thread, Lock
from base64 import b64decode
from struct import unpack

class ControlChainClient(object):
    BUFFER_SIZE = 8*1024

    def _reader(self):
        while True:
            read_buffer = self.socket.recv(ControlChainClient.BUFFER_SIZE)
            self.read_buffer = None

            if len(read_buffer) == 0:
                continue

            read_buffer = read_buffer[:-1]

            if b'"event"' in read_buffer:
                recv = json.loads(read_buffer.decode('utf-8'))
                if 'event' in recv.keys():
                    if recv['event'] == 'device_status':
                        if self._device_status_cb:
                            self._device_status_cb(recv['data'])
                    elif recv['event'] == 'data_update':
                        decoded = b64decode(recv['data']['raw_data'])
                        n, raw = decoded[0], decoded[1:]
                        updates = [raw[i:i+5] for i in range(0, len(raw), 5)]
                        update_list = []
                        for update in updates:
                            a, v = unpack('=Bf', update)
                            update_list.append({'assignment_id':a, 'value':v})

                        updates = recv['data']
                        updates.pop('raw_data')
                        updates['updates'] = update_list

                        if self._data_update_cb:
                            self._data_update_cb(updates)

                    continue

            self.read_buffer = read_buffer.decode('utf-8')
            self.lock.release()

    def __init__(self, socket_path):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.socket.connect(socket_path)
        self.thread = Thread(target=self._reader)
        self.thread.start()
        self.lock = Lock()
        self.lock.acquire()
        self.read_buffer = None
        self._device_status_cb = None
        self._data_update_cb = None

    def _send_request(self, request_name, request_data=None):
        req = {'request':request_name, 'data':request_data}
        to_send = bytes(json.dumps(req).encode('utf-8')) + b'\x00'
        self.socket.send(to_send)

        self.lock.acquire()
        reply = json.loads(self.read_buffer)
        if reply['reply'] == request_name:
            return reply['data']

        return None

    def assignment(self, assignment):
        reply = self._send_request('assignment', assignment)
        return reply['assignment_id'] if reply else -1

    def unassignment(self, unassignment):
        self._send_request('unassignment', unassignment)

    def device_list(self):
        return self._send_request('device_list')

    def device_descriptor(self, device_id):
        req = {'device_id':device_id}
        return self._send_request('device_descriptor', req)

    def device_status_cb(self, callback):
        self._device_status_cb = callback
        value = 1 if callback else 0
        enable = {'enable':value}
        self._send_request('device_status', enable)

    def data_update_cb(self, callback):
        self._data_update_cb = callback
        value = 1 if callback else 0
        enable = {'enable':value}
        self._send_request('data_update', enable)

if __name__ == "__main__":
    def device_status_cb(device):
        print(device)

    def data_update_cb(updates):
        print(updates)

    cc = ControlChainClient('/tmp/control-chain.sock')

    cc.device_status_cb(device_status_cb)
    cc.data_update_cb(data_update_cb)

    devices = cc.device_list()
    for dev in devices:
        descriptor = cc.device_descriptor(dev)
        print('dev: {0}, descriptor: {1}'.format(dev, descriptor))

    assignment = {'device_id':1, 'actuator_id':0, 'value':1.0, 'min':0.0, 'max':2.0,
                  'def':1.5, 'mode':1}

    print('assignment id: ', cc.assignment(assignment))

    from time import sleep
    sleep(0.5)

    unassignment = {'device_id':1, 'assignment_id':0}
    cc.unassignment(unassignment)
