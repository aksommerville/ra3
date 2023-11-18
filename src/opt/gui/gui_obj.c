#include "gui_internal.h"

/* Delete.
 */

void gui_del(struct gui *gui) {
  if (!gui) return;
  gui_widget_del(gui->root);
  gui_text_quit(gui);
  free(gui);
}

/* New.
 */
 
struct gui *gui_new(const struct gui_delegate *delegate) {
  struct gui *gui=calloc(1,sizeof(struct gui));
  if (delegate) gui->delegate=*delegate;
  if (gui_text_init(gui)<0) {
    gui_del(gui);
    return 0;
  }
  if (!(gui->root=gui_widget_new(gui,&gui_widget_type_root))) return 0;
  return gui;
}

/* Accessors to root widget.
 */

struct gui_widget *gui_get_root(const struct gui *gui) {
  return gui->root;
}

struct gui_widget *gui_replace_page(struct gui *gui,const struct gui_widget_type *type) {
  struct gui_widget *prev=gui->root->childv[0];
  if (gui_widget_ref(prev)<0) return 0;
  struct gui_widget *next=gui_widget_spawn_replace(gui->root,0,type);
  if (!next) {
    gui_widget_del(prev);
    return 0;
  }
  if (prev->type->signal) prev->type->signal(prev,GUI_SIGID_REMOVE);
  gui_widget_del(prev);
  return next;
}

struct gui_widget *gui_push_modal(struct gui *gui,const struct gui_widget_type *type) {
  return gui_widget_spawn(gui->root,type);
}

void gui_dismiss_modal(struct gui *gui,struct gui_widget *modal) {
  int i=gui->root->childc;
  while (i-->1) {
    if (gui->root->childv[i]==modal) {
      if (modal->type->signal) modal->type->signal(modal,GUI_SIGID_REMOVE);
      gui_widget_remove_child(gui->root,modal);
      return;
    }
  }
}

void gui_dismiss_top_modal(struct gui *gui) {
  if (gui->root->childc<2) return;
  struct gui_widget *modal=gui->root->childv[gui->root->childc-1];
  if (modal->type->signal) modal->type->signal(modal,GUI_SIGID_REMOVE);
  gui_widget_remove_child(gui->root,modal);
}

void gui_dismiss_all_modals(struct gui *gui) {
  while (gui->root->childc>1) {
    struct gui_widget *modal=gui->root->childv[gui->root->childc-1];
    if (modal->type->signal) modal->type->signal(modal,GUI_SIGID_REMOVE);
    gui_widget_remove_child(gui->root,modal);
  }
}

struct gui_widget *gui_get_page(const struct gui *gui) {
  return gui->root->childv[0];
}

struct gui_widget *gui_get_modal_by_index(const struct gui *gui,int p) {
  if (p<0) return 0;
  p++;
  if (p>=gui->root->childc) return 0;
  return gui->root->childv[p];
}

void gui_update(struct gui *gui,uint16_t input) {
  if (input!=gui->pvinput) {
    gui->pvinput=input;
    if (gui->root->type->input_changed) gui->root->type->input_changed(gui->root,input);
  }
  if (gui->root->type->update) gui->root->type->update(gui->root);
}

/* Render.
 */

void gui_render(struct gui *gui,int w,int h) {
  if ((w<1)||(h<1)) return;
  if ((w!=gui->pvw)||(h!=gui->pvh)) {
    gui->pvw=w;
    gui->pvh=h;
    gui->root->w=w;
    gui->root->h=h;
    gui_widget_pack(gui->root);
    gui->video_dirty=1;
  }
  //if (!gui->video_dirty) return;//TODO I guess we can't choose to not render from this depth. Would it be possible from higher up?
  gui->video_dirty=0;
  gui_prepare_render(gui);
  gui_widget_draw(gui->root,0,0);
}

void gui_dirty_video(struct gui *gui) {
  gui->video_dirty=1;
}

void gui_dirty_pack(struct gui *gui) {
  gui->pvw=0;
  gui->pvh=0;
}

void gui_get_screen_size(int *w,int *h,struct gui *gui) {
  *w=gui->pvw;
  *h=gui->pvh;
}
