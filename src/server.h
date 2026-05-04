#ifndef WLX_SERVER_H
#define WLX_SERVER_H

#include <wayland-server.h>
#include <wayland-util.h>
#include <xcb/xcb.h>

#include "wl/types.h"

typedef struct wlx_server wlx_server_t;

xcb_connection_t *wlx_server_get_xcb(wlx_server_t *server);

xcb_window_t wlx_server_get_xroot(wlx_server_t *server);

uint8_t wlx_server_get_xroot_depth(wlx_server_t *server);

wl_display_t *wlx_server_get_wl(wlx_server_t *server);

#define wlx_get_server(wlx_object) \
    (wlx_server_t *)((uint8_t *)(wlx_object) + offsetof(typeof(*wlx_object), server))

#define wlx_get_xcb(wlx_object) \
    wlx_server_get_xcb(wlx_get_server(wlx_object))

#define wlx_get_xroot(wlx_object) \
    wlx_server_get_xroot(wlx_get_server(wlx_object))

#define wlx_get_xroot_depth(wlx_object) \
    wlx_server_get_xroot_depth(wlx_get_server(wlx_object))

#define wlx_get_wl(wlx_object) \
    wlx_server_get_wl(wlx_get_server(wlx_object))

#endif // WLX_SERVER_PRIV_H
