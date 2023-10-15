#ifndef HTTP_INTERNAL_H
#define HTTP_INTERNAL_H

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#if FMN_USE_mswin
  // MinGW doesn't give us sys/poll.h. Not going to bother figuring it out; I won't need the editor under Windows.
  #include "fake_poll.h"
  #include <Windows.h>
#else
  #include <sys/poll.h>
  #include <netdb.h>
  #include <sys/socket.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>

#include "http.h"
#include "http_dict.h"
#include "http_context.h"
#include "http_server.h"
#include "http_socket.h"
#include "http_listener.h"
#include "http_xfer.h"

#endif
