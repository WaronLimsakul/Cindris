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

#include <vector>
#include <string>

#include "util.h"

const size_t k_max_msg = 32 << 20;

static int32_t send_req(int connfd, char *text, size_t text_len) {
    char *write_buff = new char[text_len + 4];
    memcpy(write_buff, (uint32_t *)&text_len, 4);
    memcpy(&write_buff[4], text, text_len);
    if (send_all(connfd, write_buff, text_len + 4) == -1) {
        fprintf(stderr, "send_all fail\n");
        delete[] write_buff;
        return -1;
    }
    delete[] write_buff;
    return 0;
}

static int32_t recv_res(int connfd) {
    char msg_len_buff[4];
    if (recv_all(connfd, msg_len_buff, 4) == -1) {
        fprintf(stderr, "recv_all fail\n");
        return -1;
    }
    uint32_t msg_len;
    memcpy(&msg_len, msg_len_buff, 4);
    if (msg_len > k_max_msg) {
        fprintf(stderr, "message's too long\n");
        return -1;
    }

    char *server_msg = new char[msg_len + 1];
    if (recv_all(connfd, server_msg, msg_len) == -1) {
        fprintf(stderr, "recv_all fail\n");
        delete[] server_msg;
        return -1;
    }
    server_msg[msg_len] = '\0';
    printf("server: %s\n", server_msg);
    delete[] server_msg;
    return 0;
}

// 10. send the `text` to the server
// 11. get the reply back
// 12. print out reply message
// static int32_t query(int connfd, std::string text) {
//     if (send_req(connfd, text.data(), text.size()) == -1) {
//         return -1;
//     }
//     return recv_res(connfd);
// }

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

    std::vector<std::string> query_list = {
        "hello no.1", "hello no.2", "hello no.3",
        std::string(k_max_msg, 'x'),
        "hello no.5",
    };

    if (connect(connfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        goto DONE;
    }

    for (std::string query : query_list) {
        if (send_req(connfd, query.data(), query.size()) == -1) {
            goto DONE;
        }
    }
    for (size_t i = 0; i < query_list.size(); i++) {
        if (recv_res(connfd) == -1) {
            goto DONE;
        }
    }

DONE:
    close(connfd);
    return 0;
}
