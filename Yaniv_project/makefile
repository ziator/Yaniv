all: client server
client: yaniv_client.o
	gcc yaniv_client.o -o client
	rm *.o

server: yaniv_server.o
	gcc yaniv_server.o -o server -lpthread
	rm *.o

yaniv_client.o: yaniv_client.c
	gcc -c yaniv_client.c

yaniv_server.o: yaniv_server.c
	gcc -w -c yaniv_server.c -lpthread

clean:
	rm *.o client
	rm *.o server
