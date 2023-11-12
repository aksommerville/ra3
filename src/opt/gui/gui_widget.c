#include "gui_internal.h"

/* Delete.
 */

void gui_widget_del(struct gui_widget *widget) {
  if (!widget) return;
  if (widget->refc-->1) return;
  if (widget->childv) {
    while (widget->childc>0) {
      widget->childc--;
      struct gui_widget *child=widget->childv[widget->childc];
      child->parent=0;
      gui_widget_del(child);
    }
    free(widget->childv);
  }
  free(widget);
}

/* Retain.
 */
 
int gui_widget_ref(struct gui_widget *widget) {
  if (!widget) return -1;
  if (widget->refc<1) return -1;
  if (widget->refc==INT_MAX) return -1;
  widget->refc++;
  return 0;
}

/* New.
 */
 
struct gui_widget *gui_widget_new(struct gui *gui,const struct gui_widget_type *type) {
  if (!gui||!type) return 0;
  struct gui_widget *widget=calloc(1,type->objlen);
  if (!widget) return 0;
  widget->type=type;
  widget->gui=gui;
  widget->refc=1;
  if (type->init&&(type->init(widget)<0)) {
    gui_widget_del(widget);
    return 0;
  }
  return widget;
}

/* Measure, wrapper.
 */

void gui_widget_measure(int *w,int *h,struct gui_widget *widget,int maxw,int maxh) {
  if (!widget) return;
  int _w,_h;
  if (!w) w=&_w;
  if (!h) h=&_h;
  if (maxw<0) maxw=0;
  if (maxh<0) maxh=0;
  if (widget->type->measure) { // Let the widget measure itself if it can.
    widget->type->measure(w,h,widget,maxw,maxh);
  } else { // Otherwise take the largest child, each axis independently.
    *w=0;
    *h=0;
    int i=widget->childc; while (i-->0) {
      struct gui_widget *child=widget->childv[i];
      int chw,chh;
      gui_widget_measure(&chw,&chh,child,maxw,maxh);
      if (chw>*w) *w=chw;
      if (chh>*h) *h=chh;
    }
  }
  if (*w<0) *w=0;
  else if (*w>maxw) *w=maxw;
  if (*h<0) *h=0;
  else if (*h>maxh) *h=maxh;
}

/* Pack, wrapper.
 */

void gui_widget_pack(struct gui_widget *widget) {
  if (!widget) return;
  if (widget->type->pack) {
    widget->type->pack(widget);
  } else {
    // Default: All children get the parent's full size. Can't imagine you'd ever want this.
    int i=widget->childc; while (i-->0) {
      struct gui_widget *child=widget->childv[i];
      child->x=0;
      child->y=0;
      child->w=widget->w;
      child->h=widget->h;
      gui_widget_pack(child);
    }
  }
}

/* Draw, wrapper.
 */
 
void gui_widget_draw(struct gui_widget *widget,int x,int y) {
  if (!widget) return;
  if (widget->type->draw) {
    widget->type->draw(widget,x,y);
  } else {
    int i=0; for (;i<widget->childc;i++) {
      struct gui_widget *child=widget->childv[i];
      gui_widget_draw(child,x+child->x,y+child->y);
    }
  }
}

/* Spawn child widget.
 */

struct gui_widget *gui_widget_spawn(
  struct gui_widget *parent,
  const struct gui_widget_type *type
) {
  if (!parent||!type) return 0;
  struct gui_widget *child=gui_widget_new(parent->gui,type);
  if (!child) return 0;
  int err=gui_widget_add_child(parent,child);
  gui_widget_del(child);
  if (err<0) return 0;
  return child;
}

struct gui_widget *gui_widget_spawn_insert(
  struct gui_widget *parent,
  int p,
  const struct gui_widget_type *type
) {
  if (!parent||!type) return 0;
  if (p<0) p=0;
  if (p>=parent->childc) return gui_widget_spawn(parent,type);
  struct gui_widget *child=gui_widget_new(parent->gui,type);
  if (!child) return 0;
  int err=gui_widget_insert_child(parent,p,child);
  gui_widget_del(child);
  if (err<0) return 0;
  return child;
}
  
struct gui_widget *gui_widget_spawn_replace(
  struct gui_widget *parent,
  int p,
  const struct gui_widget_type *type
) {
  if (!parent||!type) return 0;
  if ((p<0)||(p>=parent->childc)) return 0;
  struct gui_widget *child=gui_widget_new(parent->gui,type);
  if (!child) return 0;
  struct gui_widget *prev=parent->childv[p];
  int err=gui_widget_insert_child(parent,p,child);
  gui_widget_del(child);
  if (err<0) return 0;
  gui_widget_remove_child(parent,prev);
  return child;
}

/* Test child.
 */

int gui_widget_has_child(const struct gui_widget *parent,const struct gui_widget *child) {
  if (!parent||!child) return 0;
  return (child->parent==parent)?1:0;
}

int gui_widget_is_ancestor(const struct gui_widget *ancestor,const struct gui_widget *descendant) {
  if (!ancestor) return 0;
  for (;descendant;descendant=descendant->parent) {
    if (ancestor==descendant) return 1;
  }
  return 0;
}

/* Add child.
 */
 
static int gui_widget_childv_require(struct gui_widget *widget) {
  if (widget->childc<widget->childa) return 0;
  int na=widget->childa+4;
  if (na>INT_MAX/sizeof(void*)) return -1;
  void *nv=realloc(widget->childv,sizeof(void*)*na);
  if (!nv) return -1;
  widget->childv=nv;
  widget->childa=na;
  return 0;
}

static void gui_widget_childv_shuffle(struct gui_widget *parent,int top,int fromp) {
  if (top==fromp) return;
  if ((top<0)||(top>=parent->childc)) return;
  if ((fromp<0)||(fromp>=parent->childc)) return;
  struct gui_widget *traveller=parent->childv[fromp];
  if (fromp<top) {
    memmove(parent->childv+fromp,parent->childv+fromp+1,sizeof(void*)*(top-fromp));
  } else {
    memmove(parent->childv+top+1,parent->childv+top,sizeof(void*)*(fromp-top));
  }
  parent->childv[top]=traveller;
}

int gui_widget_add_child(struct gui_widget *parent,struct gui_widget *child) {
  if (!parent||!child) return -1;
  if (gui_widget_is_ancestor(child,parent)) return -1;
  if (gui_widget_childv_require(parent)<0) return -1;
  
  if (!child->parent) { // new parent from orphan. likely and easy.
    if (gui_widget_ref(child)<0) return -1;
    parent->childv[parent->childc++]=child;
    child->parent=parent;
    
  } else if (child->parent==parent) { // already belongs to this parent. just check index.
    int fromp=-1,i=parent->childc;
    while (i-->0) {
      if (parent->childv[i]==child) {
        fromp=i;
        break;
      }
    }
    if (fromp<0) return -1;
    if (fromp==parent->childc-1) return 0;
    memmove(parent->childv+fromp,parent->childv+fromp+1,sizeof(void*)*(parent->childc-fromp-1));
    parent->childv[parent->childc-1]=child;
    
  } else { // belongs to some other parent.
    if (gui_widget_ref(child)<0) return -1;
    gui_widget_remove_child(child->parent,child);
    parent->childv[parent->childc++]=child;
    child->parent=parent;
  }
  return 0;
}
    
int gui_widget_insert_child(struct gui_widget *parent,int p,struct gui_widget *child) {
  if (!parent||!child) return -1;
  if ((p<0)||(p>parent->childc)) return -1;
  
  if (parent==child->parent) {
    int fromp=-1,i=parent->childc;
    while (i-->0) {
      if (parent->childv[i]==child) {
        fromp=i;
        break;
      }
    }
    if (fromp<0) return -1;
    gui_widget_childv_shuffle(parent,p,fromp);
    return 0;
  }
  
  if (gui_widget_add_child(parent,child)<0) return -1;
  return gui_widget_insert_child(parent,p,child); // this time, parent==child->parent
}

/* Remove child.
 */
 
void gui_widget_remove_child(struct gui_widget *parent,struct gui_widget *child) {
  if (!parent||!child) return;
  if (child->parent!=parent) return;
  int i=parent->childc;
  while (i-->0) {
    if (parent->childv[i]!=child) continue;
    parent->childc--;
    memmove(parent->childv+i,parent->childv+i+1,sizeof(void*)*(parent->childc-i));
    child->parent=0;
    gui_widget_del(child);
    return;
  }
}
