#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(void) {
    struct sockaddr_in server_addr; 
    inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr));
    server_addr.sin_port = htons(1234); 
    server_addr.sin_family = AF_INET;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        return 1;
    }

    char msg[] = "hello, server";
    if (send(sockfd, msg, sizeof(msg), 0) == -1) {
        perror("send");
        return 1;
    }

    char read_buff[100];
    int bytes_recv;
    if ((bytes_recv = recv(sockfd, read_buff, 100, 0)) == -1) {
        perror("recv");
        return 1;
    }
    read_buff[bytes_recv] = '\0';
    printf("server: %s", read_buff);
}
