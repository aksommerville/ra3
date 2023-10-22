#include "http_internal.h"

/* File in error or hangup state.
 */
 
static int http_update_fd_error(struct http_context *context,int fd) {
  struct http_server *server=http_context_get_server_by_fd(context,fd);
  if (server) {
    http_context_remove_server(context,server);
    return 0;
  }
  struct http_socket *socket=http_context_get_socket_by_fd(context,fd);
  if (socket) {
    http_context_remove_socket(context,socket);
    return 0;
  }
  struct http_extfd *extfd=http_context_get_extfd_by_fd(context,fd);
  if (extfd) return extfd->cb(fd,extfd->userdata);
  return 0;
}

/* File readable.
 */
 
static int http_update_fd_read(struct http_context *context,int fd) {
  
  struct http_server *server=http_context_get_server_by_fd(context,fd);
  if (server) {
    if (http_server_accept(server)<0) {
      fprintf(stderr,"Lost server on fd %d\n",fd);
      http_context_remove_server(context,server);
    }
    return 0;
  }
  
  struct http_socket *socket=http_context_get_socket_by_fd(context,fd);
  if (socket) {
    if (http_socket_read(socket)<0) {
      if (http_socket_ok_to_close(socket)) {
        //fprintf(stderr,"Lost socket on fd %d and it seems ok.\n",fd);
        if ((socket->protocol==HTTP_PROTOCOL_WEBSOCKET)&&socket->listener&&socket->listener->cb_disconnect) {
          socket->listener->cb_disconnect(socket,socket->listener->userdata);
        }
        http_context_remove_socket(context,socket);
        return 0;
      } else {
        fprintf(stderr,"Lost socket on fd %d mid-transaction.\n",fd);
        return -1;
      }
    }
    if (http_socket_digest_input(socket)<0) {
      return -1;
    }
    return 0;
  }
  
  struct http_extfd *extfd=http_context_get_extfd_by_fd(context,fd);
  if (extfd) {
    return extfd->cb(fd,extfd->userdata);
  }
  
  return 0;
}

/* File writeable.
 */
 
static int http_update_fd_write(struct http_context *context,int fd) {
  struct http_socket *socket=http_context_get_socket_by_fd(context,fd);
  if (socket) {
    if (http_socket_write(socket)<0) {
      return -1;
    }
    return 0;
  }
  return 0;
}

/* Identify all files that need updated.
 */
 
static int http_context_pollfdv_rebuild(struct http_context *context) {
  context->pollfdc=0;
  int i;
  
  struct http_server **server=context->serverv;
  for (i=context->serverc;i-->0;server++) {
    if ((*server)->fd<0) continue;
    struct pollfd *pollfd=http_context_pollfdv_require(context);
    if (!pollfd) return -1;
    pollfd->fd=(*server)->fd;
    pollfd->events=POLLIN|POLLERR|POLLHUP;
  }
  
  struct http_socket **socket=context->socketv;
  for (i=context->socketc;i-->0;socket++) {
    if ((*socket)->fd<0) continue;
    struct pollfd *pollfd=http_context_pollfdv_require(context);
    if (!pollfd) return -1;
    pollfd->fd=(*socket)->fd;
    
    // If this socket had a deferred response that finished, encode it.
    if ((*socket)->rsp&&((*socket)->rsp->state==HTTP_XFER_STATE_DEFERRAL_COMPLETE)) {
      if (http_socket_encode_xfer(*socket,(*socket)->rsp)<0) return -1;
    }
    
    if ((*socket)->wbufc) {
      pollfd->events=POLLOUT|POLLERR|POLLHUP;
    } else {
      pollfd->events=POLLIN|POLLERR|POLLHUP;
    }
  }
  
  struct http_extfd *extfd=context->extfdv;
  for (i=context->extfdc;i-->0;extfd++) {
    struct pollfd *pollfd=http_context_pollfdv_require(context);
    if (!pollfd) return -1;
    pollfd->fd=extfd->fd;
    pollfd->events=POLLIN|POLLERR|POLLHUP;
  }

  return 0;
}

/* Update, main entry point.
 */
 
int http_update(struct http_context *context,int toms) {

  //TODO Enforce timeouts.
  //TODO Drop defunct objects.

  if (http_context_pollfdv_rebuild(context)<0) return -1;
  if (!context->pollfdc) {
    if (toms<0) {
      #if !FMN_USE_mswin
        while (1) usleep(1000000);
      #endif
      return -1;
    } else if (!toms) {
      return 0;
    } else {
      #if FMN_USE_mswin
        Sleep(toms);
      #else
        usleep(toms*1000);
      #endif
      return 0;
    }
  }
  
  int err=poll(context->pollfdv,context->pollfdc,toms);
  if (!err) return 0;
  if (err<0) {
    if (errno==EINTR) return 0;
    return err;
  }
  
  const struct pollfd *pollfd=context->pollfdv;
  int i=context->pollfdc;
  for (;i-->0;pollfd++) {
    if (pollfd->revents&(POLLERR|POLLHUP)) {
      if (http_update_fd_error(context,pollfd->fd)<0) return -1;
    } else if (pollfd->revents&POLLIN) {
      if (http_update_fd_read(context,pollfd->fd)<0) return -1;
    } else if (pollfd->revents&POLLOUT) {
      if (http_update_fd_write(context,pollfd->fd)<0) return -1;
    }
  }
  
  return 0;
}
