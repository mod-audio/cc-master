#!/usr/bin/env python

top     = '.'       # Source directory
out     = 'build'   # Build directory

def options(opt):
    opt.recurse('lib')
    opt.recurse('server')
    opt.recurse('client')

def configure(conf):
    print('Configuring Control Chain Library')
    conf.recurse('lib')
    print('Configuring Control Chain Server')
    conf.recurse('server')
    print('Configuring Control Chain Client')
    conf.recurse('client')

def build(bld):
    bld.recurse('lib')
    bld.recurse('server')
    bld.recurse('client')
