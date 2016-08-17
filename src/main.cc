#include <getopt.h>

#include <iostream>

#include "./loop.h"

const char usage_str[] = "slowloris [-p|--port PORT] [-t|--timeout SECS]\n";

int main(int argc, char **argv) {
  int timeout = 1;
  int port = 9000;
  for (;;) {
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"timeout", required_argument, 0, 't'},
        {0, 0, 0, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "hp:t:", long_options, &option_index);
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
      case 't':
        timeout = std::stod(optarg);
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

  return slowloris::RunLoop(port, timeout);
}
