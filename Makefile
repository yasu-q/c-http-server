# Compiler and flags
CC = gcc
CFLAGS = -pedantic-errors -Wall -fstack-protector-all -Werror

# paths
INCLUDE_DIR = include
SRC_DIR = src
UTIL_DIR = $(SRC_DIR)/util
TEST_DIR = test
BIN_DIR = bin

# include headers
INCLUDES = -I$(INCLUDE_DIR)

all: $(BIN_DIR) $(BIN_DIR)/main 

# check if bin exists
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# =---------- SRC ----------=

# TODO: maybe clean these up
# main executable
$(BIN_DIR)/http-server: $(BIN_DIR)/http-server.o $(BIN_DIR)/arraylist.o $(BIN_DIR)/http-util.o $(BIN_DIR)/sock-util.o
	$(CC) $(BIN_DIR)/http-server.o $(BIN_DIR)/arraylist.o $(BIN_DIR)/http-util.o $(BIN_DIR)/sock-util.o -o $(BIN_DIR)/http-server
# main object
$(BIN_DIR)/http-server.o: $(SRC_DIR)/http-server.c $(INCLUDE_DIR)/http-server.h $(INCLUDE_DIR)/arraylist.h $(INCLUDE_DIR)/http-util.h $(INCLUDE_DIR)/sock-util.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SRC_DIR)/http-server.c -o $(BIN_DIR)/http-server.o

# =---------- TEST ----------=

# listen server executable
$(BIN_DIR)/listen-server.out: $(BIN_DIR)/listen-server.o $(BIN_DIR)/sock-util.o
	$(CC) $(BIN_DIR)/listen-server.o $(BIN_DIR)/sock-util.o -o $(BIN_DIR)/listen-server.out
# listen server object
$(BIN_DIR)/listen-server.o: $(TEST_DIR)/listen-server.c $(INCLUDE_DIR)/sock-util.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(TEST_DIR)/listen-server.c -o $(BIN_DIR)/listen-server.o

# http parser executable
$(BIN_DIR)/http-parser.out: $(BIN_DIR)/http-parser.o $(BIN_DIR)/http-util.o $(BIN_DIR)/arraylist.o
	$(CC) $(BIN_DIR)/http-parser.o $(BIN_DIR)/http-util.o $(BIN_DIR)/arraylist.o -o $(BIN_DIR)/http-parser.out
# http parser object
$(BIN_DIR)/http-parser.o: $(TEST_DIR)/http-parser.c $(INCLUDE_DIR)/http-util.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(TEST_DIR)/http-parser.c -o $(BIN_DIR)/http-parser.o

# array list test executable
$(BIN_DIR)/arraylist-test.out: $(BIN_DIR)/arraylist-test.o $(BIN_DIR)/arraylist.o
	$(CC) $(BIN_DIR)/arraylist-test.o $(BIN_DIR)/arraylist.o -o $(BIN_DIR)/arraylist-test.out
# array list test object
$(BIN_DIR)/arraylist-test.o: $(TEST_DIR)/arraylist-test.c $(INCLUDE_DIR)/arraylist.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(TEST_DIR)/arraylist-test.c -o $(BIN_DIR)/arraylist-test.o

# =---------- UTIL ----------=

# sock util object
$(BIN_DIR)/sock-util.o: $(UTIL_DIR)/sock-util.c $(INCLUDE_DIR)/sock-util.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(UTIL_DIR)/sock-util.c -o $(BIN_DIR)/sock-util.o

# http util object
$(BIN_DIR)/http-util.o: $(UTIL_DIR)/http-util.c $(INCLUDE_DIR)/http-util.h $(INCLUDE_DIR)/arraylist.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(UTIL_DIR)/http-util.c -o $(BIN_DIR)/http-util.o

# array list object
$(BIN_DIR)/arraylist.o: $(UTIL_DIR)/arraylist.c $(INCLUDE_DIR)/arraylist.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(UTIL_DIR)/arraylist.c -o $(BIN_DIR)/arraylist.o

# clean - remove all .o and .out files in bin
clean:
	@rm -f $(BIN_DIR)/*.o $(BIN_DIR)/*.out $(BIN_DIR)/main