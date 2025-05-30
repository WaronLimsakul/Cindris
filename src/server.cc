#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <poll.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <vector>
#include <string>
#include <map>

#include "buffer.h"
#include "hashtable.h"
#include "util.h"

const int server_back_log = 10;
const size_t max_msg_len = 32 << 20;
static Cache g_cache;

struct Conn {
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;

    Buffer incoming;
    Buffer outgoing;
};


struct Response {
    StatusCode status = RES_OK;
    uint8_t *data = NULL;
    size_t data_len = 0;
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

// 	- Have `int parse_req(req, output)` return 0, -1 if fail
// 	- so basically just parse each word into a vector 
static ssize_t parse_req(uint8_t *req, size_t buff_len,std::vector<std::string> &cmd) {
    // check num words
    assert(buff_len >= 4);
    uint32_t num_words;
    memcpy(&num_words, req, 4);

    cmd.clear();
    
    size_t bytes_read = 4; // already read the first 4 bytes
    uint32_t cur_word_len;
    size_t wc = 0;
    for (; wc < num_words; wc++) {
        // check bytes left > prefix len
        size_t bytes_left = buff_len - bytes_read;
        if (bytes_left < 4) break;

        // check bytes left > word len
        uint8_t *cursor = req + bytes_read;
        memcpy(&cur_word_len, cursor, 4);
        if (cur_word_len > bytes_left - 4) break;

        // push to cmd
        std::string cur_word = std::string((char *)(cursor + 4), cur_word_len);
        cmd.push_back(cur_word);

        bytes_read += cur_word_len + 4;
    }
    
    // check process complete
    if (wc != num_words) return -1;
    return bytes_read; // entire len of req
}

// - The key of this hash is we will have `offset_basis` and `prime`
// - for every byte in our string, `xor` it with `acc` , which start with `basis`
// - then `*=` with `prime` 
// - search google for `basis` and `prime` for FNV hash 
// - return `acc` (whihc is our hash)
static uint64_t str_hash(uint8_t *data, size_t len) {
    uint64_t hash = 0xcbf29ce484222325; // start with offset basis
    const uint64_t prime = 0x00000100000001b3;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i]; 
        hash *= prime;
    }
    return hash;
}

static void do_get(std::vector<std::string> &cmd, Response &res) {
    assert(cmd[0] == "get");
    // create dummy Entry for look up
    Entry target_dummy; 
    target_dummy.key.swap(cmd[1]); // assign use O(N), swap use O(1)
    target_dummy.node.hashval = str_hash(
        (uint8_t *)(target_dummy.key.data()), 
        target_dummy.key.size()
    );

    HNode *target_node = hm_lookup(&g_cache.map, &target_dummy.node, &entry_eq);
    if (!target_node) {
        res.status = RES_NOTFOUND;
        return;
    }

    // assign the value + status code to the response
    res.status = RES_OK;
    Entry *target_entry = container_of(target_node, Entry, node);
    std::string target_val = target_entry->value;
    assert(target_val.size() <= max_msg_len);
    res.data = (uint8_t *)(target_val.data());
    res.data_len = target_val.size();
}

static void do_set(std::vector<std::string> &cmd, Response &res) {
    // write a dummy for looking up
    Entry dummy;
    dummy.key.swap(cmd[1]);
    dummy.value.swap(cmd[2]);
    dummy.node.hashval = str_hash(
        (uint8_t *)dummy.key.data(),
        dummy.key.size()
    );

    HNode *target = hm_lookup(&g_cache.map, &dummy.node, &entry_eq);
    if (!target) {
        // insert new entry
        Entry *target_entry = new Entry(dummy);
        hm_insert(&g_cache.map, &(target_entry->node));
    } else {
        Entry *target_entry = container_of(target, Entry, node);
        target_entry->value.swap(dummy.value); // set value
    }
    res.status = RES_OK;
}

// plan
// 1. create dummy to lookup
// 2. hm_delete()
// 3. set RES_OK;
static void do_del(std::vector<std::string> &cmd, Response &res) {
    Entry dummy; 
    dummy.key.swap(cmd[1]);
    dummy.node.hashval = str_hash(
        (uint8_t *)dummy.key.data(), 
        dummy.key.size()
    );

    HNode *detached_node = hm_delete(&g_cache.map, &dummy.node, &entry_eq);
    if (detached_node != NULL) {
        res.status = RES_OK;
        Entry *detached_entry = container_of(detached_node, Entry, node);
        delete detached_entry; // free up allocated entry
    } else {
        res.status = RES_NOTFOUND;
    }
}

static void do_cmd(std::vector<std::string> cmd, Response &res) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        do_get(cmd, res);
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        do_set(cmd, res);
    } else if (cmd.size() == 2 && cmd[0]== "del") {
        do_del(cmd, res);
    } else {
        res.status = RES_ERR; // invalid command
        return;
    }
}

// - write response length first
// - then write statuscode
// - then write message
static void make_res(Buffer &output, Response res) {
    uint32_t msg_len = 4 + res.data_len;
    output.append((uint8_t *)&msg_len, 4);
    output.append((uint8_t *)&res.status, 4);
    output.append(res.data, res.data_len);
}

// 1. Parse the command to some struct <- we use `vector<string>`
// 2. Create response based on that struct
// 3. Append the response back to output buffer
static bool try_one_request(Conn *conn) {
    if (conn->incoming.size() < 4) {
        return false;
    } 
    uint8_t *req = conn->incoming.data();
    std::vector<std::string> cmd;
    size_t read_buff_size = conn->incoming.size();
    ssize_t req_len;
    if ((req_len = parse_req(req, read_buff_size, cmd)) <= 0) {
        return false;
    }
    Response res;
    do_cmd(cmd, res);
    make_res(conn->outgoing, res);
    conn->incoming.consume(req_len);
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
    conn->outgoing.consume((size_t)bytes_sent);

    // switch back the state
    if (conn->outgoing.size() == 0) {
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
    
    conn->incoming.append(buff, (size_t)bytes_read);

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
