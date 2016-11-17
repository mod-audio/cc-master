CC ?= gcc

# source directory and output name
SRC_DIR = .
OUTPUT = controlchaind

# installation paths
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

# flags for debugging
ifeq ($(DEBUG), 1)
CFLAGS += -O0 -g -DDEBUG
else
CFLAGS += -O3
endif

# flags
CFLAGS += -Wall -Wextra
LDFLAGS +=

# libraries
LIBS = -lcontrolchain -pthread

# source and object files
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:.c=.o)

$(OUTPUT): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(OUTPUT) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

install:
	install -m 755 $(OUTPUT) $(DESTDIR)$(BINDIR)

clean:
	rm -f $(OBJ) $(OUTPUT)
