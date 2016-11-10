CC ?= gcc

# source directory and output name
SRC_DIR = lib
OUTPUT = libcontrolchain.so

# installation paths
PREFIX = /usr/local
LIBDIR = $(PREFIX)/lib
INCDIR = $(PREFIX)/include
CCDIR = $(INCDIR)/cc

# flags for debugging
ifeq ($(DEBUG), 1)
CFLAGS += -O0 -g -DDEBUG
else
CFLAGS += -O3
endif

# flags
CFLAGS += -Wall -Wextra -fPIC -std=gnu99
LDFLAGS += -shared

# libraries
LIBS = -lserialport -lpthread

# source and object files
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:.c=.o)

# version
VERSION = 0.0.0

$(OUTPUT): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(OUTPUT) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

install:
	install -d $(DESTDIR)$(CCDIR)
	install -d $(DESTDIR)$(LIBDIR)/pkgconfig
	install -m 644 $(SRC_DIR)/*.h $(DESTDIR)$(CCDIR)
	install -m 755 $(OUTPUT) $(DESTDIR)$(LIBDIR)
	sed 's|@prefix@|$(PREFIX)|;s|@libdir@|$(LIBDIR)|;s|@includedir@|$(INCDIR)|;s|@version@|$(VERSION)|' \
		controlchain.pc.in > $(DESTDIR)$(LIBDIR)/pkgconfig/controlchain.pc

clean:
	rm -f $(OBJ) $(OUTPUT)
