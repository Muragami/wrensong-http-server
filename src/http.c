#include "http.h"
#include <event2/event.h>
#include <event2/thread.h>

struct Bstring *filename = NULL;
void *thread_pool = NULL;
HttpConnection *connections = NULL;
struct event_base *http = NULL;
pthread_t http_thread;

void *_handle_connection(void *conn_fd_ptr)
{
    assert(conn_fd_ptr != NULL);
    HttpConnection *conn = conn_fd_ptr;
    HttpRequest req = conn->request;
    int closing = 0;

    char *parse_buffer = req._buffer;

    // parse method & store it to req
    char *method = strsep(&parse_buffer, " ");
    int i = 0;
    while (KNOWN_HTTP_METHODS[i].str != NULL)
    {
        if (strcmp(method, KNOWN_HTTP_METHODS[i].str) == 0)
        {
            req.method = KNOWN_HTTP_METHODS[i].typ;
            break;
        }
        i++;
    }
    // unknowm method
    if (req.method == HTTP_UNKNOWN)
    {
        return NULL;
    }
    // parse path & store it to req
    req.path = strsep(&parse_buffer, " ");
    // parse http version
    strsep(&parse_buffer, "\r");
    // the \r was consumed but the \n is still remaining so consume it
    parse_buffer++;

    for (size_t i = 0; i < MAX_HEADERS; ++i)
    {
        char *header_line = strsep(&parse_buffer, "\r");

        parse_buffer++; // consume \n

        // if the line was just "\r\n" i.e the end of headers section
        if (header_line[0] == '\0')
        {
            break;
        }

        char *key = strsep(&header_line, ":");
        header_line++; // consume the space
        char *value = strsep(&header_line, "\0");

        if (strncmp(key, "Content-Length", 14) == 0)
        {
            req.content_length = atoi(value);
        }
        else if (strncmp(key, "Host", 4) == 0)
        {
            req.host = value;
        }
        else if (strncmp(key, "Command", 12) == 0)
        {
            req.command = value;
        }
        else if (strncmp(key, "Connection", 10) == 0)
        {
            if (strncmp(value, "close", 5) == 0)
            {
                closing = 1;
            }
        }
    }

    

    free(conn->request._buffer);
    if (closing)
    {
        // remove the connection from the hash table
        HASH_DEL(connections, conn);
        bufferevent_free(conn->bev);
        free(conn);
    }
    return NULL;
}

void _http_read(struct bufferevent *bev, short events, void *ptr)
{
    HttpConnection *conn = ptr;
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    if (len == 0)
        return;
    conn->request._buffer = malloc(len + 1);
    conn->request._buffer_len = len;
    evbuffer_copyout(input, conn->request._buffer, len);
    conn->request._buffer[len] = '\0';
    conn->bev = bev;
    pool_enqueue(thread_pool, conn, 0);
}

void _http_event(struct bufferevent *bev, short events, void *ptr)
{
}

void http_handle_connection(int conn_fd, void *arg, int arg_len)
{
    // create a new HttpConnection and add it to the connections hash table
    HttpConnection *conn = calloc(1, sizeof(HttpConnection));
    conn->fd = conn_fd;
    memcpy(&conn->addr, arg, arg_len);
    conn->addr_len = arg_len;
    HASH_ADD_INT(connections, fd, conn);
    // create a bufferevent for the connection
    struct bufferevent *b = NULL;
    b = bufferevent_socket_new(http, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(b, _http_read, NULL, _http_event, conn);
    bufferevent_enable(b, EV_READ);
}

void *http_thread_func(void *arg)
{
    struct event_base *base = arg;
    event_base_dispatch(base);
}

void http_start(int thread_count)
{
    http = event_base_new();
    thread_pool = pool_start(_handle_connection, thread_count);
    pthread_create(&http_thread, NULL, http_thread_func, http);
}

void http_end()
{
    event_base_loopbreak(http);
    pthread_join(http_thread, NULL);
    event_base_free(http);
    pool_end(thread_pool);
}
