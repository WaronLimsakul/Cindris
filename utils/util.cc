#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <util.h>

int32_t recv_all(int connfd, char *buff, size_t bytes) {
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

int32_t send_all(int connfd, char *msg, size_t bytes) {
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
