#include "client_types.h"

void aut_input(authentication *aut_ptr, int type);

void request_read(int sock_ds, operation *oper, int op_type);

void data_input(int sock_ds);

void oper_input(operation *msg_ptr, int type);

int send_msg(int sock_ds, void *msg_ptr);
