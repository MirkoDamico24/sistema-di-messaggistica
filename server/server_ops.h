#include "types.h"

typedef struct _container{
	char **tokens;
	int n_entries;
}container;

key_t compute_sem_key(const char *uname);

int semaphore_operations(int *sem_ds, key_t key, int op);

int read_from_file(int fd, char *buffer, int len);

int write_on_file(int fd, char *buffer, int len);

int create_tokens(int type, container *ptr);

int uname_lookup(char *username, int *insert_pos);

int insert_user(char *username, char *password, int position);

void credentials_input(char *username, char*password);

int signin(char *username, char *password);

int login(char *username, char *password, int *state);

int write_msg_on_file(data *msg);

int deliver_msg(data *msg, char *src_username, s_replay *rep);

int read_msg(int sock_ds, char *uname, s_replay *rep, char **file_content, long *file_size, int *sem_ds);

int id_tokenize(char *buffer, long **ids, int *array_dim, int file_dim);

int delete_msg(char *file_mapping, char *uname, int n_ids, long *id);
