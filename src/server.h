#include "bstring.h"
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

struct HttpHeader
{
    char *key;
    char *value;
};

struct HttpRequest
{
    enum HttpMethodTyp method;
    char *path;

    // headers
    struct HttpHeader *headers;
    size_t headers_len;

    // the request buffer
    char *_buffer;
    size_t _buffer_len;

    struct Bstring *body;
};