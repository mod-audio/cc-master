cc-master
=========

This is the master implementation of the Control Chain protocol. It is a socket server software
which communicates with devices connected to a specific serial port and convert the Control Chain
protocol to JSON data.

Installation
------------

cc-master uses waf to build and install the files. The usual steps to build and install are:

    ./waf configure
    ./waf build
    ./waf install

You can change the base installation path passing `--prefix` argument to the configure command.

    ./waf configure --prefix=/usr


Dependencies:

    - jansson
    - libserialport


Running
-------

The cc-master is daemon program, which means that once you start it, it'll fork and run as a
background software. The execution syntax is below.

```bash
controlchaind <serialport> [-b baudrate]
```

For sake of debugging you want to run it on the foreground and display the debug messages.

```bash
export LIBCONTROLCHAIN_DEBUG=1
controlchaind <serialport> -f
```

You can also set the variable `LIBCONTROLCHAIN_DEBUG` to 2 to have more verbose messages.
