#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import print_function

import socket, json
from threading import Thread, Lock
from base64 import b64decode
from struct import unpack

class ControlChainClient(object):
    BUFFER_SIZE = 8*1024

    def _process_event(self, msg):
        recv = json.loads(msg.decode('utf-8'))
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

            return True

        return False

    def _reader(self):
        while True:
            read_buffer = self.socket.recv(ControlChainClient.BUFFER_SIZE)
            self.read_buffer = None

            if len(read_buffer) == 0:
                continue

            read_buffer = [x for x in read_buffer.split(b'\x00') if x]

            for msg in read_buffer:
                if b'"event"' in msg:
                    if self._process_event(msg):
                        continue

                self.reply = json.loads(msg.decode('utf-8'))
                self.lock.release()

    def __init__(self, socket_path):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.socket.connect(socket_path)
        self.thread = Thread(target=self._reader, daemon=True)
        self.thread.start()
        self.lock = Lock()
        self.lock.acquire()
        self.reply = None
        self._device_status_cb = None
        self._data_update_cb = None

    def _send_request(self, request_name, request_data=None):
        req = {'request':request_name, 'data':request_data}
        to_send = bytes(json.dumps(req).encode('utf-8')) + b'\x00'
        self.socket.send(to_send)

        self.lock.acquire()
        if self.reply['reply'] == request_name:
            return self.reply['data']

        return None

    def assignment(self, assignment):
        if not 'options' in assignment.keys():
            assignment['options'] = []

        reply = self._send_request('assignment', assignment)
        return reply['assignment_id'] if reply else -1

    def unassignment(self, assignment):
        self._send_request('unassignment', assignment)

    def device_list(self):
        return self._send_request('device_list')

    def device_descriptor(self, device_id):
        req = {'device_id':device_id}
        return self._send_request('device_descriptor', req)

    def device_disable(self, device_id):
        req = {'device_id':device_id, 'enable':False}
        return self._send_request('device_control', req)

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
    from time import sleep

    def device_status_cb(device):
        print('device_status_cb:', device)

    def data_update_cb(updates):
        print('data_update_cb:', updates)

    cc = ControlChainClient('/tmp/control-chain.sock')

    cc.device_status_cb(device_status_cb)
    cc.data_update_cb(data_update_cb)

    devices = cc.device_list()
    print('devices list:', devices)

    if not devices:
        exit(0)

    for dev in devices:
        descriptor = cc.device_descriptor(dev)
        print('dev: {0}, descriptor: {1}'.format(dev, descriptor))

    print('creating assignment')
    assignment = {'device_id':1, 'actuator_id':0, 'label':'Gain', 'value':1.0,
                  'min':0.0, 'max':10.0, 'def':4.0, 'mode':0x04, 'steps':32, 'unit':'dB',
                  'options': [{'pi': 3.141593}, {'e': 2.71828}, {'dozen': 12.0}]}

    assignment_id = cc.assignment(assignment)
    print('assignment id:', assignment_id)

    if (assignment_id < 0):
        exit(1)

    sleep(10)

    print('removing assignment')
    assignment_key = {'device_id':1, 'assignment_id':assignment_id}
    cc.unassignment(assignment_key)

    print('done')
