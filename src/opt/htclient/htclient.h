/* htclient.h
 * Simple HTTP client.
 * For trusted remotes only! Don't use against the public internet.
 */
 
#ifndef HTCLIENT_H
#define HTCLIENT_H

struct htclient;
struct htclient_request;
struct htclient_response;
struct htclient_websocket;

void htclient_del(struct htclient *htclient);

/* Each htclient instance is bound to one remote host.
 * That doesn't mean it has just one socket open at a time, 
 * just that every socket we do open is to this host only.
 * Creating htclient instances is pretty cheap, just a DNS lookup for each.
 */
struct htclient *htclient_new(const char *hostname,int hostnamec,int port);

/* To start a request, create an htclient_request instance.
 * Each request you create, you must configure and then commit.
 * If you don't commit it, you must cancel it.
 * Either of those relinquishes ownership of the request object.
 * If commit fails, it will automatically cancel. You hand off ownership either way.
 */
struct htclient_request *htclient_request_new(struct htclient *htclient);
void htclient_request_cancel(struct htclient_request *request);
int htclient_request_commit(struct htclient_request *request);

/* The callback will fire once eventually, even if the request is cancelled.
 * If cancelled or failed, (response) will be null.
 */
void *htclient_request_get_userdata(const struct htclient_request *request);
void htclient_request_set_callback(
  struct htclient_request *request,
  void (*cb)(struct htclient_request *request,struct htclient_response *response),
  void *userdata
);

/* Request preparation.
 * Query and header always append, we don't check whether the key is already defined.
 */
int htclient_request_set_method(struct htclient_request *request,const char *method,int methodc);
int htclient_request_set_path(struct htclient_request *request,const char *path,int pathc);
int htclient_request_add_query(struct htclient_request *request,const char *k,int kc,const char *v,int vc);
int htclient_request_add_header(struct htclient_request *request,const char *k,int kc,const char *v,int vc);
int htclient_request_set_body(struct htclient_request *request,const void *v,int c);

/* Response examination.
 */
int htclient_response_get_status_code(const struct htclient_response *response);
int htclient_response_get_status_message(void *dstpp,const struct htclient_response *response);
int htclient_response_get_header(void *dstpp,const struct htclient_response *response,const char *k,int kc);
int htclient_response_for_each_header(
  const struct htclient_response *response,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
);
int htclient_response_get_body(void *dstpp,const struct htclient_response *response);

/* Connect WebSocket.
 */
struct htclient_websocket *htclient_websocket_connect(struct htclient *htclient,const char *path,int pathc);
void htclient_websocket_close(struct htclient_websocket *websocket);
int htclient_websocket_send(struct htclient_websocket *websocket,int opcode,const void *v,int c);
void htclient_websocket_set_callback(
  struct htclient_websocket *websocket,
  void (*cb)(int opcode,const void *v,int c,void *userdata),
  void *userdata
);

#endif
