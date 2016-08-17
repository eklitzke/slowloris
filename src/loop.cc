#include "./loop.h"

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>

#include <unistd.h>

#define READ_TIMEOUT (BEV_EVENT_TIMEOUT | BEV_EVENT_READING)

namespace {
std::random_device rand_device;
std::default_random_engine rand_engine(rand_device());

void OnRead(bufferevent *bev, void *ctx);
void OnEvent(bufferevent *bev, short events, void *ctx);

int MAX_SECONDS = 1;

int SCORE = 0;

class Socket {
 public:
  Socket() = delete;
  Socket(const Socket &other) = delete;
  Socket(event_base *base, evutil_socket_t fd)
      : buf_(bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE)) {
    assert(buf_ != nullptr);
    UpdateTimeout();

    bufferevent_setcb(buf_, OnRead, nullptr, OnEvent, this);
    bufferevent_enable(buf_, EV_READ | EV_WRITE);
  }
  ~Socket() { bufferevent_free(buf_); }

  bool IsReady() const {
    timeval tv;
    gettimeofday(&tv, nullptr);
    if (tv.tv_sec == next_.tv_sec) {
      return tv.tv_usec >= next_.tv_usec;
    }
    return tv.tv_sec > next_.tv_sec;
  }

  void UpdateTimeout() {
    gettimeofday(&next_, nullptr);

    std::uniform_int_distribution<int> seconds_dist(0, MAX_SECONDS - 1);
    std::uniform_int_distribution<int> useconds_dist(0, 999999);

    int sec = seconds_dist(rand_engine);
    int usec = useconds_dist(rand_engine);

    next_.tv_usec += usec;
    while (next_.tv_usec >= 1000000) {
      next_.tv_usec -= 1000000;
      next_.tv_sec++;
    }
    next_.tv_sec += sec;

    evbuffer_add_printf(bufferevent_get_output(buf_), "%d.%06d\n", sec, usec);
  }

 private:
  timeval next_;
  bufferevent *buf_;
};

void OnRead(bufferevent *bev, void *ctx) {
  static char buf[8192];
  bufferevent_read(bev, buf, sizeof(buf));

  Socket *sock = reinterpret_cast<Socket *>(ctx);
  if (sock->IsReady()) {
    sock->UpdateTimeout();
    SCORE++;
  } else {
    SCORE -= 10;
    delete sock;
  }
  std::cout << "score = " << SCORE << std::endl;
}

void OnEvent(bufferevent *bev, short events, void *ctx) {
  if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
    Socket *sock = reinterpret_cast<Socket *>(ctx);
    delete sock;
  }
}

void OnAccept(evconnlistener *listener, evutil_socket_t fd, sockaddr *addr,
              int len, void *ctx) {
  std::cout << "new connection, fd = " << fd << std::endl;
  event_base *base = evconnlistener_get_base(listener);
  Socket *sock = new Socket(base, fd);
}
}

namespace slowloris {
int RunLoop(int port, int timeout) {
  MAX_SECONDS = timeout;

  event_base *base = event_base_new();
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
