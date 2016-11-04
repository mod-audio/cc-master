#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from ctypes import *

class String(Structure):
    _fields_ = [
        ("size", c_ubyte),
        ("text", c_char_p),
    ]

class Data(Structure):
    _fields_ = [
        ("assigment_id", c_int),
        ("value", c_float),
    ]

class UpdateList(Structure):
    _fields_ = [
        ("count", c_int),
        ("list", POINTER(Data)),
    ]

class Actuator(Structure):
    _fields_ = [
        ("id", c_int),
    ]

class DevDescriptor(Structure):
    _fields_ = [
        ("label", POINTER(String)),
        ("actuators_count", c_int),
        ("actuators", POINTER(POINTER(Actuator))),
    ]

class Assignment(Structure):
    _fields_ = [
        ("id", c_int),
        ("device_id", c_int),
        ("actuator_id", c_int),
        ("value", c_float),
        ("min", c_float),
        ("max", c_float),
        ("def", c_float),
        ("mode", c_uint32),
    ]

class Unassignment(Structure):
    _fields_ = [
        ("device_id", c_int),
        ("assignment_id", c_int),
    ]

lib = cdll.LoadLibrary("../libcontrol_chain.so")

# cc_handle_t* cc_init(const char *port_name, int baudrate);
lib.cc_init.argtypes = [c_char_p, c_int]
lib.cc_init.restype = c_void_p

# void cc_finish(cc_handle_t *handle);
lib.cc_finish.argtypes = [c_void_p]
lib.cc_finish.restype = None

#int cc_assignment(cc_handle_t *handle, cc_assignment_t *assignment);
lib.cc_assignment.argtypes = [c_void_p, POINTER(Assignment)]
lib.cc_assignment.restype = int

#void cc_unassignment(cc_handle_t *handle, cc_unassignment_t *unassignment)
lib.cc_unassignment.argtypes = [c_void_p, POINTER(Unassignment)]
lib.cc_unassignment.restype = None

#void cc_data_update_cb(cc_handle_t *handle, void (*callback)(void *arg));
DATA_CB = CFUNCTYPE(None, POINTER(UpdateList))
lib.cc_data_update_cb.argtypes = [c_void_p, DATA_CB]
lib.cc_data_update_cb.restype = None

#void cc_dev_descriptor_cb(cc_handle_t *handle, void (*callback)(void *arg));
DEVDESC_CB = CFUNCTYPE(None, POINTER(DevDescriptor))
lib.cc_dev_descriptor_cb.argtypes = [c_void_p, DEVDESC_CB]
lib.cc_dev_descriptor_cb.restype = None

class ControlChain(object):
    def __init__(self, serial_port, baudrate):
        self.obj = lib.cc_init(serial_port.encode('utf-8'), baudrate)

        if self.obj == None:
            raise NameError('Cannot create ControlChain object, check serial port')

        # define device descriptor callback
        self._dev_desc_cb = DEVDESC_CB(self._dev_desc)

        # define data update callback
        self._data_update_cb = DATA_CB(self._data_update)

    def __del__(self):
        try:
            lib.cc_finish(self.obj)
        except NameError:
            pass

    def _dev_desc(self, arg):
        dev_descriptor = {}
        dev_descriptor['label'] = arg.contents.label.contents.text.decode('utf-8')

        actuators = []
        for i in range(arg.contents.actuators_count):
            actuator = {}
            actuator['id'] = arg.contents.actuators[i].contents.id
            actuators.append(actuator)

        dev_descriptor['actuators'] = actuators
        self.user_dev_desc_cb(dev_descriptor)

    def _data_update(self, arg):
        updates = []
        for i in range(arg.contents.count):
            update = {}
            update['assigment_id'] = arg.contents.list[i].assigment_id
            update['value'] = arg.contents.list[i].value
            updates.append(update)

        self.user_data_update_cb(updates)

    def device_descriptor_cb(self, callback):
        lib.cc_dev_descriptor_cb(self.obj, self._dev_desc_cb)
        self.user_dev_desc_cb = callback

    def data_update_cb(self, callback):
        lib.cc_data_update_cb(self.obj, self._data_update_cb)
        self.user_data_update_cb = callback

    def assignment(self, assignment):
        assignment = Assignment(*assignment)
        return lib.cc_assignment(self.obj, assignment)

    def unassignment(self, unassignment):
        unassignment = Unassignment(*unassignment)
        lib.cc_unassignment(self.obj, unassignment)

### test
if __name__ == "__main__":
    def new_device(dev_desc):
        print(dev_desc)

    def data_update(updates):
        print(updates)

    cc = ControlChain('/dev/ttyACM0', 115200)
    cc.device_descriptor_cb(new_device)
    cc.data_update_cb(data_update)

    import time
    time.sleep(1)

    assignment_id = cc.assignment((-1, 1, 0, 1.0, 0.0, 1.0, 0.0, 1))
    time.sleep(3)

    cc.unassignment((1, assignment_id))
    time.sleep(1)
