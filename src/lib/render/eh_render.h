/* eh_render.h
 * Manages all the OpenGL stuff.
 */
 
#ifndef EH_RENDER_H
#define EH_RENDER_H

struct eh_render;

void eh_render_del(struct eh_render *render);

/* (eh.video) must exist before calling.
 */
struct eh_render *eh_render_new();

void eh_render_bounds_dirty(struct eh_render *render);

/* Call from outside the full set of client updates for one frame.
 * Render will tolerate clients trying to draw multiple frames.
 */
void eh_render_before(struct eh_render *render);
void eh_render_after(struct eh_render *render);

#endif
