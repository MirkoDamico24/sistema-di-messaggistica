SERVER = server
USER_FILES = user_files

$(USER_FILES):
	mkdir $(USER_FILES)

sign:
	gcc -o server_ops.o server_ops.c -c
	gcc -o sock_ops.o sock_ops.c -c

server: sign | $(USER_FILES)
	gcc -o server_entry_point.o server_entry_point.c -c
	gcc -pthread -o $(SERVER) server_entry_point.o server_ops.o sock_ops.o

clear: 
	rm -f *.o
	rm $(SERVER)
