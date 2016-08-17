#include <getopt.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <event2/event.h>
#include <event2/listener.h>

#include <cstring>
#include <iostream>

const char usage_str[] = "slowloris [-p|--port PORT]\n";

event_base *base = nullptr;

void OnAccept(evconnlistener *listener, evutil_socket_t sock, sockaddr *addr,
              int len, void *unused) {
  std::cout << "got a new connection\n";
  evutil_closesocket(sock);
}

int main(int argc, char **argv) {
  int port = 9000;
  for (;;) {
    static struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                           {"port", required_argument, 0, 'p'},
                                           {0, 0, 0, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "hp:", long_options, &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 0:
        if (long_options[option_index].flag != 0) {
          // if the option set a flag, do nothing
          break;
        }
        break;
      case 'h':
        std::cout << usage_str;
        return 0;
        break;
      case 'p':
        port = std::stod(optarg);
        break;
      case '?':
        // getopt_long should already have printed an error message
        break;
      default:
        abort();
    }
  }
  if (optind != argc) {
    std::cerr << usage_str;
    return 1;
  }

  base = event_base_new();
  if (base == nullptr) {
    std::cerr << "failed to event_base_new()\n";
    return 1;
  }

  sockaddr_in listen_addr;
  memset(&listen_addr, 0, sizeof(listen_addr));
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = INADDR_ANY;
  listen_addr.sin_port = htons(port);
  evconnlistener *listener = evconnlistener_new_bind(
      base, OnAccept, nullptr, LEV_OPT_REUSEABLE, SOMAXCONN,
      (sockaddr *)&listen_addr, sizeof(listen_addr));
  if (listener == nullptr) {
    std::cerr << "failed to bind/listen on port\n";
    return 1;
  }

  event_base_dispatch(base);

  evconnlistener_free(listener);
  event_base_free(base);

  return 0;
}
