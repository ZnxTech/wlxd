#ifndef WLX_WL_REGION_H
#define WLX_WL_REGION_H

#include "../utils/array.h"
#include "../utils/list.h"
#include "types.h"

typedef struct wlx_region_rect {
	int32_t x, y;
	int32_t width, height;
	bool	add;
} wlx_region_rect_t;

typedef struct wlx_region_resource {
	list_t		   link;
	wl_resource_t *resource;

	array_t rects;
} wlx_region_resource_t;

#endif // WLX_WL_REGION_H
