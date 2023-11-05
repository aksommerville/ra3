/* fakews.h
 * Fake WebSocket Client.
 *
 * This is a protocol that only Romassist uses, same idea as WebSocket but less ceremony.
 * Client connects to the server and sends a one-line request:
 *   FAKEWEBSOCKET /path/to/resource HTTP/1.1
 * Then client and server can each send packets at will:
 *   u8  opcode
 *   u16 len
 *   ... payload
 * No packet fragmentation.
 *
 * A fakews instance is bound to one remote host and port.
 * The instance is alive and valid whether connected or not.
 * When disconnected, we will automatically try to reconnect periodically.
 * New instances are disconnected until the first update or explicit connection.
 */
 
#ifndef FAKEWS_H
#define FAKEWS_H

struct fakews;

void fakews_del(struct fakews *fakews);

struct fakews *fakews_new(
  const char *host,int hostc,
  int port,
  const char *path,int pathc,
  void (*cb_connect)(void *userdata),
  void (*cb_disconnect)(void *userdata),
  void (*cb_message)(int opcode,const void *v,int c,void *userdata),
  void *userdata
);

int fakews_update(struct fakews *fakews,int toms);

int fakews_is_connected(const struct fakews *fakews);
int fakews_connect_now(struct fakews *fakews);

/* Fails if disconnected.
 * Success means the packet is queued.
 * We do not provide feedback on whether a packet gets delivered or not.
 */
int fakews_send(struct fakews *fakews,int opcode,const void *v,int c);

#endif
