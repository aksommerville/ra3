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
#define GUI_SIGID_FOCUS 5
#define GUI_SIGID_BLUR 6

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
void gui_modal_place_near(struct gui_widget *modal,struct gui_widget *anchor); // eg for popup menus. (anchor) must be in the page, or a lower modal, can't be above.

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

// Available during render, same as what as passed to gui_render().
void gui_get_screen_size(int *w,int *h,struct gui *gui);

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
  struct gui_widget *modal_anchor; // private to root
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

//XXX I didn't make these yet, and not sure i'm going to...
extern const struct gui_widget_type gui_widget_type_packer; // single-axis alignment helper
extern const struct gui_widget_type gui_widget_type_label; // image or single-line text
extern const struct gui_widget_type gui_widget_type_text; // multi-line text

extern const struct gui_widget_type gui_widget_type_button; // text label you can select and actuate
extern const struct gui_widget_type gui_widget_type_pickone; // modal controller for single-select list (dropdown menu)
extern const struct gui_widget_type gui_widget_type_entry; // single-line text suitable for a modal, contains its own keyboard
extern const struct gui_widget_type gui_widget_type_keyboard; // joystick-accessible keyboard
extern const struct gui_widget_type gui_widget_type_form; // plain key=value form

void gui_root_place_modal_near(struct gui_widget *widget,struct gui_widget *modal,struct gui_widget *anchor);

struct gui_widget *gui_widget_button_spawn(
  struct gui_widget *parent,
  const char *label,int labelc,
  int rgb,
  void (*cb)(struct gui_widget *button,void *userdata),
  void *userdata
);
int gui_widget_button_set_label(struct gui_widget *widget,const char *src,int srcc,int rgb);
void gui_widget_button_enable(struct gui_widget *widget,int enable);
int gui_widget_button_get_enable(const struct gui_widget *widget);
int gui_widget_button_get_text(void *dstpp,const struct gui_widget *widget);

void gui_widget_pickone_set_callback(struct gui_widget *widget,void (*cb)(struct gui_widget *pickone,int p,void *userdata),void *userdata);
struct gui_widget *gui_widget_pickone_add_option(struct gui_widget *widget,const char *label,int labelc);
void gui_widget_pickone_focus(struct gui_widget *widget,struct gui_widget *option);

int gui_widget_entry_setup(
  struct gui_widget *widget,
  const char *v,int c,
  void (*cb)(struct gui_widget *widget,const char *v,int c,void *userdata),
  void *userdata
);

void gui_widget_keyboard_set_callback(struct gui_widget *widget,void (*cb)(struct gui_widget *keyboard,int codepoint,void *userdata),void *userdata);

/* (custom) nonzero to get a callback with (v==0) and (vc==-1) on actuation.
 * Otherwise the form presents it own entry modal and only notifies you when committed.
 * There are "_json" versions as a convenience, if the incoming value is an encoded JSON string. Doesn't affect reporting.
 */
struct gui_widget *gui_widget_form_add_string(struct gui_widget *widget,const char *k,int kc,const char *v,int vc,int custom);
struct gui_widget *gui_widget_form_add_int(struct gui_widget *widget,const char *k,int kc,int v,int custom);
struct gui_widget *gui_widget_form_add_readonly_string(struct gui_widget *widget,const char *k,int kc,const char *v,int vc);
struct gui_widget *gui_widget_form_add_readonly_int(struct gui_widget *widget,const char *k,int kc,int v);
struct gui_widget *gui_widget_form_add_string_json(struct gui_widget *widget,const char *k,int kc,const char *v,int vc,int custom);
struct gui_widget *gui_widget_form_add_readonly_string_json(struct gui_widget *widget,const char *k,int kc,const char *v,int vc);
struct gui_widget *gui_widget_form_add_custom(struct gui_widget *widget,const char *k,int kc,const struct gui_widget_type *type);
void gui_widget_form_set_callback(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *widget,const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
);
struct gui_widget *gui_widget_form_get_button_by_key(struct gui_widget *widget,const char *k,int kc);

/* Text support.
 ********************************************************/
 
struct gui_font;

/* Prepare a font for use.
 * The set of available fonts is established at initialization.
 * Returns a WEAK reference.
 */
struct gui_font *gui_font_get(struct gui *gui,const char *name,int namec);

// Clients should not use this; it happens automatically at init.
struct gui_font *gui_font_add(struct gui *gui,const char *name,int namec);

struct gui_font *gui_font_get_default(struct gui *gui);

/* Rendering primitives.
 ***********************************************************/
 
struct gui_program;
struct gui_texture;

void gui_program_del(struct gui_program *program);
int gui_program_ref(struct gui_program *program);

/* (name) must be a static string, we borrow it long term.
 */
struct gui_program *gui_program_new(const char *name,const char *vsrc,int vsrcc,const char *fsrc,int fsrcc);

void gui_program_use(struct gui_program *program);

unsigned int gui_program_get_uniform(const struct gui_program *program,const char *name);

/* We always check for these three uniforms, but beyond that they are entirely up to you to use.
 */
void gui_program_set_screensize(struct gui_program *program,int w,int h);
void gui_program_set_texsize(struct gui_program *program,int w,int h);
void gui_program_set_texsize_obj(struct gui_program *program,struct gui_texture *texture);
void gui_program_set_sampler(struct gui_program *program,int sampler);

void gui_texture_del(struct gui_texture *texture);
int gui_texture_ref(struct gui_texture *texture);

struct gui_texture *gui_texture_new();

void gui_texture_set_filter(struct gui_texture *texture,int filter);
void gui_texture_get_size(int *w,int *h,const struct gui_texture *texture);

int gui_texture_alloc(struct gui_texture *texture,int w,int h,int alpha);
int gui_texture_upload_rgba(struct gui_texture *texture,int w,int h,const void *src);

void gui_texture_use(struct gui_texture *texture);

/* Null (font) is OK to use the gui's default font, which always exists.
 */
struct gui_texture *gui_texture_from_text(struct gui *gui,struct gui_font *font,const char *src,int srcc,int rgb);
struct gui_texture *gui_texture_from_multiline_text(struct gui *gui,struct gui_font *font,const char *src,int srcc,int rgb,int wlimit);

// Should be internal use only.
void gui_prepare_render(struct gui *gui);
 
void gui_draw_rect(struct gui *gui,int x,int y,int w,int h,uint32_t rgba);
void gui_draw_line(struct gui *gui,int ax,int ay,int bx,int by,uint32_t rgba);
void gui_frame_rect(struct gui *gui,int x,int y,int w,int h,uint32_t rgba);
void gui_draw_texture(struct gui *gui,int x,int y,struct gui_texture *texture);

#endif
