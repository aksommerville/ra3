/* gui.h
 * Generic joystick-driven GUI.
 * Used only by our menu app, but who knows maybe other clients will want it in the future?
 * For now, we assume a direct OpenGL context.
 */
 
#ifndef GUI_H
#define GUI_H

#include <stdint.h>

struct gui;
struct gui_widget;
struct gui_widget_type;

/* Client can implement sound effects.
 * We call your delegate when an effect should play.
 * IDs >=GUI_SFXID_USER will never be used by widgets owned by gui core.
 */
#define GUI_SFXID_MOTION 1
#define GUI_SFXID_ACTIVATE 2
#define GUI_SFXID_CANCEL 3
#define GUI_SFXID_REJECT 4
#define GUI_SFXID_MODAL_UP 5
#define GUI_SFXID_MODAL_DOWN 6
#define GUI_SFXID_USER 100

/* Stateless signals we can send to a widget.
 */
#define GUI_SIGID_REMOVE 1
#define GUI_SIGID_ACTIVATE 2
#define GUI_SIGID_CANCEL 3
#define GUI_SIGID_AUX 4

/* Global context.
 **********************************************************************/
 
struct gui_delegate {
  void *userdata;
  void (*cb_sound_effect)(int sfxid,void *userdata);
};

void gui_del(struct gui *gui);
struct gui *gui_new(const struct gui_delegate *delegate);

/* The root widget is just a container for the one "page" widget and multiple "modal" widgets.
 */
struct gui_widget *gui_get_root(const struct gui *gui);

/* Page and modal widgets.
 * These are really root's concern, not global's, but we provide API for convenience.
 */
struct gui_widget *gui_replace_page(struct gui *gui,const struct gui_widget_type *type); // => WEAK
struct gui_widget *gui_push_modal(struct gui *gui,const struct gui_widget_type *type); // => WEAK
void gui_dismiss_modal(struct gui *gui,struct gui_widget *modal);
void gui_dismiss_top_modal(struct gui *gui);
void gui_dismiss_all_modals(struct gui *gui);
struct gui_widget *gui_get_page(const struct gui *gui);
struct gui_widget *gui_get_modal_by_index(const struct gui *gui,int p); // 0 is the top, 1 below it, etc

/* Call each video frame.
 * (input) should come from eh_input_get(0).
 */
void gui_update(struct gui *gui,uint16_t input);

/* Call from within the OpenGL context, and provide the main output size.
 * If size changes, this triggers a global repack.
 * We might render nothing -- dirty first if you need to for some reason.
 */
void gui_render(struct gui *gui,int w,int h);
void gui_dirty_video(struct gui *gui); // Force render next time.
void gui_dirty_pack(struct gui *gui); // Force repack next time.

/* Generic widget.
 ********************************************************************/
 
struct gui_widget_type {
  const char *name;
  int objlen;
  void (*del)(struct gui_widget *widget);
  int (*init)(struct gui_widget *widget);
  
  /* At (measure), report your ideal dimensions.
   * At (pack), your dimensions have been established and you must not change them.
   * Pack all your children.
   * Both of these have wrappers that add some validation and defaults.
   */
  void (*measure)(int *w,int *h,struct gui_widget *widget,int maxw,int maxh);
  void (*pack)(struct gui_widget *widget);
  
  /* Widget must ignore its own position and draw itself at (x,y) in the main output.
   */
  void (*draw)(struct gui_widget *widget,int x,int y);
  
  /* The root widget is interested in update_changed.
   * Others probably want the digested events.
   */
  void (*update)(struct gui_widget *widget);
  void (*input_changed)(struct gui_widget *widget,uint16_t input);
  void (*motion)(struct gui_widget *widget,int dx,int dy);
  void (*signal)(struct gui_widget *widget,int sigid);
};

struct gui_widget {
  const struct gui_widget_type *type;
  int refc;
  struct gui *gui; // WEAK, REQUIRED
  struct gui_widget *parent; // WEAK, OPTIONAL
  struct gui_widget **childv; // STRONG
  int childc,childa;
  int x,y,w,h; // (x,y) relative to parent
  int opaque; // Hint that this widget will completely cover its bounds at draw.
};

void gui_widget_del(struct gui_widget *widget);
int gui_widget_ref(struct gui_widget *widget);
struct gui_widget *gui_widget_new(struct gui *gui,const struct gui_widget_type *type);

void gui_widget_measure(int *w,int *h,struct gui_widget *widget,int maxw,int maxh);
void gui_widget_pack(struct gui_widget *widget);
void gui_widget_draw(struct gui_widget *widget,int x,int y);

/* Create widget, append to (parent), and return WEAK reference.
 * This should be the normal way of creating widgets.
 */
struct gui_widget *gui_widget_spawn(
  struct gui_widget *parent,
  const struct gui_widget_type *type
);
struct gui_widget *gui_widget_spawn_insert(
  struct gui_widget *parent,
  int p,
  const struct gui_widget_type *type
);
struct gui_widget *gui_widget_spawn_replace(
  struct gui_widget *parent,
  int p,
  const struct gui_widget_type *type
);

/* Adding child breaks any existing connection, and puts child at the end of parent's list.
 * Modifying child lists with these hooks does not fire the REMOVE signal; caller should do that if it's a real removal.
 * For the ancestry test, a widget *is* considered its own ancestor.
 */
int gui_widget_has_child(const struct gui_widget *parent,const struct gui_widget *child);
int gui_widget_is_ancestor(const struct gui_widget *ancestor,const struct gui_widget *descendant);
int gui_widget_add_child(struct gui_widget *parent,struct gui_widget *child);
int gui_widget_insert_child(struct gui_widget *parent,int p,struct gui_widget *child);
void gui_widget_remove_child(struct gui_widget *parent,struct gui_widget *child);

/* Widget types.
 * Client should add its own types, mostly at screen or modal scope.
 *****************************************************************/
 
extern const struct gui_widget_type gui_widget_type_root;

extern const struct gui_widget_type gui_widget_type_packer; // single-axis alignment helper
extern const struct gui_widget_type gui_widget_type_label; // image or single-line text
extern const struct gui_widget_type gui_widget_type_text; // multi-line text

/* Rendering primitives.
 ***********************************************************/

// Should be internal use only.
void gui_prepare_render(struct gui *gui);
 
void gui_draw_rect(struct gui *gui,int x,int y,int w,int h,uint32_t rgba);
void gui_draw_line(struct gui *gui,int ax,int ay,int bx,int by,uint32_t rgba);

#endif
