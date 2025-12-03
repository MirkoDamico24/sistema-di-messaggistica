#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>

/*sockt libs*/
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

//private libs
#include "types.h"
#include "server_ops.h"
#include "sock_ops.h"

#define error(x) {printf("%s\n", x); exit(EXIT_FAILURE);}
#define MAX_TRIES 5  //max number of tries done if accept failes

__thread char uname[U_SIZE]; //current user's name

/*
 * The following function extracts data from the given buffer
 * and provides it to the login function. The socket descriptor is
 * used to inform the client of the failure or success of the 
 * operation.
 * Return 0 on success, -1 on failure
*/
int login_info(authentication *msg, int sock_ds)
{
	int state, ret;
	s_replay replay;

	replay.info.msg_type = S_REPLAY;

	ret = login(msg->uname, msg->passwd, &state);
	if(ret == -1){
		if(state == 1){			//when state = 1, no match has been found
			replay.error = 1;
			replay.error_code = NO_MATCH;
			replay.info.len = sizeof(s_replay) - sizeof(header);
		}
	}
	else{
		if(ret == -2){
			replay.error = 1;
			replay.error_code = FDERROR;
			replay.info.len = sizeof(s_replay) - sizeof(header);
		}
		replay.error = 0;
		replay.info.len = sizeof(replay.error);	//error_code = d.c.c
	}

	//sending data on socket
	ret = send_msg(sock_ds, &replay);
	if(ret == -1 || ret == -2){			//operations already finalized, not important if client shutdown the communication
		perror("Error occured on sending to client the replay");
		return -1;
	}	
	return 0;
}

/*
 * The following function extracts the registration info
 * from the buffer and calls the signin function. 
 * The socket descriptor is used to send the replay to 
 * the client.
 * Return 0 on success, -1 on failure.*/
int signin_info(authentication *msg, int sock_ds)
{
	int ret;
	s_replay replay;

	replay.info.msg_type = S_REPLAY;

	ret = signin(msg->uname, msg->passwd);
	if(ret == -1){
		replay.error = 1;
		replay.error_code = EXALREADY;
		replay.info.len = sizeof(s_replay) - sizeof(header);
	}
	else{
		if(ret == -2){
			replay.error = 1;
			replay.error_code = FDERROR;
			replay.info.len = sizeof(s_replay) - sizeof(header);
		}
		replay.error = 0;
		replay.info.len = sizeof(replay.error);
	}

	ret = send_msg(sock_ds, &replay);
	if(ret == -1 || ret == -2){
		perror("Error occurred on sending replay to client");
		return -1;
	}

	return 0;
}

/*
 * The following function reads the opcode in the buffer
 * and calls the function to perform the required op.
 * Return 0 on success, -1 on failure, -2 if it tries to write
 * on a socket cloed by client.
 */
int select_oper(operation *msg, int sock_ds, char *username)
{
	int ret, msg_type, len, n_msg, sem_ds;
	long *id_l, file_size, i;
	s_replay replay;
	delete ids;
	msg_end end;
	char *file_mapping, *buffer;

	replay.info.msg_type = S_REPLAY;
	replay.info.len = sizeof(s_replay) - sizeof(header);
	file_mapping = NULL;

	ret = read_msg(sock_ds, username, &replay, &file_mapping, &file_size, &sem_ds);
	if(ret == -1) goto send_rep;
	//sending file content to client

	buffer = malloc(sizeof(char)*sizeof(data));
	if(buffer == NULL) {
		printf("Malloc failed\n");
		pthread_exit(NULL);
	}

	i = 0;
	while(i < file_size){
		memcpy(buffer, file_mapping + i, sizeof(data));
		ret = send_msg(sock_ds, buffer);
		if(ret == -1) return -1;
		if(ret == -2) return -2;

		i+=sizeof(data);
	}
			
	//signaling the end of the message
	end.info.msg_type = STREAM_END;
	end.info.len = 0; //the message only has the header
	ret = send_msg(sock_ds, &end);
	if(ret == -1 || ret == -2) return -1;

	if(msg->opcode == DELETE){
		//request to client to provide the ids
		ids.info.msg_type = DELETE_IDS;
		ids.info.len = 0;		//no payload
		len = sizeof(delete) - sizeof(ids.msg_ids);
		memcpy(buffer, &ids, len);
		ret = send_msg(sock_ds, buffer);
		if(ret == -1) return -1;
		if(ret == -2) return -2;

		//waiting for ids
		ret = read_from_socket(sock_ds, buffer, &msg_type);
		if(ret == 0) return -2;
		if(ret == -1) return -1;
		memcpy(&ids, buffer, sizeof(delete));

		//retrive of ids from buffer
		ret = id_tokenize(ids.msg_ids, &id_l, &n_msg, file_size);
		if(ret == -1){
			replay.error = 1;
			replay.error_code = EMEMLEAK;
			goto send_rep;
		}

		ret = delete_msg(file_mapping, username, n_msg, id_l);
		if(ret == -1){
			replay.error = 1;
			replay.error_code = FDERROR;
			goto send_rep;
		}
		if(ret == -2){
			replay.error = 1;
			replay.error_code = EFDGENERIC;
			goto send_rep;
		}
		if(n_msg > 0) free(id_l);
	}

	//sending replay to client
	send_rep:
		semaphore_operations(&sem_ds, 0, 1);		//post on the semaphore acquired in the read_msg function		
		ret = send_msg(sock_ds, &replay);
		if(ret == -1 || ret == -2){				//ret = -2 not handled properly, because the connection will be closed in thread main func.
			perror("Error on replay");
			return -1;
		}

	if(file_mapping != NULL){
		munmap(file_mapping, file_size);
		free(buffer);
	}
	return 0;
}


/*
 * The following function delivers the msg to the 
 * intented end user. The sock_ds argument is used
 * to send the replay to client, while the username
 * argument contains the uname of the user
 * sending data
 * Return 0 on success, -1 on failure*/
int send_data(data *msg, int sock_ds, char *username)
{
	int ret;
	s_replay replay;
	
	replay.info.msg_type = S_REPLAY;
	replay.info.len = sizeof(s_replay) - sizeof(header);

	ret = deliver_msg(msg, username, &replay);
	//error checking here is not needed, because the function fills itself the replay to send
	
	//sending response to client
	ret = send_msg(sock_ds, &replay);
	if(ret == -1 || ret == -2){
		perror("Error on replay sending");
		return -1;
	}

	return 0;
}

void *server_t(void *ptr)
{
	int sock_ds, ret, prev_r, res_r, type, state;
	char buffer[sizeof(data)];    //size of buffer is to contain the max struct dim

	sock_ds = *(int *)ptr;
	printf("sock_ds value is %d\n", sock_ds);

	while(1){

		//taking data from socket
		ret = read_from_socket(sock_ds, buffer, &type);
		if(ret == -1){
			perror("Error on reading data from socket");
			pthread_exit(NULL);
		}
		if(ret == 0) break;	//connection closed
		
		switch (type){
			case LOG_MSG: authentication *aut_data;
							aut_data = (authentication *) buffer;
							ret = login_info((authentication *)buffer, sock_ds);
						  if(ret == -1){
							  perror("Error on data acquisition");
							  pthread_exit(NULL);
						  }
						  memcpy(uname,aut_data->uname, sizeof(aut_data->uname));
						  break;
		
			case SIGN_MSG: authentication *aut_data1;
						   aut_data1 = (authentication *) buffer;
							ret = signin_info((authentication *)buffer, sock_ds);
						   if(ret == -1){
							   perror("Error on connection");
							   pthread_exit(NULL);
						   }
						   printf("Server entry point uname = %s\n", aut_data1->uname);
						   memcpy(uname, aut_data1->uname, sizeof(aut_data1->uname));
					       break;

			case OPER: ret = select_oper((operation *)buffer, sock_ds, uname);
				       if(ret == -1){
						   perror("Error occurred whilst communicating with client");
						   pthread_exit(NULL);
					   }
					   if(ret == -2){
							perror("Client lost connection\n");
							close(sock_ds);
							pthread_exit(NULL);
					   }
				       break;

			case DATA: ret = send_data((data *)buffer, sock_ds, uname);
				       if(ret == -1){
						   perror("Error on sending client response");
						   pthread_exit(NULL);
					   }
				       break;
		}
	}
	return NULL;
}

int main(void)
{
	int ret, sock_ds, conn_sock, tries;
	unsigned int size;
	pthread_t tid;
	struct sockaddr_in server_addr, client_addr;
	struct sigaction act;

	printf("Server startup\n");

	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);	//explicitly ignoring SIGPIPE, which can be received when writing on a closed connection socket
	
	printf("---SOKCET SETUP---\n");

	sock_ds = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_ds == -1) error("Error on socket");
	printf("Main says: sock_ds = %d\n", sock_ds);

	printf("---SOCKET BINDING---\n");

	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(S_PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	ret = bind(sock_ds, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr_in));
	if(ret == -1) error("Error on socket binding");

	printf("---TRANSITIONING TO LISTENING STATE---\n");

	ret = listen(sock_ds, 0);		//backlog len is set to 0, which means that default setting will aplly. That's ok beacuse the server will spwn thread for each request
	if(ret == -1) error("Error on socket binding");

	size = sizeof(struct sockaddr_in);
	while(1){
		system("clear");
		
		tries = 0;
		printf("---WAITING FOR A CONNECTION REQUEST---\n");
		
		memset(&client_addr, 0, sizeof(struct sockaddr_in));
		retry:
			conn_sock = accept(sock_ds, (struct sockaddr *)(&client_addr), &size);
			if(conn_sock == -1){
				if(tries == MAX_TRIES){
					printf("Server could not accept the request\n");
					continue;
				}
				tries++;
			}

		//spawning thread to serve the client */
		ret = pthread_create(&tid, NULL, server_t, &conn_sock);
		if(ret != 0) error("Error on thread spawning");

		//detaching created thread
		ret = pthread_detach(tid);
		if(ret != 0) error("Error on thread detach");
	}

	exit(0);
}
