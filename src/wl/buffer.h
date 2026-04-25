#ifndef WLX_WL_BUFFER_H
#define WLX_WL_BUFFER_H

#include <stdint.h>

#include <xcb/xcb.h>

#include "../server.h"
#include "../utils/list.h"
#include "types.h"

typedef enum wlx_buffer_type {
	WLX_BUFFER_TYPE_NONE = 0,
	WLX_BUFFER_TYPE_SHM,
	WLX_BUFFER_TYPE_DMABUF,
} wlx_buffer_type_t;

typedef struct wlx_buffer_resource {
	list_t			  link;
	wl_resource_t	 *resource;
	wlx_server_t	 *server;
	wlx_buffer_type_t type;

	xcb_pixmap_t x_pixmap;

	int32_t	 width, height;
	int32_t	 stride;
	int32_t	 size;
	int32_t	 offset;
	uint32_t drm_format;
	uint32_t drm_flags;
} wlx_buffer_resource_t;

#endif // WLX_WL_BUFFER_H
