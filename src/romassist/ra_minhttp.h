/* ra_minhttp.h
 * Minimal HTTP client.
 * Our full "http" unit has a client API declared but not implemented.
 * For today's uses, I don't need all that ceremony, just a quick and dirty single-socket synchronous kind of deal.
 */
 
#ifndef RA_MINHTTP_H
#define RA_MINHTTP_H

struct sr_encoder;

struct ra_minhttp {
  int fd;
};

void ra_minhttp_cleanup(struct ra_minhttp *mh);

int ra_minhttp_connect(struct ra_minhttp *mh,const char *host_and_port);

/* Compose and send request, and await full response.
 * Returns HTTP status. Populates (rspheaders,rspbody) with response, if not null.
 */
int ra_minhttp_request_sync(
  struct sr_encoder *rspheaders,
  struct sr_encoder *rspbody,
  struct ra_minhttp *mh,
  const char *method,
  const char *path
);

// If you didn't write a request first, you'll be waiting forever.
int ra_minhttp_await_response(
  struct sr_encoder *rspheaders,
  struct sr_encoder *rspbody,
  struct ra_minhttp *mh
);

#endif
