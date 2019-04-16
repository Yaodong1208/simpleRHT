CXX := mpicxx
CPPFLAGS  := -g -Wall -I /mirror/ub/boost_1_67_0 #-I /RHT/boost_1_67_0 -std=c++17
LDLIBS := -lpthread  -L /mirror/ub/boost_1_67_0/stage/lib -lboost_system #-L /RHT/boost_1_67_0/stage/lib -lboost_system
.INTERMEDIATE : %.o

rht: server client
.PHONY:rht

client: client.o
	$(CXX) $< -o $@ 

clean:
	-rm client
	-rm server
