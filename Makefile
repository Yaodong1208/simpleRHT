CXX := mpicxx
CPPFLAGS  := -g -Wall -std=c++17 -I /mirror/rht/boost_1_70_0 #-I /RHT/boost_1_70_0 -std=c++17
LDLIBS := -lpthread  -L /mirror/rht/boost_1_70_0/stage/lib -lboost_system #-L /RHT/boost_1_70_0/stage/lib -lboost_system
.INTERMEDIATE : %.o

rht: server client
.PHONY:rht

client: client.o
	$(CXX) $< -o $@ 

clean:
	-rm client
	-rm server
