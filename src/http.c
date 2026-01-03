#include "http.h"

struct Bstring *filename = NULL;

char *http_get_header(struct HttpRequest *self, char *header)
{
    assert(self != NULL);
    assert(header != NULL);

    for (size_t i = 0; i < self->headers_len; ++i)
    {
        if (strcasecmp(header, self->headers[i].key) == 0)
        {
            return self->headers[i].value;
        }
    }

    return NULL;
}

void *_handle_connection(void *conn_fd_ptr)
{
    assert(conn_fd_ptr != NULL);

    int conn_fd = *(int *)conn_fd_ptr;

    printf("processing connection with fd %d\n", conn_fd);

    // read data from the client
    struct HttpRequest req = {0};
    req._buffer = malloc(BUFFER_SIZE);

    FILE *fp = NULL;
    size_t orig_filename_len = filename->length;

    if (req._buffer == NULL)
    {
        printf("error: Failed to allocate memory\n");
        goto cleanup;
    }

    ssize_t bytes_read = swrapReceive(conn_fd, req._buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0)
    {
        printf("error: Recv failed/closed: %s \n", strerror(errno));
        return NULL;
    }

    // null-terminate _buffer
    req._buffer[bytes_read] = '\0';
    req._buffer_len = (size_t)bytes_read;

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
        printf("error(parse): unknown method %s\n", method);
        return NULL;
    }

    // parse path & store it to req
    req.path = strsep(&parse_buffer, " ");

    // parse http version
    strsep(&parse_buffer, "\r");
    // char *http_version = strsep(&parse_buffer, "\r");

    // the \r was consumed but the \n is still remaining so consume it
    parse_buffer++;

    // parse headers & store it to req
    req.headers = malloc(sizeof(struct HttpHeader) * MAX_HEADERS);
    req.headers_len = 0;

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

        req.headers[i].key = key;
        req.headers[i].value = value;

        ++req.headers_len;
    }

    // get content length and parse request body
    size_t content_length = 0;
    char *content_length_header = http_get_header(&req, "content-length");
    if (content_length_header != NULL)
    {
        errno = 0;
        size_t result = strtoul(content_length_header, NULL, 10);
        // store result to content_length if there was no error
        if (errno == 0)
        {
            content_length = result;
        }
    }

    if (content_length > 0)
    {
        req.body = bstring_init(content_length + 1, NULL);
        bstring_append(req.body, parse_buffer);

        // content lenth mismatch
        if (req.body->length != content_length)
        {
        }
    }

    // send response
    int bytes_sent = -1;
    if (strcmp(req.path, "/") == 0)
    {
        const char res[] = "HTTP/1.1 200 OK\r\n\r\n";
        bytes_sent = send(conn_fd, res, sizeof(res) - 1, 0);
    }
    else if (strcmp(req.path, "/user-agent") == 0)
    {
        char *s = http_get_header(&req, "user-agent");
        if (s == NULL)
        {
            s = "NULL";
        }

        size_t slen = strlen(s);

        char *res = malloc(BUFFER_SIZE);
        sprintf(res,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length:%zu\r\n\r\n"
                "%s",
                slen, s);

        bytes_sent = send(conn_fd, res, strlen(res), 0);
        free(res);
    }
    else if (strncmp(req.path, "/echo/", 6) == 0)
    {
        char *s = req.path + 6;
        size_t slen = strlen(s);

        char *res = malloc(BUFFER_SIZE);
        sprintf(res,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length:%zu\r\n\r\n"
                "%s",
                slen, s);

        bytes_sent = send(conn_fd, res, strlen(res), 0);
        free(res);
    }
    else if (req.method == HTTP_GET && strncmp(req.path, "/files/", 7) == 0)
    {
        bstring_append(filename, req.path + 7);

        // open the file
        errno = 0;
        fp = fopen(filename->data, "r");
        if (fp == NULL && errno != ENOENT)
        {
            perror("error: fopen()");
            goto cleanup;
        }

        struct Bstring *res = bstring_init(0, HTTP_VERSION);

        // handle non-existant file
        if (errno == ENOENT)
        {
            bstring_append(res, " 404 Not Found\r\n\r\n");
            bytes_sent = send(conn_fd, res->data, res->length, 0);
            goto cleanup;
        }

        // go to the end of file
        if (fseek(fp, 0, SEEK_END) != 0)
        {
            perror("error: fseek()");
            goto cleanup;
        }

        // get file size
        long file_size = ftell(fp);
        if (file_size < 0)
        {
            perror("error: fseek()");
            goto cleanup;
        }

        // go back to the beginning of file
        errno = 0;
        rewind(fp);
        if (errno != 0)
        {
            perror("error: rewind()");
            goto cleanup;
        }

        char file_size_str[32];
        snprintf(file_size_str, 32, "%ld", file_size);

        bstring_append(res, " 200 OK\r\n");
        bstring_append(res, "Content-Type: application/octet-stream\r\n");
        bstring_append(res, "Content-Length: ");
        bstring_append(res, file_size_str);
        bstring_append(res, "\r\n\r\n");

        bytes_sent = send(conn_fd, res->data, res->length, 0);
        bstring_free(res);

        char buffer[BUFFER_SIZE];

        while (file_size > 0)
        {
            long bytes_to_read = file_size > BUFFER_SIZE ? BUFFER_SIZE : file_size;
            size_t bytes_read = fread(buffer, bytes_to_read, 1, fp);

            // check if bytes_read is less than expected because of error
            if (bytes_read < bytes_to_read && ferror(fp))
            {
                puts("error: fread()");
                goto cleanup;
            }

            bytes_sent = send(conn_fd, buffer, bytes_to_read, 0);
            file_size -= bytes_to_read;
        }
    }
    else if (req.method == HTTP_POST && strncmp(req.path, "/files/", 7) == 0)
    {
        bstring_append(filename, req.path + 7);

        // open the file in read mode to check whether it exists or not
        errno = 0;
        fp = fopen(filename->data, "r");
        if (fp != NULL)
        {
            // file already exists
            const char res[] = "HTTP/1.1 409 Conflict\r\n\r\n";
            bytes_sent = send(conn_fd, res, sizeof(res) - 1, 0);
            goto cleanup;
        }

        // open the file for writing now
        errno = 0;
        fp = fopen(filename->data, "w");
        if (fp == NULL)
        {
            perror("error: fopen()");
            goto cleanup;
        }

        size_t num_chunks = req.body->length / BUFFER_SIZE;
        if (num_chunks > 0)
        {
            fwrite(req.body->data, BUFFER_SIZE, num_chunks, fp);
        }

        size_t remaining_bytes = req.body->length % BUFFER_SIZE;
        if (remaining_bytes > 0)
        {
            fwrite(&req.body->data[req.body->length - remaining_bytes],
                   remaining_bytes, 1, fp);
        }

        const char res[] = "HTTP/1.1 201 Created\r\n\r\n";
        bytes_sent = send(conn_fd, res, sizeof(res) - 1, 0);
    }
    else
    {
        const char res[] = "HTTP/1.1 404 Not Found\r\n\r\n";
        bytes_sent = send(conn_fd, res, sizeof(res) - 1, 0);
    }

    if (bytes_sent == -1)
    {
        perror("error: send()");
    }

cleanup:
    free(conn_fd_ptr);
    free(req._buffer);
    free(req.headers);
    if (req.body != NULL)
    {
        bstring_free(req.body);
    }

    close(conn_fd);

    if (fp != NULL)
    {
        fclose(fp);
    }

    filename->length = orig_filename_len;
    filename->data[orig_filename_len] = '\0';

    return NULL;
}

void handle_connection(int conn_fd)
{
    pthread_t new_thread;
    int *conn_fd_ptr = malloc(sizeof(int));
    if (conn_fd_ptr == NULL)
    {
        printf("error: handle_connection(): Failed to allocate memory for "
               "conn_fd_ptr");
        return;
    }

    *conn_fd_ptr = conn_fd;
    pthread_create(&new_thread, NULL, _handle_connection, conn_fd_ptr);
}
