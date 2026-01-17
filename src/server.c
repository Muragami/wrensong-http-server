#include "server.h"
#include "pthread_pool.h"
#include "tconfig.h"
#include <sys/stat.h>

// our static variables
static evutil_socket_t listener;
static struct sockaddr_in sin4;
static struct sockaddr_in6 sin6;
static struct event_base *server;
static struct event *listener4_event;
static struct event *listener6_event;
static struct event *update_event;
static struct timeval tv;
static struct TConfig *config = NULL;
static int port = 40000;
static int threads = 16;
static int ipv6 = 0;
static time_t last_config_mod_time = 0;
static struct stat fstat;
static const char *the_config_path = "";

void cleanup_and_exit()
{
  if (listener != -1)
  {
    close(listener);
  }

  if (filename != NULL)
  {
    bstring_free(filename);
  }
  exit(0);
}

void do_accept(evutil_socket_t listener, short event, void *arg)
{
  struct event_base *base = arg;
  struct sockaddr_storage ss;
  socklen_t slen = sizeof(ss);
  int fd = accept(listener, (struct sockaddr *)&ss, &slen);
  if (fd < 0)
    return;
  else if (fd > FD_SETSIZE)
    close(fd);
  else
    handle_connection(fd, &ss, slen);
}

void do_update(evutil_socket_t fd, short events, void *arg)
{
  stat(the_config_path, &fstat);
  if (last_config_mod_time != fstat.st_mtime)
  {
    // reload the configuration
    // TODO
  }
}

void read_config(const char *config_path)
{
  config = tconfig_new();
  if (tconfig_read_file(config, config_path) != 0)
  {
    fprintf(stderr, "Failed to read config file: %s\n", config_path);
    tconfig_free(config);
    return;
  }

  // get server config
  ini_table_get_entry_as_int(config, "server", "port", &port);
  ini_table_get_entry_as_int(config, "server", "threads", &threads);
  ini_table_get_entry_as_bool(config, "server", "ipv6", &ipv6);

  stat(config_path, &fstat);
  last_config_mod_time = fstat.st_mtime;
  the_config_path = config_path;
}

int main(int argc, char **argv)
{
  event_init();
  // Initialize winsock2 if needed
#ifdef _WIN32
  WSADATA WsaData;
  return (WSAStartup(MAKEWORD(2, 2), &WsaData) != NO_ERROR);
  evthread_use_windows_threads();
#else
  evthread_use_pthreads();
#endif
  // read our config file
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
    return 1;
  }
  read_config((const char *)argv[1]);
  // create the server event base
  server = event_base_new();
  if (!server)
    return; /*XXXerr*/

  // create the listening socket for ipv4
  sin4.sin_family = AF_INET;
  sin4.sin_addr.s_addr = 0;
  sin4.sin_port = htons(port);
  listener = socket(AF_INET, SOCK_STREAM, 0);
  evutil_make_socket_nonblocking(listener);
  int one = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  if (bind(listener, (struct sockaddr *)&sin4, sizeof(sin4)) < 0)
  {
    perror("bind");
    return;
  }
  if (listen(listener, 16) < 0)
  {
    perror("listen");
    return;
  }
  // register the listener event
  listener4_event = event_new(server, listener, EV_READ | EV_PERSIST, do_accept, (void *)server);
  event_add(listener4_event, NULL);
  if (ipv6)
  {
    // create the listening socket for ipv6
    sin6.sin6_family = AF_INET6;
    sin6.sin6_addr = in6addr_any;
    sin6.sin6_port = htons(port);
    evutil_socket_t listener6 = socket(AF_INET6, SOCK_STREAM, 0);
    evutil_make_socket_nonblocking(listener6);
    int one = 1;
    setsockopt(listener6, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (bind(listener6, (struct sockaddr *)&sin6, sizeof(sin6)) < 0)
    {
      perror("bind");
      return;
    }
    if (listen(listener6, 16) < 0)
    {
      perror("listen");
      return;
    } // register the listener event
    listener6_event = event_new(server, listener6, EV_READ | EV_PERSIST, do_accept, (void *)server);
    event_add(listener6_event, NULL);
  }
  // register the update event, every second
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  update_event = event_new(server, -1, EV_TIMEOUT | EV_PERSIST, do_update, NULL);
  event_add(update_event, &tv);
  http_start(threads);
  // let's start the server
  event_base_dispatch(server);
  // cleanup and exit
  cleanup_and_exit();
  http_end();
  return 0;
}
