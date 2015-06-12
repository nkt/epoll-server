CC=gcc
CCFLAGS=-Wall
BIN_NAME=server

$(BIN_NAME):
	$(CC) $(CCFLAGS) -o $(BIN_NAME) main.c

clean:
	rm $(BIN_NAME)

.PHONY: $(BIN_NAME) clean
