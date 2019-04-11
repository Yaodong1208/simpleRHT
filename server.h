#ifndef SERVER_H
#define SERVER_H
#include "common.h"
#include <mpi.h>
#define LOCK_NUM 100

#define UUID_NUM 40

using namespace std;

    

enum MPITag {
	ONEAMESSAGE,
	ONEBMESSAGE,
	TWOAMESSAGE,
	TWOBMESSAGE,
};

enum Decision {
	COMMIT,
	ABORT
};

template<typename T>
struct Record{
	OperationType operation_type;
	Decision decision;
	int ack_counter;
	int send_counter;
	HashPair<T> request[3];
	set<int> part;
};

struct OneAMessage {
	long uuid;          
	long operation_type;
	HashKey hash_key[3];
};

template <typename T>
struct OneBMessage {
	long uuid;
	int status[3];
	T hash_value;
};

template <typename T>
struct TwoAMessage {
	long uuid;
	OperationType operation_type;
	Decision decision;
	HashPair<T> hash_pair[3];
};

struct TwoBMessage {
	long uuid;
	int status;
};

template <typename T>
extern void phase1a(long uuid);

template<typename T>
extern void phase1b(OneAMessage one_a_message, int source);

template <typename T>
extern void phase2a(long uuid, Decision decision);

template <typename T>
extern void phase2b(TwoAMessage<T> two_a_message, int source);
 
template <typename T>
extern void tCPReceive();

template <typename T>
extern void tCPProcess(int socket);

template <typename T>
extern void mPIReceive();

template <typename T>
extern void oneBMessageProcess(OneBMessage<T> one_b_message, int source);

template <typename T>
extern Decision abortHelper(long uuid, int source, int status, int p1, HashKey hash_key, T hash_value);

template <typename T>
extern void twoBMessageProcess(TwoBMessage two_b_message);

extern int findNode1(HashKey hash_key);

extern int findNode2(HashKey hash_key);

template <typename T>
extern int get(string hash_key, T* value);

template <typename T>
extern void put(string hash_key, T* value);

extern map<string,int> hash_table;

extern shared_mutex latch[ LOCK_NUM ];

//the long is uuid, first int is lock_no the second is count which means how many times the uuid attach the same lock
extern map<long, set<int>> lock_table;

extern  mutex  local_lock[SOCKET_NUM];

extern mutex remote_lock[UUID_NUM + 1];

extern int local_rank;

extern int world_rank;

extern int world_size;

extern mutex tcp_lock;

extern map<long,Record<int>> record_table;

extern hash<string> hasher;

#endif