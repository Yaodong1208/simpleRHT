#ifndef COMMON_H
#define COMMON_H

#include <shared_mutex>
#include <future>
#include <string.h>
#include <map>
#include <unistd.h> 
#include <stdio.h> 
#include <iostream>
#include <stdlib.h>    
#include <time.h>
#include <thread>
#include <set>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <functional> //for std::hash
#include <atomic>
#include <fstream>


using namespace std;

#define SOCKET_NUM 8

#define KEY_LEN 3

#define BUFFER 512

#define PORT 8080

typedef char HashKey[KEY_LEN];

enum OperationType {
	PUT = 1,

	GET,

	MULTIPUT
};

template <typename T>
struct HashPair {

	int p1_counter;

	HashKey hash_key;

	T hash_value;

};

struct TCPMessageSTD {

	bool is_end;

	int order;

	long operation_type;

	char message_text[400];
}; 


template <typename T>
struct TCPRequestInfo {

	HashPair<T> hash_pair[3];

};

template < typename T>
struct TCPResponseInfo {

	int status;

	T hash_value;
};

#endif







