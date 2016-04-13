
CC ?= gcc

# source directory and output name
SRC_DIR = src
OUTPUT = control-chain

# flags
CFLAGS += -O3 -Wall -Wextra
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
