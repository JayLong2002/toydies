# Compiler settings
CC := g++
CFLAGS := -Wall -Wextra -std=c++11

# Source files
SRCS := server.cc hashtable.cc client.cc
OBJS := $(SRCS:.cc=.o)

# Targets
all: server client

server: server.o hashtable.o
	$(CC) $(CFLAGS) $^ -o $@

client: client.o
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.cc
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f server client $(OBJS) hashtable.o