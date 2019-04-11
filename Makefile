CC := g++
MPICXX := mpicxx
CFLAGS  := -g -Wall -I /RHT/boost_1_67_0
LDFLAGS := -lpthread -L /RHT/boost_1_67_0/stage/lib -lboost_system 


rht: server client
.PHONY:rht

server: server.o 
	$(MPICXX) -o server server.o $(LDFLAGS) 

client: common.h client.h client.o
	$(MPICXX) -o client client.o  $(LDFLAGS)

server.o: common.h server.h server.cc
	$(MPICXX) $(CFLAGS) -std=c++17 server.cc -c

client.o: common.h client.h client.cc
	$(MPICXX) $(CFLAGS) -std=c++17 client.cc -c

clean:
	rm client
	rm server
	rm client.o
	rm server.o
