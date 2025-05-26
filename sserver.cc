#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
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

static int32_t recv_all(int connfd, char *buff, size_t bytes) {
    size_t bytes_left = bytes; 
    while (bytes_left != 0) {
        size_t bytes_read_sofar = bytes - bytes_left;
        ssize_t bytes_read = recv(connfd, buff + bytes_read_sofar, bytes_left, 0);
        if (bytes_read == -1) {
            perror("recv_all");
            return -1;
        }
        bytes_left -= bytes_read;
    }
    return 0;
}

static int32_t send_all(int connfd, char *msg, size_t bytes) {
    size_t bytes_left = bytes; 
    while (bytes_left != 0) {
        size_t bytes_read_sofar = bytes - bytes_left;
        ssize_t bytes_read = send(connfd, msg + bytes_read_sofar, bytes_left, 0);
        if (bytes_read == -1) {
            perror("send_all");
            return -1;
        }
        bytes_left -= bytes_read;
    }
    return 0;
}

int32_t reqres(int connfd) {
    const uint32_t max_msg_len = 4095;
    char msg_len_buff[4];
    errno = 0; // at the start of each function, we should set errno 0
    uint32_t read_res = recv_all(connfd, msg_len_buff, sizeof(uint32_t));
    if (read_res == -1) {
        fprintf(stderr, "recv_all: fail");
        return -1;
    }
    uint32_t msg_len;
    memcpy(&msg_len, msg_len_buff, sizeof(uint32_t));
    if (msg_len > max_msg_len) {
        fprintf(stderr, "message too long");
        return -1;
    }

    char client_msg[max_msg_len + 1];
    read_res = recv_all(connfd, client_msg, msg_len);
    if (read_res == -1) {
        fprintf(stderr, "recv_all: fail");
        return -1;
    }
    client_msg[msg_len] = '\0';
    printf("client says: %s\n", client_msg);
    
    char reply[] = "roger that!";
    size_t reply_len = sizeof(reply);
    char write_buff[reply_len + 4];
    memcpy(write_buff, &reply_len, 4);
    memcpy(&write_buff[4], reply, reply_len);
    return send_all(connfd, write_buff, sizeof(write_buff));
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

        while (true) {
            int32_t rv = reqres(connfd);
            if (rv == -1) {
                break;
            }
        }
        close(connfd);
    }
}
