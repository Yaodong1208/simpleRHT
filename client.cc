#include "client.h"
using namespace std;


int operation_number;

float put_percent = 0.5;

float multiput_percent = 0.5;

float get_percent = 0;

int thread_number = 8;


int message_received = 0;

int put_success;

int get_success;

int multiput_success;

int put_fail;

int get_fail;

int multiput_fail;

thread worker[8];

static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

	int main(int argc, char* argv[]){
		const clock_t begin_time = clock();

		srand(time(0));

		operation_number = atoi(argv[1]);

		for(int i = 0; i < thread_number; i++) {
			worker[i] =  thread(tCPSend<int>);
		}

		for(int i = 0; i < thread_number; i++) {
			worker[i].join();
		}
		
		float duration = float( clock () - begin_time ) /  CLOCKS_PER_SEC;
		
		cout<<"\n****************metrics****************\n";
		cout<<"total_process number"<<operation_number<<"\n";
		cout<<"put_success number is "<<put_success<<"\n";
		cout<<"put_fail number is "<<put_fail<<"\n";
		cout<<"get_success number is "<<get_success<<"\n";
		cout<<"get_fail number is "<<get_fail<<"\n";
		cout<<"duration is "<<duration<<"\n";
		

	}

	template<typename T>
	void tCPSend(){
		//*****socket config start ****
		//struct sockaddr_in address; 

		int sock = 0; 

		struct sockaddr_in serv_addr; 
		
		//socket creation
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		{ 
			printf("\n Socket creation error \n"); 
			return; 
		} else {
			cout<<"i'm socket "<<sock<<"\n";
		}

		memset(&serv_addr, '0', sizeof(serv_addr)); 

		serv_addr.sin_family = AF_INET; 
		serv_addr.sin_port = htons(PORT); 
		
		// Convert IPv4 and IPv6 addresses from text to binary form 
		if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) 
		{ 
			printf("\nInvalid address/ Address not supported \n"); 
			return ; 
		} 

		//connect server
		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
		{ 
			printf("\nConnection Failed \n"); 
			return ; 
		}
		//****socket config end****

		//continually send request
		int message_sent = 0;
		bool terminate;
		while(!terminate) {

			
			
			TCPMessageSTD request;

			request.is_end = false;

			if(message_sent == operation_number/8 - 1) {

				request.is_end = true;

				terminate = true;
			}

			//assemble the request by calling requestAssemble(request)
			if(requestAssemble<T>(&request)) {

				perror("assemble message failed");

				continue;
			}
			//send message by socket

			TCPRequestInfo<T>* info = ((TCPRequestInfo<T>*)(request.message_text));

			if(request.operation_type == MULTIPUT) {

				//printf("opertion_type = %d, hash_key[2] = %s, hash_value[2] = %i\n", request.operation_type, info->hash_pair[2].hash_key, info->hash_pair[2].hash_value);
			}

			

			send(sock, &request, sizeof(TCPMessageSTD), 0);

			message_sent ++;
			
			if(request.operation_type == PUT){

			} else if (request.operation_type == GET){

			} else {
			
			}

			//receive message in a close loop
			tCPReceive<T>(sock);
		}
	} 

	template<typename T>
	int requestAssemble(TCPMessageSTD* request) {

		int status = 0;

		TCPRequestInfo<T>* info = (TCPRequestInfo<T>*)request->message_text;

		// generate info by the hash_value_type and put_percent
		
		//fulfill request->operation_type;
		if(rand()%100 < put_percent*100) {

			request->operation_type = PUT;

			HashPair<T>* hp = &info->hash_pair[0];

			generateKey(hp->hash_key);

			hp->hash_value = generateValue<T>();

			hp->p1_counter = 0;


		}else if (rand() % 100 <(put_percent + multiput_percent)* 100) {

			request->operation_type = MULTIPUT;

			for (int i = 0; i < 3; i++) {

				HashPair<T>* hp = &info->hash_pair[i];

				generateKey(hp->hash_key);

				hp->hash_value = generateValue<T>();

				hp->p1_counter = 0;
			}

		}else {

			request->operation_type = GET;

			HashPair<T>* hp = &info->hash_pair[0];

			generateKey(hp->hash_key);

			hp->hash_value = 0;

			hp->p1_counter = 0;
		}

		return status;
	}

void generateKey(HashKey key){
	int i = 0;

    for (; i < KEY_LEN - 1; ++i) {

        key[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
	key[i] = 0;

}

	template<typename T>
	T generateValue() {
		T rand_val = (T)rand();
		return rand_val;
	}

	template<typename T>
	void tCPReceive(int sock){
		
		//receive one TCP response

		TCPMessageSTD response;

		//call read to receive response
		read(sock, &response, BUFFER);

		++message_received;

		printf("number of message_received is %i\n", message_received);

		//calling responseProcess to process the response
		int status = responseProcess<T>(&response);

		if(status)
		{
			perror("error in response process");
		};

		return;

	}

	template<typename T>
	int responseProcess(TCPMessageSTD* response){

		int status = 0;

		TCPResponseInfo<T> *info = (TCPResponseInfo<T>*) (response->message_text);

		//generate readable_resposne from info
		

		if(response->operation_type == PUT) {

			if(!info->status) {
			
				++put_success;
			} else {

				++put_fail;
			}

		} else if(response->operation_type == GET){

			if(!info->status){

				++get_success;

			} else {

				get_fail += 1;

			}

		} else {

			if(!info->status) {

				++multiput_success;

			} else {

				++multiput_fail;

			}
		}

		return status;

	}