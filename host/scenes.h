/* scenes.h - preview / test scenes (see scenes.c). */
#ifndef SCENES_H
#define SCENES_H

#include "rt_theme.h"

enum { SCENE_WINDOW, SCENE_INACTIVE, SCENE_DIALOG, SCENE_MENU, SCENE_BUTTONS, SCENE_COUNT };

int         scene_from_name(const char *name);
const char *scene_name(int scene);
/* Renders a scene into a new surface (free with sw_surface_free). */
RtSurface  *scene_render(const RtTheme *th, int scene);

#endif
