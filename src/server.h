#include "bstring.h"
#include "uthash.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/thread.h>
#include "wren.h"

#define BUFFER_SIZE 1024
#define MAX_HEADERS 128

enum HttpMethodTyp
{
    HTTP_UNKNOWN = 0,
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_PATCH,
    HTTP_DELETE,
};

struct HttpMethod
{
    const char *const str;
    enum HttpMethodTyp typ;
};

const char HTTP_VERSION[] = "HTTP/1.1";

struct HttpMethod KNOWN_HTTP_METHODS[] = {
    {.str = "GET", .typ = HTTP_GET},
    {.str = "POST", .typ = HTTP_POST},
    {.str = "PUT", .typ = HTTP_PUT},
    {.str = "PATCH", .typ = HTTP_PATCH},
    {.str = "DELETE", .typ = HTTP_DELETE},
    {.str = NULL, .typ = HTTP_UNKNOWN},
};

typedef struct _HttpRequest
{
    enum HttpMethodTyp method;
    char *path;
    char *host;
    char *command;
    int content_length;
    char *_buffer;
    size_t _buffer_len;
    struct Bstring *body;
} HttpRequest;

typedef struct _HttpApplication 
{
    char name[128];
    struct Bstring *path;
    WrenConfiguration vm_config;
    WrenVM *vm;
    UT_hash_handle hh;
} HttpApplication;

typedef struct _HttpConnection
{
    evutil_socket_t fd;
    struct sockaddr_storage addr;
    int addr_len;
    char addr_str[64];
    HttpRequest request;
    struct bufferevent *bev;
    HttpApplication *apps;
    HttpApplication *slot[8];
    UT_hash_handle hh;
} HttpConnection;

extern struct Bstring *filename;
extern void http_handle_connection(int conn_fd, void *arg, int arg_len);
extern void http_start(int thread_count);
extern void http_end();
