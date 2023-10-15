/* http_dict.h
 * Dictionary for HTTP headers.
 */
 
#ifndef HTTP_DICT_H
#define HTTP_DICT_H

struct http_dict {
  struct http_dict_entry {
    char *k,*v;
    int kc,vc;
  } *v;
  int c,a;
};

void http_dict_cleanup(struct http_dict *dict);

int http_dict_get(void *dstpp,const struct http_dict *dict,const char *k,int kc);
int http_dict_set(struct http_dict *dict,const char *k,int kc,const char *v,int vc);
void http_dict_clear(struct http_dict *dict);

#endif
