#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include "sock_ops.h"
#include "client_types.h"

/*
 * The following function reads data from the socket, whose
 * descriptor is provided as first argument, and memorizes it 
 * into the provided buffer.
 * Returns 0 when a client is no longer connected to socket, -1 on failure,
 * and a nonzero value on success
*/

int read_from_socket(int sock_ds, char *buffer, int *msg_type)
{
	int ret, prev_r, res_r, msg_len;
	
	//peaking msg to read its len
	res_r = recv(sock_ds, buffer, sizeof(header), MSG_PEEK);
	if(res_r == -1){
		perror("Error on recv");
		return -1;
	}
	if(res_r == 0) return 0;

	memcpy(msg_type, buffer, sizeof(int));
	memcpy(&msg_len, buffer + sizeof(int), sizeof(int));

	/**msg_type = (header *)buffer->msg_type;
	msg_len = (header *)buffer->len;*/

	prev_r = 0;
	res_r = 0;
	msg_len += sizeof(header); //header still in the socket
	
	/*in this way we read the exact number of bytes of the message*/
	while(prev_r < msg_len){
		res_r = recv(sock_ds, &buffer[prev_r], msg_len - prev_r, 0);
		if(res_r == -1){
			perror("Error on recv");
			return -1;
		}
		prev_r += res_r;
	}

	return 1;
}

/*
 *The following function sends data in the given struct to the server,
 *where the socket to use is specified in the first argument.
 *Return 0 if succesfull, -1 on failure, -2 when tries to send data 
 *on a socket who has no receiver.
 *WARNING: this function should be called only on an already connected socket
*/
int send_msg(int sock_ds, void *msg_ptr)
{
	int ret, prev_w, res_w, len;
	char buffer[sizeof(data)];		//its dimension must be equale to the largest msg type
	
	//copyind data to send onto the buffer
	len = ((data *)msg_ptr)->info.len + sizeof(header);
	memcpy(buffer, msg_ptr, len);
	
	prev_w = 0;
	res_w = 0;
	while(prev_w < len){
		res_w = send(sock_ds, &buffer[prev_w], len - prev_w, 0);
		if(res_w == -1){
			if(errno == EPIPE) return -2;
			printf("Error occurred on writing on socket during msg sending\n");
			return -1;
		}
		prev_w += res_w;
	}

	return 0;
}


