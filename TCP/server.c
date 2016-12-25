/*
 *      C socket server example
 */

#include <stdio.h>
#include <string.h>             //strlen
#include <sys/socket.h>
#include <arpa/inet.h>          //inet_addr
#include <unistd.h>             //write

char shit[8192] = "blah\n\r\n\r";

int main(int argc, char *argv[])
{
	int socket_desc, client_sock, c, read_size;
	struct sockaddr_in server, client;
	char client_message[2000];

	//Create socket
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_desc == -1)
		printf("Could not create socket");
	puts("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(8888);

	//Bind
	if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	puts("bind done");

	//Listen
	listen(socket_desc, 3);

	//Accept and incoming connection
	puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);


	while (1) {
		printf("Waiting for a new client\n");
		//accept connection from an incoming client
		client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c);
		if (client_sock < 0) {
			perror("accept failed");
			return 1;
		}
		puts("Connection accepted");


		while (1) {
			int read_size = recv(client_sock, client_message, 2, 0);
			printf("%d bytes read\n", read_size);
			int wr_size = write(client_sock, shit, read_size);
			printf("%d bytes sent back!\n", wr_size);
			if (read_size == 0) {
				printf("Client disconnected, bailing out\n");
				break;
			close(client_sock);
			break;
			}
		}
	}

	return 0;
}
