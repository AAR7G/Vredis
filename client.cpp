#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <vector>
#include <string>

//static int32_t query(int fd, const char *text);
//static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len);

static void die(const char *msg);
static void msg(const char *msg);
static int32_t read_full(int fd, char *rbuf, size_t n);
static int32_t write_all(int fd, const char *wbuf, size_t n);
static int32_t send_req(int fd, const std::vector<std::string> &cmd);
static int32_t read_res(int fd);
static int32_t print_response(const uint8_t *data, size_t size);

const size_t k_max_msg = 4096;              

int main(int argc, char **argv) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);          //127.0.0.1

    //connect
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if(rv < 0) {
        die("connect()");
    }

    /*//multiple pipelined requests/queries
    std::vector<std::string> query_list = {
        "hello_1", "hello_2", "hello_3",
        std::string(k_max_msg, 'a'),        //large messages will require multiple event loop iterations
        "hello_4"
    };

    for(const std::string &s : query_list) {
        int32_t rv = send_req(fd, (uint8_t *)s.data(), s.size());
        if(rv < 0) {
            goto L_DONE;
        }
    }

    for(size_t i = 0; i < query_list.size(); ++i) {
        int32_t rv = read_res(fd);
        if(rv < 0) {
            goto L_DONE;
        }
    }*/

    std::vector<std::string> cmd;
    for(int i = 1; i < argc; i++) {
        cmd.push_back(argv[i]);
    }

    int32_t err = send_req(fd, cmd);
    if(err < 0) {
        goto L_DONE;
    }

    err = read_res(fd);
    if(err < 0) {
        goto L_DONE;
    }
    
    L_DONE:
        close(fd);
        return 0;
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static int32_t read_full(int fd, char *rbuf, size_t n) {
    while(n > 0) {
        ssize_t rv = read(fd, rbuf, n);
        if(rv <= 0) {
            return -1;              //Error or buffer empty (-1), end of file or connection end (0)
        }

        assert((size_t)rv <= n);
        n -= (size_t)rv;            //n is the amount of free space in the buffer
        rbuf += rv;
    }

    return 0;
}

static int32_t write_all(int fd, const char *wbuf, size_t n) {
    while(n > 0) {
        ssize_t rv = write(fd, wbuf, n);
        if(rv <= 0) {
            return -1;
        }

        assert((size_t)rv <= n);
        n -= (size_t)rv;            //n is the amount of bytes left to read
        wbuf += rv;
    }

    return 0;
}

//Not used anymore;
//It has been split and implemented withing 'send_req' and 'read_res'
/*static int32_t query(int fd, const char *text) {
    int32_t len = (int32_t)strlen(text);
    if(len > k_max_msg) {
        return -1;              
    }

    //send request
    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);              //Assume little endian
    memcpy(&wbuf[4], text, len);
    int32_t err = write_all(fd, wbuf, len + 4);
    if(err) {
        return err;
    }

    //4 bytes header
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    err = read_full(fd, rbuf, 4);
    if(err) {
        msg(errno == 0 ? "End of line" : errno == EINTR ? "Buffer is empty" : "read() error");
        return err;
    }

    memcpy(&len, rbuf, 4);              //assuming little endian
    if(len > k_max_msg) {
        msg("Message too long");
        return -1;
    }

    //reply body
    err = read_full(fd, &rbuf[4], len);
    if(err) {
        msg("read() error");
        return err;
    }

    printf("Server says: %.*s\n", len, &rbuf[4]);
    return 0;
}*/

static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
    uint32_t len = 4;                       //4 bytes for the header (nstr)
    for(const std::string &s : cmd) {
        len += 4 + s.size();                //4 bytes for length of each string in cmd
    }

    if(len > k_max_msg) {
        msg("Command is too long");
        return -1;
    }

    char wbuf[4 + k_max_msg];
    memcpy(&wbuf[0], &len, 4);              //Assuming little endian
    uint32_t n = (uint32_t)cmd.size();
    memcpy(&wbuf[4], &n, 4);

    size_t cur = 8;
    for(const std::string &s :cmd) {
        uint32_t p = (uint32_t)s.size();
        memcpy(&wbuf[cur], &p, 4);
        memcpy(&wbuf[cur + 4], s.data(), s.size());
        cur += 4 + s.size();
    }

    return write_all(fd, wbuf, 4 + len);
}

static int32_t read_res(int fd) {
    //4 byte header
    char rbuf[4 + k_max_msg + 1];       //CHK
    errno = 0;
    int32_t rv = read_full(fd, rbuf, 4);
    if(rv) {
        if(errno == 0) {
            msg("End of file");
        } else {
            msg("read() error");
        }
        return rv;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);           //Assuming little endian
    if(len > k_max_msg) {
        msg("Message is too long");
        return -1;
    }

    //Actual message body
    rv = read_full(fd, &rbuf[4], len);
    if(rv) {
        msg("read() error");
        return rv;
    }

    //Print the result
    rv = print_response((uint8_t *)&rbuf[4], len);
    if(rv > 0 && (uint32_t)rv != len) {
        msg("Bad response");
        rv = -1;
    }

    return rv;
}

/*//appends to the back of the buffer
static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}*/

enum {
    TAG_NIL,    //Null
    TAG_ERR,    //Error code + message
    TAG_STR,    //String
    TAG_INT,    //Integer 64 bit
    TAG_DBL,    //Double
    TAG_ARR     //Array
};

static int32_t print_response(const uint8_t *data, size_t size) {
    if(size < 1) {
        msg("Bad response");
        return -1;
    }
    
    switch(data[0]) {
        case TAG_NIL:
            printf("(Nil)\n");
            return 1;

        case TAG_ERR:
            if(size < 1 + 8) {
                msg("Bad response");
                return -1;
            } else {
                int32_t code = 0;       //uint32_t should be fine too
                uint32_t length = 0;
                memcpy(&code, &data[1], 4);
                memcpy(&length, &data[5], 4);

                if(size < 1 + 8 + length) {
                    msg("Bad response");
                    return -1;
                }

                printf("(Err) %d %.*s\n", code, length, &data[1 + 8]);
                return 1 + 8 + length;
            }
        
        case TAG_STR:
            if(size < 1 + 4) {
                msg("Bad response");
                return -1;
            } else {
                uint32_t length = 0;
                memcpy(&length, &data[1], 4);

                if(size < 1 + 4 + length) {
                    msg("Bad response");
                    return -1;
                }

                printf("(Str) %.*s\n", length, &data[1 + 4]);
                return 1 + 4 + length;
            }

        case TAG_INT:
            if(size < 1 + 8) {
                msg("Bad response");
                return -1;
            } else {
                int64_t value = 0;
                memcpy(&value, &data[1], 8);
                
                printf("(Int) %ld\n", value);
                return 1 + 8;
            }

        case TAG_DBL:
            if(size < 1 + 8) {
                msg("Bad response");
                return -1;
            } else {
                double value = 0;
                memcpy(&value, &data[1], 8);

                printf("(Dbl) %g\n", value);
                return 1 + 8;
            }

        case TAG_ARR:
            if(size < 1 + 4) {
                msg("Bad response");
                return -1;
            } else {
                uint32_t length = 0;
                memcpy(&length, &data[1], 4);

                printf("(Arr) Length = %u\n", length);
                size_t arr_bytes = 1 + 4;
                for(uint32_t i = 0; i < length; i++) {
                    int32_t rv = print_response(&data[arr_bytes], size - arr_bytes);
                    if(rv < 0) {
                        return rv;  //Error in reading array element
                    }

                    arr_bytes += (size_t)rv;
                }

                printf("(Arr) END\n");
                return (int32_t)arr_bytes;
            }

        default:
            msg("Bad response");
            return -1;
    }
}

