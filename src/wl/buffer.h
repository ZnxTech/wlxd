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
	int32_t	 fd;
	void	*mmem;
	int32_t	 size;
	uint32_t format;
} wlx_buffer_shm_t;

typedef struct wlx_buffer_dmabuf {
	int32_t	 fd;
	uint32_t drm_format;
	uint32_t flags;
} wlx_buffer_dmabuf_t;

typedef struct wlx_buffer_resource {
	list_t			  link;
	wl_resource_t	 *resource;
	wlx_buffer_type_t type;

	int32_t width, height;
	int32_t stride;

	union {
		wlx_buffer_shm_t	shm;
		wlx_buffer_dmabuf_t dmabuf;
	};
} wlx_buffer_resource_t;

#endif
