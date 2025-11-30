#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
/*
 following libraries are for 
 socket communication
 */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
//private libraries
#include "client_ops.h"
#include "client_types.h"
#include "../libs/sock_ops.h"

#define error(x) {printf("%s\n", x); exit(EXIT_FAILURE);}

/*
 *The following function writes data into the authentication
 *structur, which brings data either for login or sign-in
 *The second argument is used to determine whether the required 
 *operation is a login or sign in, which is already requested to user
 *in the caller.
 */
void aut_input(authentication *aut_ptr, int type)
{
	char username[U_SIZE], password[U_SIZE];
	int ret, msg_type;
	char buffer[sizeof(s_replay)];
	s_replay rep;

	printf("Please enter a unique username (max 1024 characters): ");
	fgets(username, U_SIZE, stdin);
	fflush(stdout);

	printf("Please enter a strong password (min. 8 characters, max 1024): ");
	fgets(password, U_SIZE, stdin);
	fflush(stdout);

	//filling the struct fields
	switch (type){
		case 1: aut_ptr->info.msg_type = LOG_MSG;
				break;
		case 2: aut_ptr->info.msg_type = SIGN_MSG;
				break;
		default: error("Msg type specified not supported");
	}

	
	aut_ptr->info.len = sizeof(authentication) - sizeof(header);
	
	memcpy(&aut_ptr->uname, username, U_SIZE);
	memcpy(&aut_ptr->passwd, password, U_SIZE);
	return;
}

void delete_msg(int sock_ds)
{
	int ret, msg_type;
	delete id;
	s_replay rep;
	char *buffer;

	buffer = malloc(sizeof(char) * sizeof(data));
	if(buffer == NULL){
		printf("Error on malloc\n");
		exit(-1);
	}

	id.info.msg_type = DELETE;
	id.info.len = sizeof(delete) - sizeof(header);

	printf("Provide the id of messages to delete (if more than one, separate them with blank): ");
	fgets(id.msg_ids, O_SIZE, stdin);
	fflush(stdout);

	ret = send_msg(sock_ds, &id);
	if(ret == -1){
		perror("Server currently unavailable\n");
		exit(-1);
	}
	if(ret == -2){
		perror("Connection with server lost. Client shuting down\n");
		exit(-1);
	}

	//waiting for server respone
	ret = read_from_socket(sock_ds, buffer, &msg_type);
	if(ret == 0){
		perror("Connection with server lost. Client shuting down\n");
		exit(-1);
	}
	if(ret == -1){
		perror("Server currently unavailable\n");
		exit(-1);
	}

	memcpy(&rep, buffer, sizeof(s_replay));

	if(rep.error == 1){
		if(rep.error_code = EMEMLEAK){
			printf("Server coudl not perform the operation. Try again\n");
			return;
		}
		if(rep.error_code == FDERROR){
			printf("Server encountred error while using msg file. Try again\n");
			return;
		}
		if(rep.error_code == EFDGENERIC){
			printf("Server encountred error while finalizing deletion of msg file. Try again\n");
			return;
		}
	}

	return;
}

void request_read(int sock_ds, operation *oper, int op_type){
	int ret, msg_type;
	char *buffer;
	data msg;
	s_replay rep;

	oper->info.msg_type = OPER;	
	oper->info.len = sizeof(operation) - sizeof(header);
	if(op_type == READ) oper->opcode = READ;
	else oper->opcode = DELETE;

	ret = send_msg(sock_ds, oper);
	if(ret == -2){
		perror("Connection with server lost while sending request to read. Client shuting down\n");
		exit(-1);
	}
	if(ret == -1){
		perror("Server currently unavailable\n");
		exit(-1);
	}

	buffer = malloc(sizeof(char)*sizeof(data));
	if(buffer == NULL){
		printf("Unavailable memory on client. Shuting down\n");
		exit(-1);
	}

	//reading file from server
	ret = read_from_socket(sock_ds, buffer, &msg_type);
	if(ret == 0){
		perror("Connection with server lost. Client shuting down\n");
		exit(-1);
	}
	if(ret == -1){
		perror("Server currently unavailable\n");
		exit(-1);
	}
	if(msg_type == S_REPLAY){
		memcpy(&rep, buffer, sizeof(s_replay));
		if(rep.error == 1){
			if(rep.error_code == ENOMSGF){
				printf("There are no messages\n");
				return;
			}
			if(rep.error_code == EFDGENERIC || rep.error_code == EMEMGEN){
				perror("Server currently unavailable\n");
				exit(-1);
			}
			if(rep.error_code == EMEMLEAK){
				perror("Server resources are insufficient to perform the operation\n");
				exit(-1);
			}
		}
	}

	while(msg_type != STREAM_END){
		memcpy(&msg, buffer, sizeof(data));
		printf("Msg id = %d\n", msg.id);
		printf("Object = %s", msg.obj);
		printf("Source user = %s\n", msg.src_uname);
		printf("Message content = %s", msg.text);

		ret = read_from_socket(sock_ds, buffer, &msg_type);
		if(ret == 0){
			perror("Connection with server lost. Client shuting down\n");
			exit(-1);
		}
		if(ret == -1){
			perror("Server currently unavailable\n");
			exit(-1);
		}
	}

	printf("\t\t---ALL MESSAGES HAVE BEEN PRINTED---\n");

	ret = read_from_socket(sock_ds, buffer, &msg_type);
	if(ret == 0){
		printf("Connection with server lost. Client shuting down\n");
		exit(-1);
	}

	if(msg_type == DELETE_IDS){
		delete_msg(sock_ds);
	}
	free(buffer);
	return;
}

void oper_input(operation *oper_ptr, int opcode)
{
	oper_ptr->info.msg_type = OPER;
	//oper_ptr->info.len = sizeof(int);
	oper_ptr->info.len = sizeof(operation) - sizeof(header);
	oper_ptr->opcode = opcode;
	return;
}


/*
 * The following function fills the data structur, which is used to 
 * transfer the content of a message.
 */
void data_input(int sock_ds)
{
	int len, o_len, u_len, t_len, ret, msg_type;
	char obj[O_SIZE], dest_uname[U_SIZE], text[T_SIZE], buffer[sizeof(data)];
	s_replay rep;
	data *data_input;

	data_input = (data *)malloc(sizeof(data));
	if(data_input == NULL){
		printf("Malloc error\n");
		exit(-1);
	}

	data_input->info.msg_type = DATA;
	data_input->info.len = sizeof(data) - sizeof(header);

	printf("Enter a valid destination username: ");
	fflush(stdout);
	fgets(data_input->dest_uname, U_SIZE, stdin);

	printf("Enter the object of the message (max 1024 characters): ");
	fgets(data_input->obj, O_SIZE, stdin);
	fflush(stdout);

	printf("Enter the text of the message (max 4096 characters): ");
	fgets(data_input->text, T_SIZE, stdin);
	fflush(stdout);

	printf("Msg dest_uname = %s", data_input->dest_uname);
	
	ret = send_msg(sock_ds, data_input);
	if(ret == -1){
		perror("Data sending failed. Client shuting down\n");
		exit(-1);
	}
	if(ret == -2){
		perror("Connection lost. Client shuting down\n");
		exit(-1);
	}

	//waiting server replay
	ret = read_from_socket(sock_ds, buffer, &msg_type);
	if(ret == 0){
		perror("Connection with server lost. Client shutingdown\n");
		exit(-1);
	}
	if(ret == -1){
		perror("Server currently unavailable\n");
		exit(-1);
	}

	if(msg_type == S_REPLAY){
		memcpy(&rep, buffer, sizeof(s_replay));
		if(rep.error_code == FDERROR){
			perror("Server could not perform the operation. Try again later\n");
			return;
		}
		if(rep.error_code == UNOTEXIST){
			perror("The specified destination username is not in the system. Try again with another destination user\n");
			return;
		}
		if(rep.error_code == EFDGENERIC){
			perror("Error on msg memorization\n");
			exit(-1);
		}
	}

	free(data_input);
	
	return;
}


