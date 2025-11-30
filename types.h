#define LOG_MSG 1
#define SIGN_MSG 2
#define OPER 3
#define DATA 4
#define S_REPLAY 5
#define C_REPLAY 6
#define STREAM_END 7	//represents the msg end
#define DELETE_IDS 8 	//type used to erase messages

#define	U_SIZE 1025	//max size of username
#define O_SIZE 1025 //max size of object field
#define T_SIZE 4097 //max size of text section

/*
 * These macros define the opcodes
 * for the read and delete operations
 * of messages*/
#define READ 1
#define DELETE 2

#define S_PORT 1025

/*
 * follwing defines are for
 * server error codes
*/
#define NO_MATCH 1		//login failed because no match were found
#define EXALREADY 2		//registration failed because the username is already in the system
#define	UNOTEXIST 3		//dest_uname is not compatible with anyone of the registered users
#define EFDGENERIC 4	//an error occured whilst writing a msg on file
#define ENOMSGF 5		//user has no msg in his file or no files are allocated with his name
#define EMEMLEAK 6		//when mapping messages file on memory there is not enough space
#define EMEMGEN 7		//generic error mapping error
#define FDERROR 8		//genirc error on file opening


#define FUNAME "file_usernames"		//name of the file containing all usernames
#define FPWNAME "file_passwd"
						
typedef struct __attribute__((packed)) _head{
	int msg_type;
	int len;
}header;

typedef struct __attribute__((packed)) _aut{
	header info;
	char uname[U_SIZE];
	char passwd[U_SIZE];
}authentication;

typedef struct __attribute__((packed)) _oper{
	header info;
	int opcode;
}operation;

typedef struct __attribute__((packed)) _data{
	header info;
	char obj[O_SIZE];
	char dest_uname[U_SIZE];
	char src_uname[U_SIZE];
	char text[T_SIZE];
	int id;					//it identifies the msg in the file of alla messages for a user
}data;

typedef struct __attribute__((packed)) _srep{
	header info;
	char error;				//its value is 1 if an error occurred, 0 otherwise
	int error_code;
}s_replay;

typedef struct __attribute__((packed)) _crep{
	header info;
	char ack;		//client sets it to 0 when the messagge was not correctly received, otherwise it will be equal to 1
}c_replay;

typedef struct __attribute((packed)) _end{
	header info;
}msg_end;

/*
	* The following structure is used both from client and server.
	* The server uses it to request to the client to provide the
	* ids of the messages to erase.
	* The client uses the structure to provide the client the ids.
*/
typedef struct __attribute__((packed)) _del{
	header info;
	char msg_ids[O_SIZE];
}delete;
