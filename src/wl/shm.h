#ifndef WLX_WL_SHM_H
#define WLX_WL_SHM_H

#include <stdint.h>

#include <xcb/shm.h>

#include "../server.h"
#include "../utils/list.h"
#include "types.h"

typedef struct wlx_shm_pool_resource {
	wl_resource_t *resource;
	wlx_server_t  *server;

	int			  fd;
	int32_t		  size;
	xcb_shm_seg_t x_seg;

	list_t buffers;
} wlx_shm_pool_resource_t;

typedef struct wlx_shm_global {
	wl_global_t	 *global;
	wlx_server_t *server;
} wlx_shm_global_t;

int wlx_shm_global_init(wlx_shm_global_t *shm,
						wlx_server_t	 *server);

void wlx_shm_global_free(wlx_shm_global_t *shm);

#endif // WLX_WL_SHM_H
