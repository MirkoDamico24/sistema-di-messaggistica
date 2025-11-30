SERVER = server

sign:
	gcc -o server_ops.o ../libs/server_ops.c -c
	gcc -o sock_ops.o ../libs/sock_ops.c -c

server: sign
	gcc -o server_entry_point.o server_entry_point.c -c
	gcc -o $(SERVER) server_entry_point.o server_ops.o sock_ops.o

clear: 
	rm -f *.o
	rm $(SERVER)
