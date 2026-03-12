#ifndef WLX_WL_COMPOSITOR_H
#define WLX_WL_COMPOSITOR_H

#include "../utils/list.h"
#include "types.h"

typedef struct wlx_server wlx_server_t;

typedef struct wlx_compositor_global {
	wl_global_t	 *global;
	wlx_server_t *server;
	list_t		  regions;
	list_t		  surfaces;
} wlx_compositor_global_t;

int wlx_compositor_init(wlx_compositor_global_t *compositor,
						wlx_server_t			*server);

void wlx_compositor_free(wlx_compositor_global_t *compositor);

#endif // WLX_WL_COMPOSITOR_H
