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

/* Opacity of final framebuffer transfer, 0..1, default 1.
 * This is needed for Gameboy, to simulate how its screen's pixels took longer than one frame to fully change color.
 */
void eh_render_set_pixel_refresh(struct eh_render *render,float pixelRefresh);

#endif
