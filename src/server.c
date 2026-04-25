#include "server.h"
#include "server_priv.h"

#include <stdlib.h>
#include <string.h>

#include <xcb/randr.h>

xcb_connection_t *wlx_server_get_xcb(wlx_server_t *server)
{
	return server->x_display;
}

xcb_window_t wlx_server_get_xroot(wlx_server_t *server)
{
	return server->x_screenp->root;
}

uint8_t wlx_server_get_xroot_depth(wlx_server_t *server)
{
	return server->x_screenp->root_depth;
}

xid_map_t *wlx_server_get_xid_map(wlx_server_t *server)
{
	return &server->xid_map;
}

x_ext_manager_t *wlx_server_get_ext_manager(wlx_server_t *server)
{
	return &server->x_ext_manager;
}

wl_display_t *wlx_server_get_wl(wlx_server_t *server)
{
	return server->wl_display;
}

static void wlx_server_handle_x_event(wlx_server_t		  *server,
									  xcb_generic_event_t *event)
{}

int wlx_server_init_wl(wlx_server_t *server)
{
	if (!(server->wl_display = wl_display_create()))
		return 1;

	if (!(server->sock_name = wl_display_add_socket_auto(server->wl_display)))
		return 1;

	// init wayland globals/managers

	return 0;
}

int wlx_server_init_x(wlx_server_t *server)
{
	xid_map_init(&server->xid_map);

	int screenp;
	server->x_display = xcb_connect(NULL, &screenp);
	if (xcb_connection_has_error(server->x_display))
		return 1;

	server->x_setup = xcb_get_setup(server->x_display);
	xcb_screen_iterator_t screen_iter = xcb_setup_roots_iterator(server->x_setup);
	for (int i = 0; i < screenp; i++)
		xcb_screen_next(&screen_iter);

	server->x_screenp = screen_iter.data;
	x_ext_manager_init(&server->x_ext_manager, server);
	return 0;
}

int wlx_server_init(wlx_server_t *server)
{
	if (wlx_server_init_x(server) || wlx_server_init_wl(server)) {
		wlx_server_close(server);
		return 1;
	}

	server->running = true;
	return 0;
}

int wlx_server_run(wlx_server_t *server)
{
	wl_event_loop_t		*event_loop = wl_display_get_event_loop(server->wl_display);
	xcb_generic_event_t *x_event;
	while (server->running) {
		wl_display_flush_clients(server->wl_display);
		if (!wl_event_loop_dispatch(event_loop, -1)) {
			return 1;
		}

		if ((x_event = xcb_poll_for_event(server->x_display))) {
			wlx_server_handle_x_event(server, x_event);
			free(x_event);
		}
	}

	return 0;
}

int wlx_server_start(wlx_server_t *server)
{
	if (!wlx_server_init(server)) {
		wlx_server_run(server);
		return 0;
	}

	return 1;
}

int wlx_server_close(wlx_server_t *server)
{
	xid_map_release(&server->xid_map);
	xcb_disconnect(server->x_display);

	// free wayland globals/holders

	wl_display_destroy_clients(server->wl_display);
	wl_display_destroy(server->wl_display);
	return 0;
}
