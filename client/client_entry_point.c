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
#include "sock_ops.h"

#define error(x) {printf("%s\n", x); exit(EXIT_FAILURE);}
#define fflush(stdin) {while(getchar() != '\n');}

int parse_command_line(int n_arg, char *line[], char **address)
{
	int ret, i;

	if(n_arg < 3){
		printf("Syntax error. Correcr usage: prog_name -a x.x.x.x\n");
		return -1;
	}
	i = 1;
	while(i<n_arg && (strcmp(line[i], "-a") != 0 && strcmp(line[i], "-A") != 0)){
		i++;
	}
	if(i<=n_arg) *address = line[i+1];
	else{
		printf("Missing the address in input\n");
		return -1;
	}

	return 0;
}

/*
 * The following function fills the msg for a login or
 * a sign-in.
 * Return 0 on success, -1 on failure*/
int aut(int sock_ds) 
{
	int op_type, ret, msg_type;
	authentication *user_data;
	char buffer[sizeof(s_replay)];
	s_replay rep;

	system("clear"); //cleaning the screen

	redo:
		printf("Which operation would you like to perform:\n1)Login (if you already have an account)\n2)Sign in (if you don't have an account\nMake a choice (1 or 2):");
		ret = scanf("%d", &op_type);
		fflush(stdin);
		if(ret == 0){
			error("Specified selection does not match with the provided choices");
			goto redo;
		}

	user_data = malloc(sizeof(authentication));
	if(user_data == NULL){ 
		error("Error on malloc (1)");
		return -1;
	}

	switch (op_type){
		case 1:	aut_input(user_data, LOG_MSG);
				break;

		case 2: aut_input(user_data, SIGN_MSG);
				break;

		default: printf("Input choice does not match with those provided");
				 return -1;
	}

	if( send_msg(sock_ds, user_data) == -1 ){
		error("Error on message sending");
		return -1;
	}

	//analyzing replay from server
	ret =  read_from_socket(sock_ds, buffer, &msg_type);
	if(ret == -1 || msg_type != S_REPLAY){
		printf("ret = %d\n", ret);
		perror("Something went wrong\n");
		exit(-1);
	}

	memcpy(&rep, buffer, sizeof(s_replay));
	if(rep.error == 1){
		if(op_type == 1){
			if(rep.error_code == NO_MATCH){
				printf("Wrong username or password\n");
				free(user_data);
				goto redo;
			}
		}
		else{
			if(op_type == 2){
				if(rep.error_code == EXALREADY){
					printf("Provided username already exist. Please enter anotherone\n");
					free(user_data);
					goto redo;
				}	
			}
		}
		if(rep.error_code == FDERROR){
			printf("Server couldn't complete the authentication. Client shuting down\n");
			exit(-1);
		}

	}
	printf("Authenticaion succesfull\n");

	free(user_data);
	return 0;
}

int main(int argc, char *argv[])		//takes the ip address of the server
{
	int sock_ds, ret, sel;
	char *s_addr, *msg_ptr;
	struct sockaddr_in client_addr, server_addr;

	ret = parse_command_line(argc, argv, &s_addr);
	if(ret == -1) error("Error on parsing the input line");

	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(S_PORT);
	
	ret = inet_aton(s_addr, &server_addr.sin_addr);
	if(ret == 0) error("The provided IP address is not valid");

	//connection to server
	sock_ds = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_ds == -1) error("Error on gettin socket");

	ret = connect(sock_ds, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr_in));
	if(ret == -1) error("Error on connecting to server");

	//authentication is a necessary operation
	ret = aut(sock_ds);
	if(ret == -1) error("Error occurred during authentication");

	while(1){
		//system("clear");

		printf("---OPERATION SELECTION MENU---\n");
		printf("Available operations:\n1)Read all messages\n2)Send a messege to a user\n3)Delete some messages\n4)Exit\n");
		ret = scanf("%d", &sel);
		fflush(stdin);
		if(ret == 0){
			printf("Wrong input type. Try again\n");
			fflush(stdin);
			continue;
		}

		switch (sel){
			case 1:	msg_ptr = malloc(sizeof(operation));
					if(msg_ptr == NULL) error("Error on malloc");
					request_read(sock_ds, (operation *)msg_ptr, READ);
					free(msg_ptr);
					break;
			
			case 2: data_input(sock_ds);
					break;

			case 3: msg_ptr = malloc(sizeof(operation));
					if(msg_ptr == NULL) error("Error on malloc");
					request_read(sock_ds, (operation *)msg_ptr, DELETE);
					break;
			
			case 4: exit(0);

			default: printf("The specified choice does not match with those provided (1-4). Please try again\n");
					 continue;
		}

	}
}
