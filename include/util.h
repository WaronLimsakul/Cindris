#pragma once

int32_t recv_all(int connfd, char *buff, size_t bytes);
int32_t send_all(int connfd, char *msg, size_t bytes);

