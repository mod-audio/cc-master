
CC ?= gcc

# source directory and output name
SRC_DIR = src
OUTPUT = control-chain

# flags
ifeq ($(DEBUG), 1)
CFLAGS += -O0 -Wall -Wextra -g -DDEBUG
else
CFLAGS += -O3 -Wall -Wextra
endif
LDFLAGS +=

# libraries
LIBS = -lserialport -lpthread

# source and object files
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:.c=.o)

$(OUTPUT): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(OUTPUT) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f $(SRC_DIR)/*.o $(OUTPUT)
