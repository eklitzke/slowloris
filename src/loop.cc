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
#include <unordered_set>

#include <unistd.h>

namespace {
std::random_device rand_device;
std::default_random_engine rand_engine(rand_device());

class Socket;
void OnRead(bufferevent *bev, void *ctx);
void OnEvent(bufferevent *bev, short events, void *ctx);

std::unordered_set<Socket *> sockets_;

struct GlobalState {
  GlobalState(int max_s, int max_br, event_base *b)
      : max_seconds(max_s),
        max_before_reset(max_br),
        score(0),
        reset_timer(nullptr),
        base(b) {}

  int max_seconds;
  int max_before_reset;
  int score;
  event *reset_timer;
  event_base *base;

  void ResetAndPrintScore() {
    std::cout << score << std::endl;
    score = 0;
  }
};

class Socket {
 public:
  Socket() = delete;
  Socket(const Socket &other) = delete;
  Socket(GlobalState *state, evutil_socket_t fd)
      : buf_(bufferevent_socket_new(state->base, fd, BEV_OPT_CLOSE_ON_FREE)),
        state_(state) {}
  ~Socket() {
    sockets_.erase(this);
    bufferevent_free(buf_);
  }

  void Start() {
    assert(buf_ != nullptr);
    UpdateTimeout();

    bufferevent_setcb(buf_, OnRead, nullptr, OnEvent, this);
    bufferevent_enable(buf_, EV_READ | EV_WRITE);
    sockets_.insert(this);
  }

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

    std::uniform_int_distribution<int> seconds_dist(0, state_->max_seconds - 1);
    std::uniform_int_distribution<int> useconds_dist(0, 999999);

    int sec = seconds_dist(rand_engine);
    int usec = useconds_dist(rand_engine);

    next_.tv_usec += usec;
    while (next_.tv_usec >= 1000000) {
      next_.tv_usec -= 1000000;
      next_.tv_sec++;
    }
    next_.tv_sec += sec;

    evbuffer_add_printf(bufferevent_get_output(buf_), "%d %d.%06d\n",
                        state_->score, sec, usec);
  }

  GlobalState *state() { return state_; }

 private:
  timeval next_;
  bufferevent *buf_;
  GlobalState *state_;
};

void Reset(evutil_socket_t unused_fd, short unused_what, void *ctx);

// close all of the sockets
void CloseAllSockets(GlobalState *state) {
  while (!sockets_.empty()) {
    auto it = sockets_.begin();
    delete *it;
  }
  state->ResetAndPrintScore();

  // schedule a Reset
  event_free(state->reset_timer);
  state->reset_timer = nullptr;
}

void Reset(evutil_socket_t unused_fd, short unused_what, void *ctx) {
  GlobalState *state = reinterpret_cast<GlobalState *>(ctx);
  CloseAllSockets(state);
}

void OnRead(bufferevent *bev, void *ctx) {
  static char buf[8192];
  bufferevent_read(bev, buf, sizeof(buf));

  Socket *sock = reinterpret_cast<Socket *>(ctx);
  if (sock->IsReady()) {
    sock->state()->score++;
    sock->UpdateTimeout();
  } else {
    CloseAllSockets(sock->state());
  }
}

void OnEvent(bufferevent *unused_bev, short events, void *ctx) {
  if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
    Socket *sock = reinterpret_cast<Socket *>(ctx);
    delete sock;
  }
}

void OnAccept(evconnlistener *listener, evutil_socket_t fd,
              sockaddr *unused_addr, int unused_len, void *ctx) {
  GlobalState *state = reinterpret_cast<GlobalState *>(ctx);
  if (state->reset_timer == nullptr) {
    state->reset_timer = evtimer_new(state->base, Reset, state);
    timeval tv;
    tv.tv_sec = state->max_before_reset;
    tv.tv_usec = 0;
    evtimer_add(state->reset_timer, &tv);
  }
  Socket *sock = new Socket(state, fd);
  sock->Start();
}
}

namespace slowpoke {
int RunLoop(int port, int timeout, int max_timeout) {
  event_base *base = event_base_new();
  GlobalState state(timeout, max_timeout, base);

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
      base, OnAccept, &state, LEV_OPT_REUSEABLE, SOMAXCONN,
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
