
CC ?= gcc

# source directory and output name
SRC_DIR = src
OUTPUT = libcontrol_chain.so

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

$(OUTPUT): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(OUTPUT) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

install:
	install -d $(DESTDIR)$(CCDIR)
	install -d $(DESTDIR)$(LIBDIR)
	install -m 644 src/*.h $(DESTDIR)$(CCDIR)
	install -m 755 $(OUTPUT) $(DESTDIR)$(LIBDIR)

clean:
	rm -f $(SRC_DIR)/*.o $(OUTPUT)
