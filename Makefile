BINARY_NAME=main
CC=gcc
LIB_DIR=/usr/local/lib
INCLUDE_DIR=/usr/local/include

# Configuration for Linux, modify to build for Windows
# Add .exe format
# Change compiler and paths

all: $(BINARY_NAME)

$(BINARY_NAME):
	$(CC) $(BINARY_NAME).c -o $(BINARY_NAME) -L$(LIB_DIR) -I$(INCLUDE_DIR) -I. -lraylib -lGL -lm -g -O3

clean:
	rm $(BINARY_NAME)

.PHONY: all clean
