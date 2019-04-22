#include "server.h"
#include <mpi.h>

using namespace std;
	
	map<string,int> hash_table;

	shared_mutex latch[ LOCK_NUM ];

	//the first int is lock_no the second is count;
	map<long, set<int>> lock_table;

	mutex local_lock[8]; //in this case we use 8, but can more

	mutex remote_lock[UUID_NUM + 1];

	int local_rank = 0;

	int uuid_count = 0;

	int world_rank;

	int world_size;

	mutex tcp_lock;

	//long locked_id[LOCK_NUM] = {0};

	map<long,Record<int>> record_table;

	hash<string> hasher;

	bool locked[LOCK_NUM] = {0};

	boost::asio::thread_pool pool;

	atomic_int throughput_counter = 0;

	std::ofstream throughput_file;

	//do some config in main then start therads
	int main(){
		//
		MPI_Init(NULL,NULL);
		//set world_rank and world_size by mpi_call
		MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

		MPI_Comm_size(MPI_COMM_WORLD, &world_size);

		cout<<"my world_rank is "<<world_rank<<"\n";

		srand(time(0));

		thread t1(tCPReceive<int>);
		thread t2(mPIReceive<int>);
		thread t3(monitor);
		t1.join();
		t2.join();
		t3.join();

	}
		
	////////////////////////////////////////
	////////    TCP Message handler
	///////////////////////////////////////
template<typename T>
void tCPReceive() {
		//****socket config start****
		int server_fd, new_socket; 

		struct sockaddr_in address; 

		int opt = 1; 

		int addrlen = sizeof(address);
		
		// Creating socket file descriptor 
		if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
		{ 
			perror("socket failed"); 

			exit(EXIT_FAILURE); 
		} 
		
		// Forcefully attaching socket to the port 8080 
		if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
													&opt, sizeof(opt))) 
		{ 
			perror("setsockopt"); 
			exit(EXIT_FAILURE); 
		} 
		address.sin_family = AF_INET; 

		address.sin_addr.s_addr = INADDR_ANY; 

		address.sin_port = htons( PORT ); 
		
		// Forcefully attaching socket to the port 8080 
		if (bind(server_fd, (struct sockaddr *)&address, 
									sizeof(address))<0) 
		{ 
			perror("bind failed"); 
			exit(EXIT_FAILURE); 
		} 

		if (listen(server_fd, 3) < 0) 
		{ 
			perror("listen"); 
			exit(EXIT_FAILURE); 
		} 
		//****socket config end****

		//use new therads to handle sockets which create by client, we only consider 1 client, otherwise need to maintain a map for different client
		while(true) {

			if ((new_socket = accept(server_fd, (struct sockaddr *)&address, 
							(socklen_t*)&addrlen))<0) 
			{ 
				perror("accept"); 
				exit(EXIT_FAILURE); 
			}

			printf("accept new client\n");
		
			boost::asio::post(pool, boost::bind(tCPProcess<T>, new_socket));

	}
}

    template<typename T>
	void tCPProcess(int socket) {

		bool terminate = false;

		long uuid = (long)socket << (8 * sizeof(int));

		int temp;

		tcp_lock.lock();

		temp = local_rank++;

		tcp_lock.unlock();

		uuid += temp;
		
		char buffer[BUFFER];
		memset(buffer,0,512);

		while(!terminate) {

			char temp_buffer[BUFFER];

			//the TCP_end may be set between last set of terminate and this new round, so check it first
			
			read(socket, temp_buffer, BUFFER); 

			printf("testing...\n");

			if(!strcmp(buffer,temp_buffer)) {
				continue;
			}else {
				printf("new read\n");
				memcpy(buffer, temp_buffer, BUFFER);
			}

			

			TCPMessageSTD* TCP_request = (TCPMessageSTD*)buffer;

			//the first time see TCP_end means this is the last request should be handled
			if(TCP_request->is_end) {
				
				//this socket will be closed after respond to client
				terminate = true;

			}

			//parse TCP_request to record_table

			record_table[uuid].operation_type = (OperationType)TCP_request->operation_type;

			memset(&(record_table[uuid].request),0,sizeof(HashPair<T>) * 3);
			
			memcpy(&(record_table[uuid].request), &(((TCPRequestInfo<T>*)(TCP_request->message_text))->hash_pair), sizeof(HashPair<T>) * 3);
			
			record_table[uuid].decision = COMMIT;

			record_table[uuid].ack_counter = 0;

			record_table[uuid].send_counter = 0;

			record_table[uuid].part.clear();

			if(record_table[uuid].operation_type == MULTIPUT) {

				//printf("store hash_key = %s, hash_value = %i\n", record_table[uuid].request[2].hash_key, record_table[uuid].request[2].hash_value);
			}
			
			   
			//phase1a<T>(uuid);
			

		}

	}

/////////////////////////////////////////////
///////////// MPI Message handler
////////////////////////////////////////////
      int findNode1(HashKey key){

		return key[0] % world_size;

	}

	int findNode2(HashKey key) {

		return (key[0] + 1) % world_size;

	}

    template<typename T>
	void mPIReceive() {

		while(true) {

			MPI_Status status;

			MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);


			switch(status.MPI_TAG) {

				case ONEAMESSAGE:

					OneAMessage one_a_message;

					MPI_Recv(
					/* data         = */ &one_a_message, 
					/* count        = */ sizeof(OneAMessage), 
					/* datatype     = */ MPI_BYTE, 
					/* source       = */ status.MPI_SOURCE, 
					/* tag          = */ status.MPI_TAG, 
					/* communicator = */ MPI_COMM_WORLD, 
					/* status       = */ MPI_STATUS_IGNORE);

					printf("myrank = %i, recive one_a_message from %ld\n",world_rank, one_a_message.uuid);
					
					boost::asio::post(pool, boost::bind(phase1b<T>, one_a_message, status.MPI_SOURCE));

					break;

				case ONEBMESSAGE:

					OneBMessage<T> one_b_message;

					//receive MPI_respponse
					MPI_Recv(
					/* data         = */ &one_b_message, 
					/* count        = */ sizeof(OneBMessage<T>), 
					/* datatype     = */ MPI_BYTE, 
					/* source       = */ status.MPI_SOURCE, 
					/* tag          = */ status.MPI_TAG, 
					/* communicator = */ MPI_COMM_WORLD, 
					/* status       = */ MPI_STATUS_IGNORE);

					//printf("myrank = %i, recive one_b_message from %i, uuid = %ld\n",world_rank,status.MPI_SOURCE, one_b_message.uuid);

					//use oenBMessageProcess to process one_b_message
					boost::asio::post(pool, boost::bind(oneBMessageProcess<T>, one_b_message, status.MPI_SOURCE));

					break;

				/*case TWOAMESSAGE:

					TwoAMessage<T> two_a_message;

					//receive MPI_respponse
					MPI_Recv(
					 &two_a_message, 
					 sizeof(TwoAMessage<T>), 
					 MPI_BYTE, 
					 status.MPI_SOURCE, 
					 status.MPI_TAG, 
					 MPI_COMM_WORLD, 
					 MPI_STATUS_IGNORE);

					boost::asio::post(pool, boost::bind(phase2b<T>, two_a_message, status.MPI_SOURCE));

					break;

				case TWOBMESSAGE:

					TwoBMessage two_b_message;

					//receive MPI_respponse
					MPI_Recv(
					 &two_b_message, 
					 sizeof(TwoAMessage<T>), 
					 MPI_BYTE, 
					 status.MPI_SOURCE, 
					 status.MPI_TAG, 
					 MPI_COMM_WORLD, 
					 MPI_STATUS_IGNORE);

					boost::asio::post(pool, boost::bind(twoBMessageProcess<T>, two_b_message));*/

			}

		}

	}
	template <typename T>
	void oneBMessageProcess(OneBMessage<T> one_b_message, int source) {

		printf("process 1b message\n");

		fflush(stdout);

		long uuid = one_b_message.uuid;
				
		long local_rank = uuid << 32;

		local_rank = local_rank >> 32;

		local_lock[local_rank].lock();

		switch (record_table[uuid].operation_type) {

			case GET: {

				TCPMessageSTD TCP_message_std;

				TCP_message_std.operation_type = GET;

				((TCPResponseInfo<T>*)TCP_message_std.message_text)->status = one_b_message.status[0];

				if(!one_b_message.status[0]) {
					((TCPResponseInfo<T>*)(TCP_message_std.message_text))->hash_value = one_b_message.hash_value;
				}

				int sock = uuid >> 32;

				printf("%ld send get result to %i\n", uuid, sock);

				send(sock, &TCP_message_std, sizeof(TCPMessageSTD), 0);

				record_table[uuid].part.clear();

				throughput_counter ++;

				break;
			}



			/*case PUT: {
				

				record_table[uuid].ack_counter++;

				if(one_b_message.status[0] == 0) {

				record_table[uuid].request[0].p1_counter++;
				}

				if(record_table[uuid].ack_counter == record_table[uuid].send_counter) {

					record_table[uuid].ack_counter = 0;

					if(record_table[uuid].request[0].p1_counter == 2) {

						record_table[uuid].decision = COMMIT;

						phase2a<T>(uuid, COMMIT);

						printf("commit %ld\n", uuid);

					} else {

						record_table[uuid].decision = ABORT;

						printf("abort %ld\n", uuid);

						phase2a<T>(uuid, ABORT);
						
					}
	
				}

				break;
			}



			case MULTIPUT: {
					

					record_table[uuid].ack_counter++;

					for(int i = 0; i < 3; i++) {

						if(one_b_message.status[i] == 0) {

							record_table[uuid].request[i].p1_counter++;

						}

					}

					if(record_table[uuid].ack_counter == record_table[uuid].send_counter) {

						record_table[uuid].ack_counter = 0;
						
						for(int i = 0 ; i < 3; i++) {

							if(record_table[uuid].request[0].p1_counter != 2) {

								record_table[uuid].decision = ABORT;
								
								phase2a<T>(uuid, ABORT);

								break;

							}
						}

						record_table[uuid].decision = COMMIT;

						phase2a<T>(uuid, COMMIT);

						
							
					}
			}*/

		}
		local_lock[local_rank].unlock();

	}

template <typename T>
void twoBMessageProcess(TwoBMessage two_b_message){

	long uuid = two_b_message.uuid;
				
	long local_rank = uuid << 32;

	local_rank = local_rank >> 32;

	local_lock[local_rank].lock();

	record_table[uuid].ack_counter++;

	if(record_table[uuid].ack_counter == record_table[uuid].send_counter) {

		

		if(record_table[uuid].decision == ABORT) {

			//printf("ABORT success by %ld, restart\n", uuid);

			//reset
			record_table[uuid].decision = COMMIT;

			record_table[uuid].ack_counter = 0;

			for(int i = 0; i < 3; i++) {

				record_table[uuid].request[i].p1_counter = 0;
			}

			//restart after a while
			
			phase1a<T>(uuid);
			

		} else {

			//printf("COMMIT success by %ld\n", uuid);

			//send tcp response
			TCPMessageSTD tcp_message_std;

			tcp_message_std.operation_type = record_table[uuid].operation_type;

			((TCPResponseInfo<T>*)tcp_message_std.message_text)->status = 0;

			int sock = uuid >> 32;

			send(sock, &tcp_message_std, sizeof(TCPMessageSTD), 0);

			throughput_counter++;

		}
	} 
		
	local_lock[local_rank].unlock();
}

////////////////////////////////////////
////////////// phase1 handler
///////////////////////////////////////

    template <typename T>
	void phase1a(long uuid) {

		OneAMessage one_a_message;

		one_a_message.uuid = uuid;

		one_a_message.operation_type = record_table[uuid].operation_type;

		switch (one_a_message.operation_type){
		
			/*case MULTIPUT:{

				for(int i = 0; i < 3; i ++) {

					memcpy(&one_a_message.hash_key[i], &record_table[uuid].request[i].hash_key, KEY_LEN);

					record_table[uuid].part.insert(findNode1(one_a_message.hash_key[i]));

					record_table[uuid].part.insert(findNode2(one_a_message.hash_key[i]));

				}

				break;
			}*/

			case GET:{

				memcpy(&one_a_message.hash_key[0], &record_table[uuid].request[0].hash_key, KEY_LEN);

				//for GET only one phase is needed, and send to only one node
				if(world_rank == findNode1(one_a_message.hash_key[0]))

					record_table[uuid].part.insert(findNode1(one_a_message.hash_key[0]));

				else 

					record_table[uuid].part.insert(findNode2(one_a_message.hash_key[0]));

				break;
			}

			/*case PUT: {

				memcpy(&one_a_message.hash_key[0], &record_table[uuid].request[0].hash_key, KEY_LEN);

				record_table[uuid].part.insert(findNode1(one_a_message.hash_key[0]));

				record_table[uuid].part.insert(findNode2(one_a_message.hash_key[0]));
			}*/

		}

		record_table[uuid].send_counter = record_table[uuid].part.size();
		

		for(auto node = record_table[uuid].part.begin(); node != record_table[uuid].part.end(); node++) {
			MPI_Send(
			&one_a_message, 
			sizeof(OneAMessage), 
			MPI_BYTE, 
			*node,
			ONEAMESSAGE, 
			MPI_COMM_WORLD
			);
			//int sock = one_a_message.uuid>>32;
			//printf("send one_a_message from %ld,  sock = %i, send_node = %i\n", one_a_message.uuid,sock, *node);
		}
		
	}

    template<typename T>
	void phase1b(OneAMessage one_a_message, int source){
		long uuid = one_a_message.uuid;

		//printf("send one_b_message from %i, to %i, uuid = %ld\n", world_rank, source, one_a_message.uuid);

		OneBMessage<T> one_b_message;

		one_b_message.uuid = one_a_message.uuid;

		string temp[3];

		switch(one_a_message.operation_type) {

		case GET:{

				temp[0] = one_a_message.hash_key[0];

				latch[hasher(temp[0])%LOCK_NUM].lock_shared();

				one_b_message.status[0] = get<T>(temp[0], &one_b_message.hash_value);

				//printf("get shared lock %ld\n", hasher(temp[0])%LOCK_NUM);

				latch[hasher(temp[0])%LOCK_NUM].unlock_shared();

				break;
		}

		/*case PUT:{

			temp[0] = one_a_message.hash_key[0];

			if(latch[hasher(temp[0])%LOCK_NUM].try_lock()) {

				if(!locked[hasher(temp[0])%LOCK_NUM]) {

					locked[hasher(temp[0])%LOCK_NUM] = true;

					one_b_message.status[0] = 0;

					printf("lock %ld on %i success by %ld\n", (hasher(temp[0])%LOCK_NUM), world_rank, uuid);

					lock_table[uuid].insert((hasher(temp[0])%LOCK_NUM));

				} else {

					one_b_message.status[0] = 1;

						printf("lock %ld on %i fail by %ld because of locked\n", (hasher(temp[0])%LOCK_NUM), world_rank, uuid);


				}

				latch[(hasher(temp[0])%LOCK_NUM)].unlock();
				printf("unlock trylock %ld on %i by %ld\n", hasher(temp[0])%LOCK_NUM, world_rank, uuid);

			} else {

				one_b_message.status[0] = 1;

				printf("lock %ld on %i fail by %ld because of try_lock\n", hasher(temp[0])%LOCK_NUM, world_rank, uuid);
				
				}
			break;
		}
			

		case MULTIPUT: {

			for(int i = 0; i < 3; i++) {

				temp[i] = one_a_message.hash_key[i];
				
				if(findNode1(one_a_message.hash_key[i]) == world_rank || findNode2(one_a_message.hash_key[i]) == world_rank) {
				//this item is on this server

					auto it = lock_table[uuid].find(hasher(temp[i])%LOCK_NUM);

					
					if(it != lock_table[uuid].end()) {
						//we've already lock it 

						one_b_message.status[i] = 0;

					} else {
						//need to lock but haven't lock yet
						
						if(!latch[hasher(temp[i])%LOCK_NUM].try_lock()) {

						//fail to lock
							one_b_message.status[i] = 1;

						} else {
						//check the locked

							if(!locked[hasher(temp[0])%LOCK_NUM]) {
								//success to lock
								locked[hasher(temp[0])%LOCK_NUM] = true;
								
								one_b_message.status[i] = 0;

								lock_table[uuid].insert(hasher(temp[0])%LOCK_NUM);

							} else {

								one_b_message.status[i] = 1;

							}
							
							latch[hasher(temp[i])%LOCK_NUM].unlock();
						}
					}
				} else {
				//this item is not on this server

					one_b_message.status[i] = 1;
				}
			}
		}*/
	}
	
		

		/*MPI_Send(
		&one_b_message, 
		sizeof(OneBMessage<T>), 
		MPI_BYTE, 
		source,
		ONEBMESSAGE, 
		MPI_COMM_WORLD
		);*/

		//printf("send 1b message to %i from %ld\n", source, world_rank);

		fflush(stdout);


		

	}

	////////////////////////////////////////////
	///////////// phase2 handler
	///////////////////////////////////////////

    template <typename T>
 
	void phase2a(long uuid, Decision decision) {

		TwoAMessage<T> two_a_message;

		two_a_message.uuid = uuid;

		two_a_message.operation_type = record_table[uuid].operation_type;

		two_a_message.decision = decision;

		switch(record_table[uuid].operation_type) {

			case PUT: {

				if(decision == COMMIT) {

					memcpy(&two_a_message.hash_pair[0], &record_table[uuid].request[0], sizeof(HashPair<T>));

				}

				break;				
			}

			case MULTIPUT:{

				if(decision == COMMIT) {

					memcpy(&two_a_message.hash_pair, &record_table[uuid].request, sizeof(HashPair<T>) * 3);

				}
				break;
			}

			case GET: {
				return;
			}

		}

		for(auto it = record_table[uuid].part.begin(); it != record_table[uuid].part.end(); it++) {

		printf("decision is %i by %ld, send to %i\n", two_a_message.decision, two_a_message.uuid, *it);

			MPI_Send(
			/* data         = */ &two_a_message, 
			/* count        = */ sizeof(TwoAMessage<T>), 
			/* datatype     = */ MPI_BYTE, 
			/* destination  = */ *it,
			/* tag          = */ TWOAMESSAGE, 
			/* communicator = */ MPI_COMM_WORLD
			);
		}



	}
    template <typename T>
	void phase2b(TwoAMessage<T> two_a_message, int source) {

		TwoBMessage two_b_message;

		two_b_message.uuid = two_a_message.uuid;

		two_b_message.status = 0;

		long uuid = two_b_message.uuid;

		string temp[3];

		switch (two_a_message.decision) {

			case ABORT:

				break;
			
			case COMMIT:

				if(two_a_message.operation_type == PUT) {

					temp[0] = two_a_message.hash_pair[0].hash_key;

					put(temp[0], &two_a_message.hash_pair[0].hash_value);
				} else {

					for(int i = 0; i < 3; i++) {

						temp[i] = two_a_message.hash_pair[i].hash_key;
						
						if(findNode1(two_a_message.hash_pair[i].hash_key) == world_rank || findNode2(two_a_message.hash_pair[i].hash_key) == world_rank) {

							put(temp[i], &two_a_message.hash_pair[i].hash_value);
						}
				
					}
				}
		}

		for(auto it = lock_table[uuid].begin(); it != lock_table[uuid].end(); it++) {

			printf("unlock %i for %ld on %i\n", *it, uuid, world_rank);

			locked[*it] = false;

		}

		lock_table[uuid].clear();

		MPI_Send(
		/* data         = */ &two_b_message, 
		/* count        = */ sizeof(TwoBMessage), 
		/* datatype     = */ MPI_BYTE, 
		/* destination  = */ source,
		/* tag          = */ TWOBMESSAGE, 
		/* communicator = */ MPI_COMM_WORLD
		);

	}

	///////////////////////////////////////////////
	/////////// get put handler
	//////////////////////////////////////////////

    template <typename T>
	int get(string hash_key, T* value) {

		

		auto it = hash_table.find(hash_key);

		if(it == hash_table.end()) {

			return 1;

		}else {

			memcpy(value, &hash_table[hash_key], sizeof(T));

		}

		return 0;
	}

	template <typename T>
	void put(string hash_key, T* value) {

		memcpy(&hash_table[hash_key], value, sizeof(T));

	}


/////////////////////////////////////////
///////////// monitor
/////////////////////////////////////////
void monitor(){
	while(true) {
		int prior = throughput_counter.load();
		usleep(1000);
		if(throughput_counter.load() - prior != 0) {
			throughput_file.open("throughput.txt", std::ios_base::app);
			throughput_file << throughput_counter.load() - prior;
		}
	}

}










