CXX = clang++
FLAGS = -lboost_system -pthread -std=c++14

PHONY: all
all: server

server: server.cpp
    $(CXX) -o $@ $(FLAGS) $<

client: client.cpp
    $(CXX) -o $@ $(FLAGS) $<


.PHONY: clean
clean:
    rm -rf server client 2>/dev/null
