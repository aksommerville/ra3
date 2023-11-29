/* eh_drm.h
 * Linux Direct Rendering Manager.
 */
 
#ifndef EH_DRM_H
#define EH_DRM_H

void eh_drm_quit();
int eh_drm_init(const char *device);
int eh_drm_swap();

#endif
