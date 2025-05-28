#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <poll.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <vector>

const int server_back_log = 10;
const size_t max_msg_len = 32 << 20;

struct Conn {
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;

    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
};

static void sock_set_nonblock(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL); 
    if (errno != 0) {
        perror("fcntl(get flags)");
        abort();
    }

    flags |= O_NONBLOCK;
    errno = 0;
    fcntl(fd, F_SETFL, flags);
    if (errno != 0) {
        perror("fcntl(set flags)");
        abort();
    }
}

// append chunk to the back
static void buff_append(std::vector<uint8_t> &dst, uint8_t src[], size_t len) {
    dst.insert(dst.end(), src, src + len); 
}

// remove from the front
static void buff_consume(std::vector<uint8_t> &buff, size_t len) {
    buff.erase(buff.begin(), buff.begin() + len); 
}

// 	1. Try to parse accumulated buffer
// 	2. Process the parse message
//  3. if the message is already complete 
//      - echo is back to outgoing vector
// 	    - Remove the message from Conn::incoming
// 	4. if not, just left it like that
static bool try_one_request(Conn *conn) {
    if (conn->incoming.size() < 4) {
        return false;
    } 

    uint32_t msg_len;
    memcpy(&msg_len, conn->incoming.data(), 4);
    if (msg_len > max_msg_len) {
        fprintf(stderr, "msg too long\n");
        conn->want_close = true;
        return false;
    }
    // messaeg not ready
    if (msg_len > conn->incoming.size() - 4) {
        return false;
    }

    uint8_t *msg = &(conn->incoming[4]);
    printf("client says: len: %u | msg: %.*s\n", msg_len, 
           msg_len < 100 ? msg_len : 100, msg);
    // echo the message back to connection using outgoing buffer
    buff_append(conn->outgoing, (uint8_t *)&msg_len, 4);
    buff_append(conn->outgoing, msg, msg_len);

    buff_consume(conn->incoming, msg_len + 4);
    return true;
}

// just send what you can
// remove from the outgoing
// catch some error
static void handle_write(Conn *conn) {
    ssize_t bytes_sent = 
        send(conn->fd, conn->outgoing.data(), conn->outgoing.size(), 0);
    // in case client not ready (we use non-block send)
    if (bytes_sent <= 0 && errno == EAGAIN) {
        return;
    }
    // actual err
    if (bytes_sent <= 0) {
        conn->want_close = true;
        return; 
    }
    buff_consume(conn->outgoing, (size_t)bytes_sent);

    // switch back the state
    if (conn->outgoing.empty()) {
        conn->want_read = true;
        conn->want_write = false;
    }
}

// 1. Do non-blocking read
// 2. Add new data to Conn::incoming buffer
//  we need a buf_append(vec, src, len)
// 3. Use `try_one_request(conn)`
static void handle_read(Conn *conn) {
    uint8_t buff[64 * 1024];
    ssize_t bytes_read = recv(conn->fd, buff, sizeof(buff), 0);
    if (bytes_read <= 0) {
        conn->want_close = true;
        return;
    }
    
    buff_append(conn->incoming, buff, (size_t)bytes_read);

    while(try_one_request(conn));
    // switch back the state
    if (conn->outgoing.size() > 0) {
        conn->want_read = false;
        conn->want_write = true;

        // client should be ready to recv after sending requests
        // in chunk, therefore, we don't wait for next iteration
        // and just write right away.
        return handle_write(conn);
    }
}

// 1. accept
// 2. print out the ip addr of new conn
// 3. translate it to conn*
//  - we want to read from it
static Conn *handle_accept(int listenerfd) {
    struct sockaddr_in newsock;
    socklen_t newsock_len = sizeof(newsock);
    int connfd = accept(listenerfd, (struct sockaddr *)&newsock, &newsock_len); 
    if (connfd == -1) {
        perror("accept");
        return NULL;
    }

    char connip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(newsock.sin_addr.s_addr), connip, INET_ADDRSTRLEN) == NULL) {
        perror("inet_ntop");
        return NULL;
    }
    printf("new connection from %s\n", connip);

    sock_set_nonblock(connfd); // set to nonblocking mode

    Conn *new_conn = new Conn{};
    new_conn->fd = connfd;
    new_conn->want_read = true;
    return new_conn;
}

int main() {
    // fd is always small nat number. So just use array/vector is enough
    std::vector<Conn *> fdtoconn; 
    std::vector<struct pollfd> pfds;

    ////// prepare listener
    int listenerfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenerfd == -1) {
        perror("socket");
        exit(1);
    }
    
    int yes = 1;
    setsockopt(listenerfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(1234);
    serveraddr.sin_addr.s_addr = htonl(0);
    if (bind(listenerfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
        perror("bind");
        exit(1);
    }

    sock_set_nonblock(listenerfd);

    if (listen(listenerfd, server_back_log) == -1) {
        perror("listen");
        exit(1);
    }

    while (true) {
        ////// prepare pfds
        pfds.clear();
        struct pollfd listener = {listenerfd, POLLIN, 0};
        pfds.push_back(listener);
        for (Conn *conn : fdtoconn) {
            if (conn == NULL) continue;
            struct pollfd new_pollfd = { conn->fd, POLLERR, 0 };
            if (conn->want_read) {
                new_pollfd.events |= POLLIN;
            }
            if (conn->want_write) {
                new_pollfd.events |= POLLOUT;
            }
            if (conn->want_close) {
                new_pollfd.events |= POLLHUP;
            }
            pfds.push_back(new_pollfd);
        }

        ////// wait for readiness
        int num_events = poll(pfds.data(), (nfds_t)pfds.size(), -1);
        // don't care if process got interupting signal by OS.
        if (num_events < 0 && errno == EINTR) {
            continue;
        }
        if (num_events < 0) {
            perror("poll");
            exit(1);
        }

        ////// handle listener socket
        if (pfds[0].revents & POLLIN) {
            Conn *new_conn = handle_accept(listenerfd);
            if (new_conn != NULL) {
                // resize if too small
                if (fdtoconn.size() <= (size_t)new_conn->fd) {
                    fdtoconn.resize(new_conn->fd + 1);
                }
                fdtoconn[new_conn->fd] = new_conn;
            }
        }

        ////// handle connection socket
        for (size_t i = 1; i < pfds.size(); i++) {
            Conn *conn = fdtoconn[pfds[i].fd];
            struct pollfd pfd = pfds[i];
            int readiness = pfd.revents;
            if (conn->want_read && (readiness & POLLIN)) {
                handle_read(conn);
            }
            if (conn->want_write && (readiness & POLLOUT)) {
                handle_write(conn);
            }
            // why handle error after read/write? cuz handle function might
            // toggle want_close after it found some err
            if ((readiness & POLLERR) || conn->want_close) {
                // only deal with conn. pfds will be reset in each loop.
                close(conn->fd);
                fdtoconn[conn->fd] = NULL;
                delete conn;
            }
        }
    }
}
