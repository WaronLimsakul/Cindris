#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define LISTEN_BACKLOG 10
#define READ_BUFF_MAXLEN 128

void do_something(int connfd) {
    char read_buff[READ_BUFF_MAXLEN];     
    int bytes_read = recv(connfd, read_buff, sizeof(read_buff) - 1, 0);
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("socket %d hung up\n", connfd);
        } else {
            perror("recv"); 
        }
        return;
    }
    read_buff[bytes_read] = '\0';
    printf("client: %s\n", read_buff);

    char write_buff[] = "hello, world";
    int bytes_sent = send(connfd, write_buff, strlen(write_buff), 0);
}

int main(void) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    // so we can bind the same port after old process terminated
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    // handroll our target destination
    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET; // not got sent, don't have to convert
    target_addr.sin_port = htons(1234); // use short for handroll sin_port
    target_addr.sin_addr.s_addr = htonl(0); // use long for handroll sin_addr (we want ::0)
    
    if (bind(sockfd, (struct sockaddr*)&target_addr, sizeof(target_addr)) == -1) {
        perror("bind");
        exit(1);
    }
    
    if (listen(sockfd, LISTEN_BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    while (true) {
        struct sockaddr_in remote_addr;
        socklen_t remote_len = sizeof(remote_addr);
        int connfd = accept(sockfd, (struct sockaddr *)&remote_addr, &remote_len);
        if (connfd == -1) {
            perror("accept");
            continue;
        }

        do_something(connfd);
        close(connfd);
    }
}
