#ifndef WLX_WL_OUTPUT_H
#define WLX_WL_OUTPUT_H

#include <xcb/randr.h>
#include <xcb/xcb.h>

#include "../server.h"
#include "../utils/list.h"
#include "types.h"

typedef struct wlx_output_mode {
	list_t		  link;
	wlx_server_t *server;

	xcb_randr_mode_t xid;
	int32_t			 mode;
	int32_t			 width_pix, height_pix;
	int32_t			 refresh_mhz;
} wlx_output_mode_t;

typedef struct wlx_output_crtc {
	list_t		  link;
	wlx_server_t *server;

	wlx_output_mode_t *mode;

	xcb_randr_crtc_t xid;
	int32_t			 logical_x, logical_y;
	int32_t			 logical_width_pix, logical_height_pix;
	int32_t			 transform;
} wlx_output_crtc_t;

typedef struct wlx_output_global_resource {
	list_t		 link;
	wl_global_t *global;

	wl_resource_t *resource;
} wlx_output_global_resource_t;

typedef struct wlx_output_global {
	list_t		  link;
	wlx_server_t *server;

	wlx_output_mode_t *mode; // TODO: multi mode handling for wlr output managment
	wlx_output_crtc_t *crtc;
	list_t			   resources;

	xcb_randr_output_t xid;
	wl_global_t		  *global;
	int32_t			   width_phy, height_phy;
	int32_t			   scale_factor;
	int32_t			   subpixel_order;

	char *make, *model, *name, *desc;
} wlx_output_global_t;

typedef struct wlx_output_manager {
	wlx_server_t *server;

	xcb_timestamp_t config_timestamp;
	list_t			modes;
	list_t			crtcs;
	list_t			outputs;
} wlx_output_manager_t;

int wlx_output_manager_init(wlx_output_manager_t *manager,
							wlx_server_t		 *server);

void wlx_output_manager_free(wlx_output_manager_t *manager);

void wlx_output_manager_handle_event(wlx_output_manager_t *manager,
									 xcb_generic_event_t  *event);

#endif // WLX_WL_OUTPUT_H
