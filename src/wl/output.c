#include "output.h"

#include <stdlib.h>

#include "../server.h"
#include "../utils/xid_map.h"
#include "../x/ext.h"

#define X_ROTATION_MASK   0b00001111
#define X_REFLECTION_MASK 0b00110000

static uint32_t x_to_wl_transform(uint32_t x_transform)
{
	uint32_t x_rotation = x_transform & X_ROTATION_MASK;
	uint32_t x_reflection = x_transform & X_REFLECTION_MASK;

	uint32_t wl_transform = 0;
	switch (x_rotation) {
	case XCB_RANDR_ROTATION_ROTATE_0:
		wl_transform = WL_OUTPUT_TRANSFORM_NORMAL;
		break;

	case XCB_RANDR_ROTATION_ROTATE_90:
		wl_transform = WL_OUTPUT_TRANSFORM_90;
		break;

	case XCB_RANDR_ROTATION_ROTATE_180:
		wl_transform = WL_OUTPUT_TRANSFORM_180;
		break;

	case XCB_RANDR_ROTATION_ROTATE_270:
		wl_transform = WL_OUTPUT_TRANSFORM_270;
		break;
	}

	if (x_reflection & XCB_RANDR_ROTATION_REFLECT_X)
		wl_transform ^= WL_OUTPUT_TRANSFORM_FLIPPED;

	if (x_reflection & XCB_RANDR_ROTATION_REFLECT_Y) {
		wl_transform ^= WL_OUTPUT_TRANSFORM_FLIPPED;
		wl_transform ^= WL_OUTPUT_TRANSFORM_180;
	}

	return wl_transform;
}

static uint32_t x_to_wl_subpixel_order(uint32_t x_subpixel_order)
{
	switch (x_subpixel_order) {
	case XCB_RENDER_SUB_PIXEL_UNKNOWN:
		return WL_OUTPUT_SUBPIXEL_UNKNOWN;

	case XCB_RENDER_SUB_PIXEL_HORIZONTAL_RGB:
		return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;

	case XCB_RENDER_SUB_PIXEL_HORIZONTAL_BGR:
		return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;

	case XCB_RENDER_SUB_PIXEL_VERTICAL_RGB:
		return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;

	case XCB_RENDER_SUB_PIXEL_VERTICAL_BGR:
		return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;

	case XCB_RENDER_SUB_PIXEL_NONE:
		return WL_OUTPUT_SUBPIXEL_NONE;

	default:
		return WL_OUTPUT_SUBPIXEL_UNKNOWN;
	}
}

// -------- output mode --------

static int32_t modeinfo_get_refresh_mhz(xcb_randr_mode_info_t x_mode)
{
	uint16_t htotal = x_mode.htotal;
	uint16_t vtotal = x_mode.vtotal;

	if (x_mode.mode_flags & XCB_RANDR_MODE_FLAG_DOUBLE_SCAN)
		vtotal *= 2;

	if (x_mode.mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE)
		vtotal /= 2;

	if (vtotal && htotal)
		return ((double)x_mode.dot_clock * 1000) / (htotal * vtotal);
	else
		return 0;
}

void wlx_output_mode_create(wlx_output_manager_t *manager,
							xcb_randr_mode_info_t x_mode,
							int32_t				  mode)
{
	xid_map_t *map = wlx_get_xid_map(manager);

	wlx_output_mode_t *output_mode = calloc(1, sizeof(*output_mode));
	output_mode->server = manager->server;
	output_mode->xid = x_mode.id;
	output_mode->width_pix = x_mode.width;
	output_mode->height_pix = x_mode.height;
	output_mode->refresh_mhz = modeinfo_get_refresh_mhz(x_mode);
	output_mode->mode = mode;

	list_insert(&manager->modes, &output_mode->link);
	xid_map_insert(map, output_mode->xid, output_mode);
}

void wlx_output_mode_free(wlx_output_mode_t *output_mode)
{
	xid_map_t *map = wlx_get_xid_map(output_mode);

	xid_map_remove(map, output_mode->xid);
	list_remove(&output_mode->link);
	free(output_mode);
}

// -------- output crtc --------

void wlx_output_crtc_create(wlx_output_manager_t *manager,
							xcb_randr_crtc_t	  crtc_xid)
{
	xcb_connection_t *xcb = wlx_get_xcb(manager);
	xid_map_t		 *map = wlx_get_xid_map(manager);

	wlx_output_crtc_t *output_crtc = calloc(1, sizeof(*output_crtc));
	output_crtc->server = manager->server;
	output_crtc->xid = crtc_xid;

	xcb_generic_error_t				*gci_err = NULL;
	xcb_randr_get_crtc_info_cookie_t gci_cookie;
	xcb_randr_get_crtc_info_reply_t *gci_reply;
	gci_cookie = xcb_randr_get_crtc_info(xcb, crtc_xid, manager->config_timestamp);
	gci_reply = xcb_randr_get_crtc_info_reply(xcb, gci_cookie, &gci_err);

	if (gci_err)
		goto err;

	output_crtc->logical_x = gci_reply->x;
	output_crtc->logical_y = gci_reply->y;
	output_crtc->logical_width_pix = gci_reply->width;
	output_crtc->logical_height_pix = gci_reply->height;
	output_crtc->transform = x_to_wl_transform(gci_reply->rotation);

	wlx_output_mode_t *output_mode = xid_map_search(map, gci_reply->mode);

	if (!output_mode)
		goto err;

	output_crtc->mode = output_mode;
	list_insert(&manager->crtcs, &output_crtc->link);
	xid_map_insert(map, crtc_xid, output_crtc);

	free(gci_reply);
	return;
err:
	free(gci_reply);
	free(output_crtc);
}

void wlx_output_crtc_free(wlx_output_crtc_t *output_crtc)
{
	xid_map_t *map = wlx_get_xid_map(output_crtc);

	xid_map_remove(map, output_crtc->xid);
	list_remove(&output_crtc->link);
	free(output_crtc);
}

// -------- output resource --------

void wl_output_resource_destroy(wl_resource_t *resource)
{
	wlx_output_global_resource_t *output_resource = wl_resource_get_user_data(resource);
	list_remove(&output_resource->link);
	free(output_resource);
}

void wl_output_request_release(wl_client_t	 *client,
							   wl_resource_t *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_output_interface wl_output_impl = {
	.release = wl_output_request_release,
};

void wl_output_bind(wl_client_t *client,
					void		*data,
					uint32_t	 version,
					uint32_t	 id)
{
	wlx_output_global_t			 *output = data;
	wlx_output_global_resource_t *output_resource = calloc(1, sizeof(*output_resource));
	output_resource->resource = wl_resource_create(client, &wl_output_interface, version, id);
	output_resource->global = output->global;
	list_insert(&output->resources, &output_resource->link);

	wl_resource_set_implementation(output_resource->resource, &wl_output_impl, output_resource,
								   wl_output_resource_destroy);
	wl_output_send_geometry(output_resource->resource, output->crtc->logical_x, output->crtc->logical_y,
							output->width_phy, output->height_phy, output->subpixel_order, output->make, output->model,
							output->crtc->transform);
	wl_output_send_mode(output_resource->resource, WL_OUTPUT_MODE_CURRENT, output->mode->width_pix,
						output->mode->height_pix, output->mode->refresh_mhz);
	wl_output_send_scale(output_resource->resource, output->scale_factor);
	wl_output_send_name(output_resource->resource, output->name);
	wl_output_send_description(output_resource->resource, output->desc);
	wl_output_send_done(output_resource->resource);
}

// -------- output global --------

void wlx_output_global_send_mode(wlx_output_global_t *output)
{
	wlx_output_global_resource_t *output_global_resource;
	list_for_each (output_global_resource, &output->resources, link)
		wl_output_send_mode(output_global_resource->resource, WL_OUTPUT_MODE_CURRENT, output->mode->width_pix,
							output->mode->height_pix, output->mode->refresh_mhz);
}

void wlx_output_global_connect(wlx_output_global_t *output)
{
	if (output->global)
		return;

	output->global =
		wl_global_create(wlx_get_wl(output), &wl_output_interface, wl_output_interface.version, output, wl_output_bind);
}

void wlx_output_global_disconnect(wlx_output_global_t *output)
{
	if (!output->global)
		return;

	wl_global_destroy(output->global);
}

void wlx_output_global_create(wlx_output_manager_t *manager,
							  xcb_randr_output_t	output_xid)
{
	xcb_connection_t *xcb = wlx_get_xcb(manager);
	xid_map_t		 *map = wlx_get_xid_map(manager);

	wlx_output_global_t *output = calloc(1, sizeof(*output));
	output->server = manager->server;
	output->xid = output_xid;
	list_init(&output->resources);

	xcb_randr_get_output_info_cookie_t goi_cookie;
	xcb_randr_get_output_info_reply_t *goi_reply;
	goi_cookie = xcb_randr_get_output_info(xcb, output_xid, manager->config_timestamp);
	goi_reply = xcb_randr_get_output_info_reply(xcb, goi_cookie, NULL);

	if (!goi_reply)
		goto err_randr;

	output->width_phy = goi_reply->mm_width;
	output->height_phy = goi_reply->mm_height;
	output->subpixel_order = x_to_wl_subpixel_order(goi_reply->subpixel_order);
	output->scale_factor = 1;

	wlx_output_crtc_t *output_crtc = xid_map_search(map, goi_reply->crtc);

	if (!output_crtc)
		goto err_randr;

	output->crtc = output_crtc;
	output->mode = output_crtc->mode;

	list_insert(&manager->outputs, &output->link);
	xid_map_insert(map, output_xid, output);

	if (goi_reply->connection == XCB_RANDR_CONNECTION_CONNECTED)
		wlx_output_global_connect(output);

	free(goi_reply);
	return;
err_randr:
	free(goi_reply);
	free(output);
}

void wlx_output_global_free(wlx_output_global_t *output)
{
	wlx_output_global_disconnect(output);
	xid_map_remove(wlx_get_xid_map(output), output->xid);
	list_remove(&output->link);
	free(output);
}

// -------- output manager --------

int wlx_output_manager_init(wlx_output_manager_t *manager,
							wlx_server_t		 *server)
{
	xcb_connection_t *xcb = wlx_server_get_xcb(server);
	xcb_window_t	  root = wlx_server_get_xroot(server);
	x_ext_manager_t	 *ext_manager = wlx_server_get_ext_manager(server);

	manager->server = server;
	list_init(&manager->modes);
	list_init(&manager->crtcs);
	list_init(&manager->outputs);

	if (!ext_manager->randr.present)
		goto err_randr;

	xcb_randr_get_screen_resources_cookie_t gsr_cookie;
	xcb_randr_get_screen_resources_reply_t *gsr_reply;
	gsr_cookie = xcb_randr_get_screen_resources(xcb, root);
	gsr_reply = xcb_randr_get_screen_resources_reply(xcb, gsr_cookie, NULL);

	if (!gsr_reply)
		goto err_reply;

	manager->config_timestamp = gsr_reply->config_timestamp;

	xcb_randr_mode_info_t *gsr_modes;
	int					   gsr_moden;
	gsr_modes = xcb_randr_get_screen_resources_modes(gsr_reply);
	gsr_moden = xcb_randr_get_screen_resources_modes_length(gsr_reply);
	for (int i = 0; i < gsr_moden; i++)
		wlx_output_mode_create(manager, gsr_modes[i], 0);

	xcb_randr_crtc_t *gsr_crtcs;
	int				  gsr_crtcn;
	gsr_crtcs = xcb_randr_get_screen_resources_crtcs(gsr_reply);
	gsr_crtcn = xcb_randr_get_screen_resources_crtcs_length(gsr_reply);
	for (int i = 0; i < gsr_crtcn; i++)
		wlx_output_crtc_create(manager, gsr_crtcs[i]);

	xcb_randr_output_t *gsr_outputs;
	int					gsr_outputn;
	gsr_outputs = xcb_randr_get_screen_resources_outputs(gsr_reply);
	gsr_outputn = xcb_randr_get_screen_resources_outputs_length(gsr_reply);
	for (int i = 0; i < gsr_outputn; i++)
		wlx_output_global_create(manager, gsr_outputs[i]);

	free(gsr_reply);
	return 0;
err_reply:
	free(gsr_reply);
err_randr:
	return 1;
}

void wlx_output_manager_free(wlx_output_manager_t *manager)
{
	wlx_output_global_t *output, *tmp_output;
	list_for_each_safe (output, tmp_output, &manager->outputs, link)
		wlx_output_global_free(output);

	wlx_output_crtc_t *output_crtc, *tmp_output_crtc;
	list_for_each_safe (output_crtc, tmp_output_crtc, &manager->crtcs, link)
		wlx_output_crtc_free(output_crtc);

	wlx_output_mode_t *output_mode, *tmp_output_mode;
	list_for_each_safe (output_mode, tmp_output_mode, &manager->modes, link)
		wlx_output_mode_free(output_mode);
}

// -------- x event handling --------

void handle_randr_notify_output_change(wlx_output_manager_t		*manager,
									   xcb_randr_output_change_t oc_event)
{
	xid_map_t *map = wlx_get_xid_map(manager);

	wlx_output_global_t *output = xid_map_search(map, oc_event.output);

	if (!output)
		return;

	if (output->crtc->xid != oc_event.crtc) {
		wlx_output_crtc_t *output_crtc = xid_map_search(map, oc_event.crtc);
		output->crtc = output_crtc;
	}

	if (output->mode->xid != oc_event.mode) {
		wlx_output_mode_t *output_mode = xid_map_search(map, oc_event.mode);
		output->mode = output_mode;
		wlx_output_global_send_mode(output);
	}

	output->subpixel_order = oc_event.subpixel_order;

	if (oc_event.connection == XCB_RANDR_CONNECTION_CONNECTED)
		wlx_output_global_connect(output);
	else
		wlx_output_global_disconnect(output);
}

void handle_randr_notify_crtc_change(wlx_output_manager_t	*manager,
									 xcb_randr_crtc_change_t cc_event)
{
	xid_map_t *map = wlx_get_xid_map(manager);

	wlx_output_crtc_t *output_crtc = xid_map_search(map, cc_event.crtc);
	wlx_output_mode_t *output_mode = xid_map_search(map, cc_event.mode);

	// new crtc case
	if (!output_crtc) {
		wlx_output_crtc_create(manager, cc_event.crtc);
		return;
	}

	if (!output_mode)
		return;

	output_crtc->logical_x = cc_event.x;
	output_crtc->logical_y = cc_event.y;
	output_crtc->logical_width_pix = cc_event.width;
	output_crtc->logical_height_pix = cc_event.height;
	output_crtc->transform = x_to_wl_transform(cc_event.rotation);
	output_crtc->mode = output_mode;
}

void resources_compare_modes(wlx_output_manager_t  *manager,
							 xcb_randr_mode_info_t *gsr_modes,
							 int					gsr_moden)
{
	for (int i = 0; i < gsr_moden; i++) {
		bool			   found = false;
		wlx_output_mode_t *output_mode;
		list_for_each (output_mode, &manager->modes, link) {
			if (gsr_modes[i].id == output_mode->xid) {
				found = true;
				break;
			}
		}

		if (!found)
			wlx_output_mode_create(manager, gsr_modes[i], 0);
	}

	wlx_output_mode_t *output_mode, *tmp_output_mode;
	list_for_each_safe (output_mode, tmp_output_mode, &manager->modes, link) {
		bool found = false;
		for (int i = 0; i < gsr_moden; i++) {
			if (gsr_modes[i].id == output_mode->xid) {
				found = true;
				break;
			}
		}

		if (!found)
			wlx_output_mode_free(output_mode);
	}
}

void resources_compare_crtcs(wlx_output_manager_t *manager,
							 xcb_randr_crtc_t	  *gsr_crtcs,
							 int				   gsr_crtcn)
{
	for (int i = 0; i < gsr_crtcn; i++) {
		bool			   found = false;
		wlx_output_crtc_t *output_crtc;
		list_for_each (output_crtc, &manager->crtcs, link) {
			if (gsr_crtcs[i] == output_crtc->xid) {
				found = true;
				break;
			}
		}

		if (!found)
			wlx_output_crtc_create(manager, gsr_crtcs[i]);
	}

	wlx_output_crtc_t *output_crtc, *tmp_output_crtc;
	list_for_each_safe (output_crtc, tmp_output_crtc, &manager->crtcs, link) {
		bool found = false;
		for (int i = 0; i < gsr_crtcn; i++) {
			if (gsr_crtcs[i] == output_crtc->xid) {
				found = true;
				break;
			}
		}

		if (!found)
			wlx_output_crtc_free(output_crtc);
	}
}

void resources_compare_outputs(wlx_output_manager_t *manager,
							   xcb_randr_output_t	*gsr_outputs,
							   int					 gsr_outputn)
{
	for (int i = 0; i < gsr_outputn; i++) {
		bool				 found = false;
		wlx_output_global_t *output;
		list_for_each (output, &manager->outputs, link) {
			if (gsr_outputs[i] == output->xid) {
				found = true;
				break;
			}
		}

		if (!found)
			wlx_output_global_create(manager, gsr_outputs[i]);
	}

	wlx_output_global_t *output, *tmp_output;
	list_for_each_safe (output, tmp_output, &manager->outputs, link) {
		bool found = false;
		for (int i = 0; i < gsr_outputn; i++) {
			if (gsr_outputs[i] == output->xid) {
				found = true;
				break;
			}
		}

		if (!found)
			wlx_output_global_free(output);
	}
}

void handle_randr_notify_resource_change(wlx_output_manager_t		*manager,
										 xcb_randr_resource_change_t rc_event)
{
	xcb_connection_t *xcb = wlx_get_xcb(manager);
	xcb_window_t	  root = wlx_get_xroot(manager);

	xcb_generic_error_t					   *gsr_err = NULL;
	xcb_randr_get_screen_resources_cookie_t gsr_cookie;
	xcb_randr_get_screen_resources_reply_t *gsr_reply;
	gsr_cookie = xcb_randr_get_screen_resources(xcb, root);
	gsr_reply = xcb_randr_get_screen_resources_reply(xcb, gsr_cookie, &gsr_err);

	if (gsr_err)
		goto err;

	manager->config_timestamp = gsr_reply->config_timestamp;

	xcb_randr_mode_info_t *gsr_modes;
	int					   gsr_moden;
	gsr_modes = xcb_randr_get_screen_resources_modes(gsr_reply);
	gsr_moden = xcb_randr_get_screen_resources_modes_length(gsr_reply);
	resources_compare_modes(manager, gsr_modes, gsr_moden);

	xcb_randr_crtc_t *gsr_crtcs;
	int				  gsr_crtcn;
	gsr_crtcs = xcb_randr_get_screen_resources_crtcs(gsr_reply);
	gsr_crtcn = xcb_randr_get_screen_resources_crtcs_length(gsr_reply);
	resources_compare_crtcs(manager, gsr_crtcs, gsr_crtcn);

	xcb_randr_output_t *gsr_outputs;
	int					gsr_outputn;
	gsr_outputs = xcb_randr_get_screen_resources_outputs(gsr_reply);
	gsr_outputn = xcb_randr_get_screen_resources_outputs_length(gsr_reply);
	resources_compare_outputs(manager, gsr_outputs, gsr_outputn);

err:
	free(gsr_reply);
}

void handle_randr_notify_event(wlx_output_manager_t		*manager,
							   xcb_randr_notify_event_t *event)
{
	switch (event->subCode) {
	case XCB_RANDR_NOTIFY_OUTPUT_CHANGE:
		handle_randr_notify_output_change(manager, event->u.oc);
		break;

	case XCB_RANDR_NOTIFY_CRTC_CHANGE:
		handle_randr_notify_crtc_change(manager, event->u.cc);
		break;

	case XCB_RANDR_NOTIFY_RESOURCE_CHANGE:
		handle_randr_notify_resource_change(manager, event->u.rc);
		break;
	}
}

void wlx_output_manager_handle_event(wlx_output_manager_t *manager,
									 xcb_generic_event_t  *event)
{
	x_ext_manager_t *ext_manager = wlx_get_ext_manager(manager);

	if (ext_manager->randr.present) {
		switch (event->response_type - ext_manager->randr.f_event) {
		case XCB_RANDR_NOTIFY:
			handle_randr_notify_event(manager, (xcb_randr_notify_event_t *)event);
			break;
		}
	}
}
