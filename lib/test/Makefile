CC ?= gcc

# source directory
SRC_DIR = .

# flags for debugging
ifeq ($(DEBUG), 1)
CFLAGS += -O0 -g -DDEBUG
else
CFLAGS += -O3
endif
CFLAGS += -Wall -Wextra -Werror

# flags
LIB_PATH=../../build/lib
LIB_NAME=$(shell basename $(LIB_PATH)/*.so)
CFLAGS += -Wall -Wextra -std=c99
LDFLAGS += -L$(LIB_PATH) -Wl,-rpath=$(LIB_PATH)

# includes and libraries
INCS = -I../src
LIBS = -l:$(LIB_NAME)

# source, object and output
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:.c=.o)
OUTPUTS = $(SRC:.c=.bin)

all: $(OUTPUTS)

%.bin: $(OBJ)
	$(CC) $(@:.bin=.o) $(LDFLAGS) -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCS) -o $@ -c $<

clean:
	rm -f $(OBJ) *.bin

run-tests:
	@for f in *.bin; do valgrind --leak-check=full --show-leak-kinds=all ./$$f; echo; done
