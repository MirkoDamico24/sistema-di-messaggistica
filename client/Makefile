CLIENT = client

lib:
	gcc -o client_ops.o client_ops.c -c
	gcc -o sock_ops.o sock_ops.c -c

client:  lib
	gcc -o client_entry_point.o client_entry_point.c -c
	gcc -pthread -o $(CLIENT) client_entry_point.o client_ops.o sock_ops.o

clear:
	rm -f *.o
	rm -f $(CLIENT)
