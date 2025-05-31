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

// plan
// 1. init write_buff with k_max_msg len
// 2. write how many word to buff + update offset
// 3. have offset = 4
// 4. for each word in cmd
//  - write len of that word
//  - write that word
//  - update offset
// 5. now sendall(connfd, write_buff, offset)
static int32_t send_req(int connfd, std::vector<std::string> cmd) {
    uint8_t write_buff[k_max_msg];
    uint32_t cmd_size = (uint32_t)cmd.size();
    memcpy(write_buff, &cmd_size, 4); // assume little-endian
    size_t offset = 4;
    
    for (std::string word : cmd) {
        if (offset + 4 + word.size() > k_max_msg) return -1;
        uint32_t word_size = (uint32_t)word.size();
        memcpy(write_buff + offset, &word_size, 4);
        memcpy(write_buff + offset + 4, word.data(), word.size());
        offset += 4 + word.size();
    }
    return send_all(connfd, (char *)write_buff, offset);
}

// plan 
// 1. recv_all first
// 2. get m
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

    uint32_t status_code;
    memcpy(&status_code, server_msg, 4);
    switch (status_code) {
        case RES_OK:
            printf("status: ok");
            break;
        case RES_ERR:
            printf("status: error");
            break;
        case RES_NOTFOUND:
            printf("status: not found");
            break;
        default:
            printf("status: invalid status");
    }

    if (msg_len > 4) {
        server_msg[msg_len] = '\0';
        printf(" | message: %s\n", server_msg + 4);
    }
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
int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("please write command \n");
        return 1;
    }

    int connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1234);
    inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr.s_addr));

    std::vector<std::string> cmd;
    for (int i = 1; i < argc; i++) {
        cmd.push_back(std::string(argv[i]));
    }

    if (connect(connfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        goto DONE;
    }

    if (send_req(connfd, cmd) == -1) {
        goto DONE;
    }

    if (recv_res(connfd) == -1) {
        goto DONE;
    }

DONE:
    close(connfd);
    return 0;
}
