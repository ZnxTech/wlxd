#ifndef WLX_X_EXTENSION_H
#define WLX_X_EXTENSION_H

#include <stdint.h>
#include <xcb/xcb.h>

#include "../server.h"

typedef struct x_ext {
	uint8_t present;
	uint8_t opcode;
	uint8_t f_event;
	uint8_t f_error;
} x_ext_t;

typedef struct x_ext_manager {
	wlx_server_t *server;

	x_ext_t dri3;
	x_ext_t present;
	x_ext_t randr;
	x_ext_t shm;
	x_ext_t xfixes;
} x_ext_manager_t;

x_ext_manager_t *wlx_server_get_ext_manager(wlx_server_t *server);

#define wlx_get_server(wlx_object) \
    (wlx_server_t *)((uint8_t *)(wlx_object) + offsetof(typeof(*wlx_object), server))

#define wlx_get_ext_manager(wlx_object) \
    wlx_server_get_ext_manager(wlx_get_server(wlx_object))

void x_ext_manager_init(x_ext_manager_t *x_ext_manager,
						wlx_server_t	*server);

#endif // WLX_X_EXTENSION_H
