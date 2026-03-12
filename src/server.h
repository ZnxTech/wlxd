#ifndef WL_SERVER_H
#define WL_SERVER_H

#include <wayland-server.h>
#include <xcb/xcb.h>

#include "utils/xid_map.h"

#include "wl/compositor.h"
#include "wl/output.h"
#include "wl/types.h"

#include "x/extension.h"

typedef struct wlx_server {
	bool				 running;
	x_extension_holder_t x_ext_holder;
	xid_map_t			 xid_map;

	// -------- wayland variables --------
	wl_display_t *wl_display;
	const char	 *sock_name;
	int			  sock_fd;

	wlx_output_holder_t output_holder;

	// -------- x11 variables --------
	xcb_connection_t  *x_display;
	const xcb_setup_t *x_setup;
	xcb_screen_t	  *x_screenp;
} wlx_server_t;

int wlx_server_init(wlx_server_t *server);

int wlx_server_run(wlx_server_t *server);

int wlx_server_start(wlx_server_t *server);

int wlx_server_close(wlx_server_t *server);

#endif // WL_SERVER_H
