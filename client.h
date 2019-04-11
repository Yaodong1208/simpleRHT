#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include <string>
using namespace std;
enum FrequenceLevel {
	LOW,
	MEDIAN,
	HIGH,

};

template<typename T>
void tCPSend();

template<typename T>
void tCPReceive(int sock);

template<typename T>
int requestAssemble(TCPMessageSTD* request);

void generateKey(HashKey key);

template<typename T>
T generateValue();

template<typename T>
int responseProcess(TCPMessageSTD* response);

void timeout();

#endif
