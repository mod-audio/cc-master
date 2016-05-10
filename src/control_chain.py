#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from ctypes import *

class ControlChainMsg(Structure):
    _fields_ = [
        ("dev_address", c_ubyte),
        ("command", c_ubyte),
        ("data_size", c_ushort),
        ("data", POINTER(c_uint8)),
    ]

lib = cdll.LoadLibrary("../control-chain.so")

# cc_handle_t* cc_init(const char *port_name, int baudrate);
lib.cc_init.argtypes = [c_char_p, c_int]
lib.cc_init.restype = c_void_p

# void cc_finish(cc_handle_t *handle);
lib.cc_finish.argtypes = [c_void_p]
lib.cc_finish.restype = None

# void cc_set_recv_callback(cc_handle_t *handle, void (*callback)(void *arg));
CBFUNC = CFUNCTYPE(None, POINTER(ControlChainMsg))
lib.cc_set_recv_callback.argtypes = [c_void_p, CBFUNC]
lib.cc_set_recv_callback.restype = None

# void cc_send(cc_handle_t *handle, cc_msg_t *msg);
lib.cc_send.argtypes = [c_void_p, POINTER(ControlChainMsg)]
lib.cc_send.restype = None

class ControlChain(object):
    def __init__(self, serial_port, baudrate):
        self.obj = lib.cc_init(serial_port.encode('utf-8'), baudrate)

        if self.obj == None:
            raise NameError('Cannot create ControlChain object, check serial port')

        self.recv_callback = CBFUNC(self._parser)
        lib.cc_set_recv_callback(self.obj, self.recv_callback)

    def __del__(self):
        lib.cc_finish(self.obj)

    def send(self, msg):
        a = ARRAY(c_uint8, msg[2])
        d = a(*msg[3])
        msg[3] = d
        x = ControlChainMsg(*msg)
        lib.cc_send(self.obj, pointer(x))

    def _parser(self, arg):
        print('callback')
        print(arg.contents.dev_address)
        print(arg.contents.command)
        print(arg.contents.data_size)
        print(arg.contents.data)
        print(arg.contents.data[0])
        print(arg.contents.data[1])

### test

cc = ControlChain('/dev/ttyACM0', 115200)
