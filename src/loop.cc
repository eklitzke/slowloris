#include "./loop.h"

#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>

#include <cstring>
#include <iostream>

#define READ_TIMEOUT (BEV_EVENT_TIMEOUT | BEV_EVENT_READING)

namespace {
event_base *base = nullptr;

int SOCKET_TIMEOUT = 1;

void OnRead(bufferevent *bev, void *ctx) {
  std::cout << "got data on connection" << std::endl;
}

void OnEvent(bufferevent *bev, short events, void *ctx) {
  if ((events & READ_TIMEOUT) == READ_TIMEOUT) {
    std::cout << "connection timed out\n";
    bufferevent_free(bev);
  }
}

void OnAccept(evconnlistener *listener, evutil_socket_t sock, sockaddr *addr,
              int len, void *unused) {
  std::cout << "got a new connection " << sock << std::endl;
  bufferevent *bev = bufferevent_socket_new(base, sock, BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb(bev, OnRead, nullptr, OnEvent, nullptr);

  timeval tv;
  tv.tv_sec = SOCKET_TIMEOUT;
  tv.tv_usec = 0;
  bufferevent_set_timeouts(bev, &tv, nullptr);

  bufferevent_enable(bev, EV_READ | EV_WRITE);
}
}

namespace slowloris {

int RunLoop(int port, int timeout) {
  SOCKET_TIMEOUT = timeout;

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
}
