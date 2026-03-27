#ifndef WLX_WL_SURFACE_H
#define WLX_WL_SURFACE_H

#include "../utils/list.h"
#include "buffer.h"
#include "region.h"
#include "types.h"

typedef enum wlx_surface_role {
	WLX_SURFACE_ROLE_NONE = 0,
	WLX_SURFACE_ROLE_XDG_TOPLEVEL,
	WLX_SURFACE_ROLE_XDG_POPUP,
	WLX_SURFACE_ROLE_WLR_LAYER,
} wlx_surface_role_t;

typedef enum wlx_surface_damage {
	WLX_SURFACE_DAMAGE_NONE = 0,
	WLX_SURFACE_DAMAGE_SURFACE,
	WLX_SURFACE_DAMAGE_BUFFER,
} wlx_surface_damage_t;

typedef struct wlx_surface_resource {
	list_t		   link;
	wl_resource_t *resource;

	wlx_surface_role_t role;
	wl_resource_t	  *role_resource;

	xcb_window_t x_window;

	// double buffered, variables are moved from index-0
	// to index-1 when commited
	wlx_buffer_resource_t *buffer_attached[2];
	int32_t				   buffer_x[2], buffer_y[2];
	int32_t				   buffer_transform[2];
	int32_t				   buffer_scale[2];
	wlx_surface_damage_t   damage_type[2];
	int32_t				   damage_x[2], damage_y[2];
	int32_t				   damage_width[2], damage_height[2];
	wl_resource_t		  *frame_callback[2];
	wlx_region_resource_t *opaque_region[2];
	wlx_region_resource_t *input_region[2];
} wlx_surface_resource_t;

#endif // WLX_WL_SURFACE_H
