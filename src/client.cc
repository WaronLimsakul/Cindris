#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "util.h"

// static void fd_set_nonblock(int fd) {
//     errno = 0;
//     int fdflags = fcntl(fd, F_GETFL); 
//     fdflags |= O_NONBLOCK;
//     fcntl(fd, F_SETFL, fdflags);
//     if (errno) {
//         perror("fd_set_nonblock");
//     }
// }

// 10. send the `text` to the server
// 11. get the reply back
// 12. print out reply message
int32_t query(int connfd, char *text) {
    size_t text_len = strlen(text);
    char write_buff[text_len + 4];
    memcpy(write_buff, (uint32_t *)&text_len, 4);
    memcpy(&write_buff[4], text, text_len);
    if (send_all(connfd, write_buff, sizeof(write_buff)) == -1) {
        fprintf(stderr, "send_all fail\n");
        return -1;
    }

    char msg_len_buff[4];
    if (recv_all(connfd, msg_len_buff, 4) == -1) {
        fprintf(stderr, "recv_all fail\n");
        return -1;
    }
    uint32_t msg_len;
    memcpy(&msg_len, msg_len_buff, 4);
    const uint32_t max_msg_len = 4095;
    if (msg_len > max_msg_len) {
        fprintf(stderr, "message's too long\n");
        return -1;
    }
    char server_msg[max_msg_len];
    if (recv_all(connfd, server_msg, msg_len) == -1) {
        fprintf(stderr, "recv_all fail\n");
        return -1;
    }
    server_msg[msg_len] = '\0';
    printf("server: %s\n", server_msg);
    return 0;
}

// 13. Create a socket
// 14. Get connection
// 15. query something like `"hello number 1"`
// 16. query something like `"hello number 2"`
// 17. Clean up by `goto CLEANUP`
// 	- It should close connection and return 0
int main(void) {
    int connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1234);
    inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr.s_addr));

    if (connect(connfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        goto DONE;
    }

    if (query(connfd, "hello number 1") == -1) {
        goto DONE;
    }
    
    if (query(connfd, "hello number 2") == -1) {
        goto DONE;
    }

DONE:
    close(connfd);
    return 0;
}
