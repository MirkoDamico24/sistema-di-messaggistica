#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include "server_ops.h"
#include "types.h"

#define error(x) {printf("%s\n", x); exit(EXIT_FAILURE);}
#define FILE_SIZE 1024     //max number of users registerd in the system. Defined to facilitate the tokenization of strings in the files

/*
* The following function computes the value of the sem key,
* which is made by the sum of the binary representation of
* the username of the user whose file has to be accessed.
* In the system usernames are unique and so will be the key.
*/
int compute_sem_key(const char *uname)
{
	int i, sum;

	i = 0;
	sum = 0;
	while(uname[i] != '\0' && uname[i] != '\n'){
		sum += uname[i];
		i++;
	}

	return sum;
}

/*
* The following function performs operations on the semaphore. 
* When the caller doesn't have the sem. descriptor, the first 
* argument has to be set to 0, so that the function can obtain it.
* Also, the function initializes the semaphore if it is the first
* call to require it. The operation to perform is specified 
* in the op argument. Return 0 on success, -1 on failure.
*/
int semaphore_operations(int *sem_ds, int key, int op)
{
	int des, ret;
	union semun{
		int val;
		struct semid_ds *buf;
		unsigned short *array;
	}arg;
	struct sembuf sops;

	printf("In semaphore operation\n");
	printf("Requested key = %d\n", key);

	if (*sem_ds == -1){
		des = semget(key, 1, IPC_CREAT | IPC_EXCL | 0660);		//if semaphore already created, no need to init it.
		if(des == -1){
			if(errno == EEXIST){
				des = semget(key, 1, IPC_CREAT | 0660);
				printf("Non Exclusive\n");
				if(des == -1) return -1;
			}
		}
		else{
			printf("exclusive\n");
			arg.val = 1;
			ret = semctl(des, 0, SETVAL, arg);		//semaphore has been created and it needs to be init
			if(ret == -1) {
				return -1;
			}
		}

		*sem_ds = des;
	}

	sops.sem_num = 0;
	sops.sem_op = op;
	sops.sem_flg = SEM_UNDO;
	ret = semop(*sem_ds, &sops, 1);
	if(ret == -1) return -1;
	
	return 0;
}

/*
 * The following function reads len bytes from the file
 * whose descriptor is fd. When all bytes have been read,
 * it return 1, -1 if an error occurred, 0 when no bytes 
 * available on file.
*/
int read_from_file(int fd, char *buffer, int len)
{
	int res_r, prev_r;
	
	prev_r = 0;
	res_r = 0;
	while(prev_r < len){
		res_r = read(fd, (buffer + prev_r), len - prev_r);
		if(res_r == -1){
			return -1;
		}
		if(res_r == 0) return 0;
		
		prev_r += res_r;
	}

	return 1;
}

/*
 * The following function writes len bytes on the file,
 * whose descriptor is fd. Returns -1 if any error occurs, 
 * 0 if no bytes are wrote, 1 on success.
*/ 
int write_on_file(int fd, char *buffer, int len)
{
	int prev_w, res_w;

	prev_w = 0;
	res_w = 0;
	while(prev_w < len){
		res_w = write(fd, (buffer + prev_w), len - prev_w);
		if(res_w == -1) return -1;
		if(res_w == 0) return 0;

		prev_w += res_w;
	}

	return 1;
}

/*
 * The following function takes the content of a file and extracts the single lines in it. 
 * Returns 0 when the file is not empty, -1 when the file has no content Returns -2 when an error
 * occures while opening file. Moreover, it
 * produces side-effect on the pointer in the second argument, which will point at the
 * area containing the lines of the file*/
int create_tokens(int type, container *ptr)  //returns 0 when file is not empty, -1 when file is empty and no tokens found
{
	int i, len, fd;
	char *file_content, *tk;

	if(type == 1){		//type argument describes if the function is called on passwd file or username file
		fd = open(FUNAME, O_CREAT | O_RDWR, 0660);
	}
	else{
		fd = open(FPWNAME, O_CREAT | O_RDWR, 0660);
	}
	if(fd == -1) return -2;	

	len = lseek(fd, 0, SEEK_END);
	if(len == 0) return -1; //empty file

	file_content = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0); 
	if(file_content == NULL) error("Error on mmap");

	ptr->tokens = malloc(FILE_SIZE*sizeof(char *));
	if(ptr->tokens == NULL) error("Error on malloc");

	i = 0;	
	tk = strtok(file_content, "\n");		//strdup necessaria perché sennò aggiungendo i caratteri speciali sovrascrivo altre stringhe
	while(tk != NULL){
		(ptr->tokens)[i] = strdup(tk);
		len = strlen((ptr->tokens)[i]);
		((ptr->tokens)[i])[len] = '\n';
		((ptr->tokens)[i])[len + 1] = '\0';
		i++;
		tk = strtok(NULL, "\n");
	}
	ptr->n_entries = i;

	if(munmap(file_content, len) == -1) error("Error occurred on munmap");

	close(fd);
	
	return 0;
}

/*
 * The following function determins where a new username has to be
 * positioned into the usernames file. It performs a binary research onto
 * the content of the file, which is copied onto a region of memory.
 * Returns 0 if the username is NOT found in the file, 1 in case the uname is found,
 * -1 if an error on create_tokens occurs.
*/
int uname_lookup(char *username, int *insert_pos)  //return 0 if uname not found, 1 if it founds the uname
{
	int len, i, j, m, ret;
	char found=0;
	container uname_t;  //struct with usernames token

	ret = create_tokens(1, &uname_t); //creating readable strings
	if(ret == -1){
		*insert_pos = 0;	//file is empty so insertion at first position
		return 0;
	}
	if(ret == -2){
		return -1;
	} 
	if(uname_t.n_entries == FILE_SIZE){
		printf("Maximum number of registered account reached. Registration failed\n");
		//TODO :exit(EXIT_FAILURE);
	}
	
	//non-recursive binary search
	i = 0;
	j = uname_t.n_entries-1;

	while(i<=j){
		m = (i+j)/2;
		ret = strcmp(username, (uname_t.tokens)[m]);
		if(ret == 0){
			found = 1;
			break;
		}
		if(ret < 0){
			j = m - 1;
		}
		else i = m + 1;		
	}
	
	if(found == 1){
		*insert_pos = m;
		free(uname_t.tokens);
		return 1;
	}
	else{
		*insert_pos = i;		//position in which will be inserted the username in file
		free(uname_t.tokens);
		return 0;
	} 
}

/*
 * The following function insert the username and the password in the
 * appropriate file. Moreover, the storing is made keeping an alphabetical order
*/
int insert_user(char *username, char *password, int position)
{
	char *file_content;
	container passwd_token, uname_t;
	int len, n_tokens_u, n_tokens_p, i, j, ret, uname, passwd;

	//orederly insertion
	n_tokens_u = create_tokens(1, &uname_t);
	if(n_tokens_u == -2) return -1;
	n_tokens_p = create_tokens(2, &passwd_token);
	if(n_tokens_p == -2) return -1;

	uname = open(FUNAME, O_RDWR, 0660);
	if(uname == -1) return -1;

	passwd = open(FPWNAME, O_RDWR, 0660);
	if(passwd == -1) return -1;

	if(n_tokens_u == -1 && n_tokens_p == -1){
		ret = write_on_file(uname, username, strlen(username));
		if(ret == -1) return -1;

		ret = write_on_file(passwd, password, strlen(password));
		if(ret == -1) return -1;
	}
	else{
		if(n_tokens_u != n_tokens_p){
			//TODO: gestisci disallineamento tra file degli username e quello delle password
		}
		else{
			i = 0;
			j = 0;
			while(j<passwd_token.n_entries+1){
				if(j == position){
					ret = write_on_file(uname, username, strlen(username));
					if(ret == -1) return -1;

					ret = write_on_file(passwd, password, strlen(password));
					if(ret == -1) return -1;
				}
				else{
					ret = write_on_file(uname, (uname_t.tokens)[i], strlen((uname_t.tokens)[i]));
					if(ret == -1) return -1;

					ret = write_on_file(passwd, (passwd_token.tokens)[i], strlen((passwd_token.tokens)[i]));
					if(ret == -1) return -1;
					i++;
				}
				j++;
			}
		}
	}
		
	close(uname);
	close(passwd);
	return 0;
}

/*
 * The following function performs the operations needed to register a user. It also creates the file
 * in which all the msg for the user will be stored.
 * Returns 0 on success, -1 on failure, -2 to singal a generic fd error.
*/
int signin(char *username, char *password)	
{
	int len, position, fd, uname_ds, passwd_ds, ret;
	int key_uname, key_passwd;
	char file_name[U_SIZE];
	
	//acquiring semaphore to work on file
	uname_ds = -1;
	key_uname= compute_sem_key(FUNAME);
	semaphore_operations(&uname_ds, key_uname, -1);
	
	//check if username already exists
	ret = uname_lookup(username, &position);
	if(ret == 1 || ret == -1){
		printf("The inserted username already exists. Sign-in falied\n");
		goto signin_error_end_1;
	}

	
	//acquiring semaphore to work on passwords file
	passwd_ds = -1;
	key_passwd = compute_sem_key(FPWNAME);
	semaphore_operations(&passwd_ds, key_passwd, -1);

	//inserting new user in file
	ret = insert_user(username, password, position);
	if(ret == -1) goto signin_error_end_2;

	//creating user file in which memorize user's messagges
	len = strlen(username);
	username[len-1] = '\0';			//eliminating the \n character
	sprintf(file_name, "%s%s%s","../server/user_files/", username, ".txt");
	printf("Path name to create = %s\n", file_name);

	fd = open(file_name, O_CREAT | O_EXCL | O_RDONLY, 0660);
	if(fd == -1){
		if(errno == EEXIST){
			printf("Username already exists. Something's gone wrong");
			goto signin_error_end_2;
		}
	}

	printf("Registration succesfully completed\n");
	
	//freeing the critical section
	semaphore_operations(&uname_ds, 0, 1);
	semaphore_operations(&passwd_ds, 0, 1);

	close(fd);
	return 0;

signin_error_end_2:
	semaphore_operations(&passwd_ds, 0, 1);
signin_error_end_1:
	semaphore_operations(&uname_ds, 0, 1);
	close(fd);
	if(ret == -1) return -2;
	return -1;
}


/*
 * The following function check the provided information to perform
 * the login.
 * On failure return -1 and provieds the state of the function. 
 * When it fails because there is no match between the provided username and those
 * registred, state is set to 1. Otherwise, it is set to 0.
 * On success, the value of state is a d.c.c
*/
int login(char *username, char *password, int *state)  
{
	int pos, n_tokens, uname_ds, passwd_ds, ret,len;
	int key_uname, key_passwd;
	container passwords;

	ret = 3; 		//initialized with a value that it would never have if used as return from a function
	passwords.tokens = NULL;

	//acquiring semaphore to work on usernames file
	uname_ds = -1;
	key_uname = compute_sem_key(FUNAME);
	semaphore_operations(&uname_ds, key_uname, -1);

	ret = uname_lookup(username, &pos);
	if(ret == 0 || ret == -1){
		printf("The specified username is not in the system\n");
		*state = 1;
		goto login_error_end_1;
	}

	//acquiring semaphore to work on passwords file
	passwd_ds = -1;
	key_passwd = compute_sem_key(FPWNAME);
	semaphore_operations(&passwd_ds, key_passwd, -1);

	//removing \n at the end of username
	len = strlen(username);
	(username)[len -1] = '\0';

	//password control
	n_tokens = create_tokens(2, &passwords);
	if(n_tokens == -1 || n_tokens == -2){
		printf("Error occurred on password checking\n");
		*state = 0;
		//TODO: se siamo qui e controllo su uname_lookup è andato a buon fine, potrebbe esserci
		//disallineamento tra file username e quello delle password. Fai controllo sopra per sapere
		//se lo username non è stato trovato perché non esiste, o perché il file degli username è vuoto
		goto login_error_end_2;
	}

	if(strcmp(password, (passwords.tokens)[pos]) != 0){
		printf("Password incorrect. Please try again\n");
		*state = 1;
		goto login_error_end_2;
	}

	semaphore_operations(&uname_ds, 0, 1);
	semaphore_operations(&passwd_ds, 0, 1);

	free(passwords.tokens);

	return 0;

login_error_end_2:
	semaphore_operations(&passwd_ds, 0, 1);
login_error_end_1:
	semaphore_operations(&uname_ds, 0, 1);
	if(passwords.tokens != NULL)
		free(passwords.tokens);
	if(ret == -1) return -2;
	return -1;
}

/*The following function archives msg on the 
 * recipient user's file.
 * Return 0 on success, -1 on failure
*/
int write_msg_on_file(data *msg)
{
	int fd, ret, prev_w, res_w, len, fd_ind, msg_id, sem_ds;
	long curr_dim;
	int sem_key;
	char filename[128],filename_ind[128], buffer[sizeof(data)];

	len = strlen(msg->dest_uname);
	(msg->dest_uname)[len -1] = '\0';
	
	sprintf(filename, "%s%s%s","../server/user_files/", msg->dest_uname, ".txt");
	sprintf(filename_ind, "%s%s%s", "../server/user_files/", msg->dest_uname, "_dim.txt");

	printf("MSg filename = %s\n", filename);
	printf("Index file = %s\n", filename_ind);

	sem_ds = -1;
	sem_key = compute_sem_key(msg->dest_uname);
	semaphore_operations(&sem_ds, sem_key, -1);

	fd = open(filename, O_CREAT | O_RDWR, 0660);
	if(fd == -1){
		printf("Errno = %s\n", strerror(errno));
		goto end_error_1;
	}

	fd_ind = open(filename_ind, O_CREAT | O_RDWR, 0660);			//contains the last valid byte in msg file
	if(fd_ind == -1){
		printf("Error while opening index file = %s\n", strerror(errno));
		goto end_error_2;
	}

	ret = read_from_file(fd_ind, buffer, sizeof(long));
	if(ret == -1) goto end_error_3;
	if(ret == 0) curr_dim = 0;   //index file is currently empty
	else{
		curr_dim = *(long *)buffer;
	}
	lseek(fd, curr_dim, SEEK_SET);	//write not in append because of msg erasure
	
	//assignin the correct id to the msg
	if(curr_dim == 0) msg_id = 0;
	else{
		lseek(fd, curr_dim - sizeof(data), SEEK_SET);		//reading the last valid msg in file
		ret = read_from_file(fd, buffer, sizeof(data));
		if(ret == -1){
			goto end_error_3;
		}
		
		msg_id = *(int *)(buffer + sizeof(header) + O_SIZE + 2*U_SIZE + T_SIZE);		//reading the last valid id in msg file
	}
	msg->id = msg_id + 1;

	len = sizeof(*msg);
	memcpy(buffer, msg, len);
	retry:
		ret = write_on_file(fd, buffer, len);
		if(ret == -1) goto end_error_3;
		if(ret == 0) goto retry;

	//updating current file dim on index
	curr_dim = lseek(fd, 0, SEEK_CUR);
	printf("Current file dim = %ld\n", curr_dim);
	lseek(fd_ind, 0, SEEK_SET);		//always override previous dim
	ret = write_on_file(fd_ind, (char *)&curr_dim, sizeof(long));

	semaphore_operations(&sem_ds, 0, 1);

	close(fd);
	close(fd_ind);
	return 0;

end_error_3:
	close(fd_ind);
end_error_2:
	close(fd);
end_error_1:
	semaphore_operations(&sem_ds, 0, 1);
	return -1;
}

/*The following function inserts the message st by 
 * src_username into the destination user's file. It also
 * fills the s_replay struct for the client.
 * Return 0 on success, -1 on failure*/
int deliver_msg(data *msg, char *src_username, s_replay *rep)
{
	int ret, pos, sem_ds;
	int sem_key;
	char dest_uname[U_SIZE];

	sem_ds = -1;
	sem_key = compute_sem_key(FUNAME);		//semaphore to access to usernames file
	semaphore_operations(&sem_ds, sem_key, -1);

	printf("In deliver_msg\n");
	//checking if the recipiet user exists
	printf("Destuname received = %s", msg->dest_uname);
	ret = uname_lookup(msg->dest_uname, &pos);
	if(ret == 0 || ret == -1){
		//username not found, msg cannot be delivered
		semaphore_operations(&sem_ds, 0, 1);
		rep->error = 1;
		if(ret == -1) rep->error_code = FDERROR;
		else rep->error_code = UNOTEXIST;
		return -1;
	}

	semaphore_operations(&sem_ds, 0, 1);

	memcpy(msg->src_uname, src_username, strlen(src_username));
	printf("The inserted src_uname is %s", msg->src_uname);
	ret = write_msg_on_file(msg);
	if(ret == -1){
		rep->error = 1;
		rep->error_code = EFDGENERIC;
		return -1;
	}
	
	rep->error = 0;
	return 0;
}

/*
 * The following function maps the messages in the user's file in
 * memory and returns the address of the mapping in the
 * file_content argument, whilst in the file_size argument is 
 * returned the size of the mapping.
 * Returns 0 on success, -1 on failure*/
int read_msg(int sock_ds, char *uname, s_replay *rep, char **file_content, long *file_size, int *sem_ds)
{
	int ret, fd, prev_w, res_w, fd_ind, curr_dim;
	char buffer[sizeof(data)], filename[128], filename_ind[128];
	int sem_key;
	off_t size;

	sprintf(filename, "%s%s%s", "../server/user_files/", uname, ".txt");
	sprintf(filename_ind, "%s%s%s", "../server/user_files/", uname, "_dim.txt");

	//prendere semaforo per file utente, calcolare prima la key N.B: non è necessario semaforo per file indice, perché viene aperto sempre insieme al file dei messaggi. Ritorna sem_ds in parametro
	sem_key = compute_sem_key(uname);
	*sem_ds = -1;
	semaphore_operations(sem_ds, sem_key, -1);

	fd = open(filename, O_RDWR, 0660);
	if(fd == -1){
		rep->error = 1;
		if(errno == ENOENT) rep->error_code = ENOMSGF; 
		else rep->error_code = EFDGENERIC;
		goto bad_end_1;
	}

	fd_ind = open(filename_ind, O_RDONLY, 0660);
	if(fd_ind == -1){
		rep->error = 1;
		if(errno == ENOENT) rep->error_code = ENOMSGF;
		else rep->error_code = EFDGENERIC;
		goto bad_end_2;
	}
	
	//mapping file content in memory
	read_again:
		ret = read_from_file(fd_ind, buffer, sizeof(long));
		if(ret == -1){
			rep->error = 1;
			rep->error_code = EFDGENERIC;
			goto bad_end_3;
		}
		if(ret == 0){
			lseek(fd_ind, 0, SEEK_SET); 	//EOF -> redo op starting from the beginning of file
			goto read_again;
		}
	
	*file_size = *(long *)buffer;
	if(*file_size == 0){			//no msg in file
		printf("Size = 0\n");
		rep->error = 1;
		rep->error_code = ENOMSGF;
		goto bad_end_3;
	}

	printf("file_size = %ld\n", *file_size);

	*file_content = mmap(NULL, *file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if(file_content == NULL){
		rep->error = 1;
		if(errno == EAGAIN) rep->error_code = EMEMLEAK;
		else rep->error_code = EMEMGEN;
		goto bad_end_3;
	}
	
	return 0;

bad_end_3:
	close(fd_ind);
bad_end_2:
	close(fd);
bad_end_1:
	semaphore_operations(sem_ds, 0, 1);
	return -1;
}

/*
* The following function creates an array
* of msg_id from the provided stream. Return
* 0 on success, -1 on failure
*/
int id_tokenize(char *buffer, long **ids, int *array_dim, int file_dim)
{
	int i, dim, len;
	char **id_tok;

	if(strcmp(buffer, "all\n") == 0){
		*array_dim = 0;
		return 0;
	}

	i = 0;
	dim = (file_dim)/sizeof(data);		//gives the number of msg in file
	id_tok = malloc(dim*sizeof(long));			//max dim of array is equal to # of msg in file -1, otherwise "all" is specified
	if(id_tok == NULL) return -1;

	id_tok[i] = strtok(buffer, " ");
	while(id_tok[i] != NULL){
		i++;
		id_tok[i] = strtok(NULL, " ");
	}
	if(i == 0) return -1;

	len = strlen(id_tok[i-1]);
	(id_tok[i-1])[len - 1] = '\0'; 	//removing the "\n" at the end of the stream

	*ids = malloc(i*sizeof(long));
	if(*ids == NULL) return -1;

	for(int k = 0; k<i; k++){
		(*ids)[k] = strtol(id_tok[k], NULL, 10);
		printf("k = %d\n", k);
	}

	*array_dim = i;

	free(id_tok);
	return 0;
}

/*
* The following function erases the specified msg. The deletion is realized
* in O(n), where n is the number of messages on file. The function also 
* writes back msg that must not be erased on file. Returns 0 on success, -1 if error 
* occurres during file opening, -2 on failure.
*/
int delete_msg(char *file_mapping, char *uname, int n_ids, long *id)
{
	int fd, fd_ind, ret, curr_id, i;
	int id_addr = sizeof(header) + O_SIZE + 2*U_SIZE + T_SIZE;		//offset to read msg_id in the struct loaded from file
	long curr_dim;
	char filename[128], filename_ind[128], *state, buffer[sizeof(long)];

	sprintf(filename, "%s%s%s", "../server/user_files/", uname, ".txt");
	sprintf(filename_ind, "%s%s%s", "../server/user_files/", uname, "_dim.txt");

	fd = open(filename, O_WRONLY, 0660);
	if(fd == -1) return -1;

	fd_ind = open(filename_ind, O_RDWR, 0660);
	if(fd_ind == -1) return -1;

	ret = read_from_file(fd_ind, (char *)(&curr_dim), sizeof(long));
	if(ret == -1) return -2;
	if(ret == 0){
		curr_dim = lseek(fd, 0, SEEK_END);			//index file empty, so current dim is taken
		lseek(fd, 0, SEEK_SET);
	}

	if(n_ids == 0){				//all msg have to be erased
		curr_dim = 0;
	}
	else{
		state = file_mapping;
		curr_id = 1;
		i = 0;

		printf("n_ids = %d\n", n_ids);

		/* Cicle repetaed untile the value of the first bytes of state are equal to 0
		   meaning that msg_type = 0, and that happens only if reading invalid memory locations.
		   The number of bytes currently written of the msg file must be lower than the current dim
		   minus the number of currently erased bytes.
		*/
		while( (*(int *)state != 0) && (lseek(fd, 0, SEEK_CUR) < (curr_dim - i*sizeof(data))) ){			//i*sizeof(data) = #msg erased
			printf("id[%d] = %d\n", i, id[i]);
			if( (i < n_ids) && (*(int *)(state + id_addr) == id[i]) ) {
				printf("In if\n");
				++i;
				state += sizeof(data);
				continue;
			}
			if( *(int *)(state + id_addr) > curr_id ) {*(int *)(state + id_addr) -= i;}	//decrements the id of the number of previosly erased msg
			curr_id++;

			ret = write_on_file(fd, state, sizeof(data));
			if(ret == -1) return -2;
			if(ret == 0) continue; 		//retry the writing

			state += sizeof(data);
		}		
		
		curr_dim = lseek(fd, 0, SEEK_CUR);
	}	

	redo_write:
		lseek(fd_ind, 0, SEEK_SET);		//always override previous dim
		ret = write_on_file(fd_ind, (char *)(&curr_dim), sizeof(long));
		if(ret == 0) goto redo_write;
		if(ret == -1) return -2;

	close(fd);
	close(fd_ind);
	return 0;
}