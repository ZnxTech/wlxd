#ifndef WLX_DMABUF_H
#define WLX_DMABUF_H

#include <stdint.h>

#include "../server.h"
#include "../utils/array.h"
#include "../utils/list.h"
#include "types.h"

#define WLX_DMABUF_MAX_PLANES 4

typedef struct wlx_dmabuf_global wlx_dmabuf_global_t;

typedef struct wlx_dmabuf_feedback_tranche {
	list_t link;

	dev_t	 dev;
	uint32_t flags;
	array_t	 indices;
} wlx_dmabuf_feedback_tranche_t;

typedef struct wlx_dmabuf_feedback_resource {
	wl_resource_t		*resource;
	wlx_dmabuf_global_t *dmabuf;
	wlx_server_t		*server;

	dev_t  main_dev;
	bool   main_dev_failed;
	int	   table_fd;
	size_t table_size;
	list_t tranches;
} wlx_dmabuf_feedback_resource_t;

typedef struct wlx_dmabuf_params_resource {
	wl_resource_t		*resource;
	wlx_dmabuf_global_t *dmabuf;
	wlx_server_t		*server;

	bool used;

	uint64_t drm_modifier;
	bool	 has_modifier;

	uint8_t	 planen;
	int		 fds[WLX_DMABUF_MAX_PLANES];
	uint32_t offsets[WLX_DMABUF_MAX_PLANES];
	uint32_t strides[WLX_DMABUF_MAX_PLANES];
} wlx_dmabuf_params_resource_t;

typedef struct wlx_dmabuf_table_entry {
	uint32_t drm_format;
	uint32_t pad; // unused
	uint64_t drm_modifier;
} wlx_dmabuf_table_entry_t;

typedef struct wlx_dmabuf_global {
	wl_global_t	 *global;
	wlx_server_t *server;

	array_t table;
	int		table_fd;
	array_t indices;
} wlx_dmabuf_global_t;

int wlx_dmabuf_init(wlx_dmabuf_global_t *dmabuf,
					wlx_server_t		*server);

void wlx_dmabuf_free(wlx_dmabuf_global_t *dmabuf);

#endif // WLX_DMABUF_H
