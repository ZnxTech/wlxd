#ifndef WL_BUFFER_H
#define WL_BUFFER_H

#include <stdint.h>

#include <xcb/xcb.h>

#include "../utils/list.h"
#include "types.h"

typedef enum wlx_buffer_type {
	WLX_BUFFER_TYPE_NONE = 0,
	WLX_BUFFER_TYPE_SHM,
	WLX_BUFFER_TYPE_DMABUF,
} wlx_buffer_type_t;

typedef struct wlx_buffer_shm {
	int32_t fd;
	void   *mmem;
} wlx_buffer_shm_t;

typedef struct wlx_buffer_dmabuf {
	uint32_t flags;
} wlx_buffer_dmabuf_t;

typedef struct wlx_buffer_resource {
	list_t			  link;
	wl_resource_t	 *resource;
	wlx_buffer_type_t type;

	xcb_pixmap_t x_pixmap;

	int32_t	 width, height;
	int32_t	 stride;
	int32_t	 size;
	uint32_t drm_format;

	union {
		wlx_buffer_shm_t	shm;
		wlx_buffer_dmabuf_t dmabuf;
	};
} wlx_buffer_resource_t;

#endif
