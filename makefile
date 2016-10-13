all:
	gcc -w  -o  server  tftp_server.c
clean:
	rm -rf server
