#define SWRAP_IMPLEMENTATION
#include "swrap.h"
#include "server.h"
#include "pthread_pool.h"

// ----- HTTP server related declarations ----- //

extern struct Bstring *filename;
extern void handle_connection(int conn_fd);

int server_fd = -1;
struct swrap_addr client_addr;

void cleanup_and_exit()
{
  if (server_fd != -1)
  {
    swrapClose(server_fd);
  }

  if (filename != NULL)
  {
    bstring_free(filename);
  }

  swrapTerminate();
  exit(0);
}

int main(int argc, char **argv)
{
  swrapInit();
  // set base directory of public files (defaults to public)
  if (argc > 2)
  {
    filename = bstring_init(0, argv[2]);
    if (argv[2][strlen(argv[2]) - 1] != '/')
    {
      bstring_append(filename, "/");
    }
  }
  else
  {
    filename = bstring_init(0, "./public/");
  }

  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  server_fd = swrapSocket(SWRAP_TCP, SWRAP_BIND, 0, "0.0.0.0", "42024");
  if (server_fd == -1)
  {
    printf("error: Socket creation failed: %s...\n", strerror(errno));
    cleanup_and_exit();
  }

  // lose the "Address already in use" error message. why this happens
  // in the first place? well even after the server is closed, the port
  // will still be hanging around for a while, and if you try to restart
  // the server, you'll get an "Address already in use" error message
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
  {
    printf("error: SO_REUSEADDR failed: %s \n", strerror(errno));
    cleanup_and_exit();
  }
  int connection_backlog = 5;
  if (swrapListen(server_fd, connection_backlog) != 0)
  {
    printf("error: Listen failed: %s \n", strerror(errno));
    cleanup_and_exit();
  }

  printf("Waiting for a client to connect...\n");

  while (true)
  {
    int conn_fd = swrapAccept(server_fd, &client_addr);
    if (conn_fd == -1)
    {
      printf("error: Accept failed: %s \n", strerror(errno));
      continue;
    }

    handle_connection(conn_fd);
  }

  cleanup_and_exit();
  return 0;
}
