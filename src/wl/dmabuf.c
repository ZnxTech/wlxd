#include "dmabuf.h"

#include <drm/drm_fourcc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xcb/dri3.h>
#include <xcb/randr.h>

#include "../proto/linux-dmabuf-v1.h"
#include "../server.h"
#include "../utils/shm.h"
#include "buffer.h"
#include "surface.h"

// x11 DRM format support is a bit cloudy in terms of direct documentation,
// it is known that it only supports RGB-like formats but, currently, every
// DRM format listed below is a pure guess from src and might change in the
// future.

static const uint32_t WLX_DMABUF_DRM_FORMATS[] = {
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ARGB8888,
};

#define WLX_DMABUF_DRM_FORMATN (sizeof(WLX_DMABUF_DRM_FORMATS)/sizeof(*WLX_DMABUF_DRM_FORMATS))

static void drm_format_get_depth_bpp(uint32_t drm_format,
									 uint8_t *depth,
									 uint8_t *bpp)
{
	switch (drm_format) {
	case DRM_FORMAT_RGB565:
		*depth = 16;
		*bpp = 16;
		break;

	case DRM_FORMAT_XRGB8888:
		*depth = 24;
		*bpp = 32;
		break;

	case DRM_FORMAT_ARGB2101010:
		*depth = 30;
		*bpp = 32;
		break;

	case DRM_FORMAT_ARGB8888:
		*depth = 32;
		*bpp = 32;
		break;

	default:
		*depth = 0;
		*bpp = 0;
	}
}

// -------- buffer resource --------

static void wl_buffer_resource_destroy(wl_resource_t *resource)
{
	wlx_buffer_resource_t *buffer = wl_resource_get_user_data(resource);
	xcb_connection_t	  *xcb = wlx_get_xcb(buffer);
	xcb_free_pixmap(xcb, buffer->x_pixmap);
	free(buffer);
}

static void wl_buffer_request_destroy(wl_client_t	*client,
									  wl_resource_t *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_buffer_interface wl_buffer_impl = {
	.destroy = wl_buffer_request_destroy,
};

// -------- feedback resource --------

static void wl_dmabuf_feedback_resource_destroy(wl_resource_t *resource)
{
	wlx_dmabuf_feedback_resource_t *feedback = wl_resource_get_user_data(resource);
	wlx_dmabuf_feedback_tranche_t  *tranche, *tmp_tranche;
	list_for_each_safe (tranche, tmp_tranche, &feedback->tranches, link) {
		array_release(&tranche->indices);
		free(tranche);
	}

	free(feedback);
}

static void wl_dmabuf_feedback_request_destroy(wl_client_t	 *client,
											   wl_resource_t *resource)
{
	wl_resource_destroy(resource);
}

static const struct zwp_linux_dmabuf_feedback_v1_interface wl_dmabuf_feedback_impl = {
	.destroy = wl_dmabuf_feedback_request_destroy,
};

// -------- params resource --------

static void wlx_dmabuf_params_clear_fds(wlx_dmabuf_params_resource_t *params)
{
	for (int i = 0; i < WLX_DMABUF_MAX_PLANES; i++) {
		params->fds[i] = -1;
	}
}

static void wlx_dmabuf_params_close_fds(wlx_dmabuf_params_resource_t *params)
{
	for (int i = 0; i < WLX_DMABUF_MAX_PLANES; i++) {
		if (params->fds[i] >= 0)
			close(params->fds[i]);

		params->fds[i] = -1;
	}
}

static void wl_dmabuf_params_resource_destroy(wl_resource_t *resource)
{
	wlx_dmabuf_params_resource_t *params = wl_resource_get_user_data(resource);
	wlx_dmabuf_params_close_fds(params);
	free(params);
}

static void wl_dmabuf_params_request_destroy(wl_client_t   *client,
											 wl_resource_t *resource)
{
	wl_resource_destroy(resource);
}

static void wl_dmabuf_params_request_add(wl_client_t   *client,
										 wl_resource_t *resource,
										 int32_t		fd,
										 uint32_t		plane_index,
										 uint32_t		offset,
										 uint32_t		stride,
										 uint32_t		modifier_hi,
										 uint32_t		modifier_lo)
{
	wlx_dmabuf_params_resource_t *params = wl_resource_get_user_data(resource);

	if (params->used) {
		wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "params object already used");
		close(fd);
		return;
	}

	if (plane_index >= WLX_DMABUF_MAX_PLANES) {
		wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "out of bounds plane index %u",
							   plane_index);
		close(fd);
		return;
	}

	if (params->fds[plane_index] != -1) {
		wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET, "plane already set at index %u",
							   plane_index);
		close(fd);
		return;
	}

	uint64_t modifier = (uint64_t)modifier_hi << 32 | modifier_lo;
	if (params->has_modifier && params->drm_modifier != modifier) {
		wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
							   "given modifier doesnt match existing set modifier");
		close(fd);
		return;
	}

	params->drm_modifier = modifier;
	params->has_modifier = true;

	params->fds[plane_index] = fd;
	params->offsets[plane_index] = offset;
	params->strides[plane_index] = stride;
	params->planen++;
}

static int wlx_dmabuf_params_create_pixmap(wlx_dmabuf_params_resource_t *params,
										   xcb_pixmap_t					 pixmap_xid,
										   int32_t						 width,
										   int32_t						 height,
										   uint32_t						 drm_format)
{
	xcb_connection_t *xcb = wlx_get_xcb(params);
	xcb_window_t	  root = wlx_get_xroot(params);

	if (params->used) {
		wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
							   "params object already used");
		goto err_fail;
	}

	if (params->planen == 0) {
		wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE, "no planes were given");
		goto err_fail;
	}

	if (params->fds[0] == -1) {
		wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE, "first plane missing");
		goto err_fail;
	}

	if ((params->fds[2] >= 0 && params->fds[1] == -1) || (params->fds[3] >= 0 && params->fds[2] == -1)) {
		wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
							   "gap exists in planes given");
		goto err_fail;
	}

	bool					  supported = false;
	wlx_dmabuf_table_entry_t *entry;
	array_for_each (entry, &params->dmabuf->table) {
		if (drm_format == entry->drm_format && params->drm_modifier == entry->drm_modifier) {
			supported = true;
			break;
		}
	}

	if (!supported) {
		wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
							   "drm format and modifier pair not supported");
		goto err_fail;
	}

	uint8_t depth, bpp;
	drm_format_get_depth_bpp(drm_format, &depth, &bpp);

	xcb_void_cookie_t pfb_cookie;
	if (params->planen == 1) {
		pfb_cookie = xcb_dri3_pixmap_from_buffer_checked(xcb, pixmap_xid, root, height * params->strides[0], width,
														 height, params->strides[0], depth, bpp, params->fds[0]);

	} else {
		pfb_cookie = xcb_dri3_pixmap_from_buffers_checked(
			xcb, pixmap_xid, root, params->planen, width, height, params->strides[0], params->offsets[0],
			params->strides[1], params->offsets[1], params->strides[2], params->offsets[2], params->strides[3],
			params->offsets[3], depth, bpp, params->drm_modifier, params->fds);
	}

	wlx_dmabuf_params_clear_fds(params);

	if (xcb_request_check(xcb, pfb_cookie))
		goto err_pixmap;

	return 0;

err_fail:
	wlx_dmabuf_params_close_fds(params);

err_pixmap:
	return 1;
}

static void wl_dmabuf_params_request_create(wl_client_t	  *client,
											wl_resource_t *resource,
											int32_t		   width,
											int32_t		   height,
											uint32_t	   drm_format,
											uint32_t	   flags)
{
	wlx_dmabuf_params_resource_t *params = wl_resource_get_user_data(resource);
	params->used = true;

	wlx_buffer_resource_t *buffer = calloc(1, sizeof(*buffer));

	if (!buffer) {
		wl_resource_post_no_memory(resource);
		return;
	}

	buffer->server = params->server;
	buffer->type = WLX_BUFFER_TYPE_DMABUF;
	buffer->width = width;
	buffer->height = height;
	buffer->drm_format = drm_format;
	buffer->drm_flags = flags;

	buffer->x_pixmap = xcb_generate_id(wlx_get_xcb(params));
	if (wlx_dmabuf_params_create_pixmap(params, buffer->x_pixmap, width, height, drm_format)) {
		zwp_linux_buffer_params_v1_send_failed(resource);
		goto err_fail;
	}

	buffer->resource = wl_resource_create(client, &wl_buffer_interface, wl_buffer_interface.version, 0);

	if (!buffer->resource) {
		wl_resource_post_no_memory(resource);
		goto err_fail;
	}

	wl_resource_set_implementation(buffer->resource, &wl_buffer_impl, buffer, wl_buffer_resource_destroy);
	zwp_linux_buffer_params_v1_send_created(resource, buffer->resource);
	return;

err_fail:
	xcb_free_pixmap(wlx_get_xcb(buffer), buffer->x_pixmap);
	free(buffer);
}

static void wl_dmabuf_params_request_create_immed(wl_client_t	*client,
												  wl_resource_t *resource,
												  uint32_t		 id,
												  int32_t		 width,
												  int32_t		 height,
												  uint32_t		 drm_format,
												  uint32_t		 flags)
{
	wlx_dmabuf_params_resource_t *params = wl_resource_get_user_data(resource);
	params->used = true;

	wlx_buffer_resource_t *buffer = calloc(1, sizeof(*buffer));

	if (!buffer) {
		wl_resource_post_no_memory(resource);
		return;
	}

	buffer->server = params->server;
	buffer->type = WLX_BUFFER_TYPE_DMABUF;
	buffer->width = width;
	buffer->height = height;
	buffer->drm_format = drm_format;
	buffer->drm_flags = flags;
	buffer->resource = wl_resource_create(client, &wl_buffer_interface, wl_buffer_interface.version, id);
	wl_resource_set_implementation(buffer->resource, &wl_buffer_impl, buffer, wl_buffer_resource_destroy);

	buffer->x_pixmap = xcb_generate_id(wlx_get_xcb(params));
	if (wlx_dmabuf_params_create_pixmap(params, buffer->x_pixmap, width, height, drm_format)) {
		wl_resource_post_error(buffer->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
							   "dmabuf wl_buffer failed");
	}
}

static const struct zwp_linux_buffer_params_v1_interface wl_dmabuf_params_impl = {
	.destroy = wl_dmabuf_params_request_destroy,
	.add = wl_dmabuf_params_request_add,
	.create = wl_dmabuf_params_request_create,
	.create_immed = wl_dmabuf_params_request_create_immed,
};

// -------- dmabuf resource --------

static int xcb_get_dri_dev(xcb_connection_t	   *xcb,
						   xcb_window_t			window,
						   xcb_randr_provider_t provider,
						   dev_t			   *dev)
{
	xcb_dri3_open_cookie_t o_cookie;
	xcb_dri3_open_reply_t *o_reply;
	o_cookie = xcb_dri3_open(xcb, window, provider);
	o_reply = xcb_dri3_open_reply(xcb, o_cookie, NULL);

	if (!o_reply || o_reply->nfd != 1) {
		if (o_reply)
			free(o_reply);
		return 1;
	}

	int o_fd = xcb_dri3_open_reply_fds(xcb, o_reply)[0];
	free(o_reply);
	if (o_fd < 0)
		return 1;

	struct stat fd_stat;
	if (fstat(o_fd, &fd_stat) == -1) {
		close(o_fd);
		return 1;
	}

	close(o_fd);
	*dev = fd_stat.st_rdev;
	return 0;
}

static void wlx_dmabuf_feedback_add_tranche(wlx_dmabuf_feedback_resource_t *feedback,
											dev_t							dev,
											uint32_t						flags,
											array_t							indices)
{
	wlx_dmabuf_feedback_tranche_t *tranche = calloc(1, sizeof(*tranche));

	if (!tranche) {
		wl_resource_post_no_memory(feedback->resource);
		return;
	}

	tranche->dev = dev;
	tranche->flags = flags;
	array_copy(&tranche->indices, &indices);

	list_insert(&feedback->tranches, &tranche->link);
}

static void wlx_dmabuf_feedback_send_events(wlx_dmabuf_feedback_resource_t *feedback)
{
	array_t main_dev;
	main_dev.data = &feedback->main_dev;
	main_dev.size = sizeof(feedback->main_dev);

	zwp_linux_dmabuf_feedback_v1_send_format_table(feedback->resource, feedback->table_fd, feedback->table_size);
	zwp_linux_dmabuf_feedback_v1_send_main_device(feedback->resource, &main_dev);

	wlx_dmabuf_feedback_tranche_t *tranche;
	list_for_each (tranche, &feedback->tranches, link) {
		array_t dev;
		dev.data = &tranche->dev;
		dev.size = sizeof(tranche->dev);

		zwp_linux_dmabuf_feedback_v1_send_tranche_target_device(feedback->resource, &dev);
		zwp_linux_dmabuf_feedback_v1_send_tranche_formats(feedback->resource, &tranche->indices);
		zwp_linux_dmabuf_feedback_v1_send_tranche_flags(feedback->resource, tranche->flags);
		zwp_linux_dmabuf_feedback_v1_send_tranche_done(feedback->resource);
	}

	zwp_linux_dmabuf_feedback_v1_send_done(feedback->resource);
}

static void wl_dmabuf_resource_destroy(wl_resource_t *resource)
{
	// left empty
}

static void wl_dmabuf_request_destroy(wl_client_t	*client,
									  wl_resource_t *resource)
{
	wl_resource_destroy(resource);
}

static void wl_dmabuf_request_create_params(wl_client_t	  *client,
											wl_resource_t *resource,
											uint32_t	   id)
{
	wlx_dmabuf_global_t			 *dmabuf = wl_resource_get_user_data(resource);
	wlx_dmabuf_params_resource_t *params = calloc(1, sizeof(*params));

	if (!params) {
		wl_resource_post_no_memory(resource);
		return;
	}

	params->server = dmabuf->server;
	wlx_dmabuf_params_clear_fds(params);

	params->resource = wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
										  zwp_linux_buffer_params_v1_interface.version, id);
	wl_resource_set_implementation(params->resource, &wl_dmabuf_feedback_impl, params,
								   wl_dmabuf_params_resource_destroy);
}

static void wl_dmabuf_request_get_default_feedback(wl_client_t	 *client,
												   wl_resource_t *resource,
												   uint32_t		  id)
{
	wlx_dmabuf_global_t *dmabuf = wl_resource_get_user_data(resource);
	xcb_connection_t	*xcb = wlx_get_xcb(dmabuf);
	xcb_window_t		 root = wlx_get_xroot(dmabuf);

	wlx_dmabuf_feedback_resource_t *feedback = calloc(1, sizeof(*feedback));

	if (!feedback) {
		wl_resource_post_no_memory(resource);
		return;
	}

	list_init(&feedback->tranches);
	feedback->server = dmabuf->server;
	feedback->table_fd = dmabuf->table_fd;
	feedback->table_size = dmabuf->table.size;
	feedback->resource = wl_resource_create(client, &zwp_linux_dmabuf_feedback_v1_interface,
											zwp_linux_dmabuf_feedback_v1_interface.version, id);

	if (!feedback->resource) {
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_resource_set_implementation(feedback->resource, &wl_dmabuf_feedback_impl, feedback,
								   wl_dmabuf_feedback_resource_destroy);

	// get main device using provider 0, no mention of this in documentaion but it works?
	if (xcb_get_dri_dev(xcb, root, 0, &feedback->main_dev)) {
		feedback->main_dev_failed = true;
	} else {
		feedback->main_dev_failed = false;
		wlx_dmabuf_feedback_add_tranche(feedback, feedback->main_dev, 0, dmabuf->indices);
		wlx_dmabuf_feedback_send_events(feedback);
	}
}

static void wl_dmabuf_request_get_surface_feedback(wl_client_t	 *client,
												   wl_resource_t *resource,
												   uint32_t		  id,
												   wl_resource_t *surface_resource)
{
	wlx_dmabuf_global_t *dmabuf = wl_resource_get_user_data(resource);
	xcb_connection_t	*xcb = wlx_get_xcb(dmabuf);

	wlx_dmabuf_feedback_resource_t *feedback = calloc(1, sizeof(*feedback));

	if (!feedback) {
		wl_resource_post_no_memory(resource);
		return;
	}

	list_init(&feedback->tranches);
	feedback->server = dmabuf->server;
	feedback->table_fd = dmabuf->table_fd;
	feedback->table_size = dmabuf->table.size;
	feedback->resource = wl_resource_create(client, &zwp_linux_dmabuf_feedback_v1_interface,
											zwp_linux_dmabuf_feedback_v1_interface.version, id);

	if (!feedback->resource) {
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_resource_set_implementation(feedback->resource, &wl_dmabuf_feedback_impl, feedback,
								   wl_dmabuf_feedback_resource_destroy);

	wlx_surface_resource_t *surface = wl_resource_get_user_data(surface_resource);

	// get main device using provider 0, no mention of this in documentaion but it works?
	if (xcb_get_dri_dev(xcb, surface->x_window, 0, &feedback->main_dev)) {
		feedback->main_dev_failed = true;
	} else {
		feedback->main_dev_failed = false;
		wlx_dmabuf_feedback_add_tranche(feedback, feedback->main_dev, 0, dmabuf->indices);
		wlx_dmabuf_feedback_send_events(feedback);
	}
}

static const struct zwp_linux_dmabuf_v1_interface wl_dmabuf_impl = {
	.destroy = wl_dmabuf_request_destroy,
	.create_params = wl_dmabuf_request_create_params,
	.get_default_feedback = wl_dmabuf_request_get_default_feedback,
	.get_surface_feedback = wl_dmabuf_request_get_surface_feedback,
};

static void wl_dmabuf_send_formats(wlx_dmabuf_global_t *dmabuf,
								   wl_resource_t	   *resource)
{
	uint32_t modifier_hi = (DRM_FORMAT_MOD_INVALID & 0xFFFFFFFF00000000) >> 32;
	uint32_t modifier_lo = (DRM_FORMAT_MOD_INVALID & 0x00000000FFFFFFFF);

	for (size_t i = 0; i < WLX_DMABUF_DRM_FORMATN; i++) {
		zwp_linux_dmabuf_v1_send_modifier(resource, WLX_DMABUF_DRM_FORMATS[i], modifier_hi, modifier_lo);
		zwp_linux_dmabuf_v1_send_format(resource, WLX_DMABUF_DRM_FORMATS[i]);
	}
}

static void wl_dmabuf_bind(wl_client_t *client,
						   void		   *data,
						   uint32_t		version,
						   uint32_t		id)
{
	wlx_dmabuf_global_t *dmabuf = data;
	wl_resource_t		*resource = wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &wl_dmabuf_impl, dmabuf, wl_dmabuf_resource_destroy);
	wl_dmabuf_send_formats(dmabuf, resource);
}

// -------- dmabuf global --------

static int wlx_dmabuf_init_table(wlx_dmabuf_global_t *dmabuf)
{
	xcb_connection_t *xcb = wlx_get_xcb(dmabuf);
	xcb_window_t	  root = wlx_get_xroot(dmabuf);

	for (size_t i = 0; i < WLX_DMABUF_DRM_FORMATN; i++) {
		uint32_t drm_format = WLX_DMABUF_DRM_FORMATS[i];

		uint8_t depth, bpp;
		drm_format_get_depth_bpp(drm_format, &depth, &bpp);

		xcb_dri3_get_supported_modifiers_cookie_t gsm_cookie;
		xcb_dri3_get_supported_modifiers_reply_t *gsm_reply;
		gsm_cookie = xcb_dri3_get_supported_modifiers(xcb, root, depth, bpp);
		gsm_reply = xcb_dri3_get_supported_modifiers_reply(xcb, gsm_cookie, NULL);
		if (!gsm_reply)
			continue;

		uint64_t *modifiers;
		int		  modifiern;
		modifiers = xcb_dri3_get_supported_modifiers_screen_modifiers(gsm_reply);
		modifiern = xcb_dri3_get_supported_modifiers_screen_modifiers_length(gsm_reply);

		for (int j = 0; j < modifiern; j++) {
			wlx_dmabuf_table_entry_t *entry;
			entry = array_add(&dmabuf->table, sizeof(*entry));
			entry->drm_format = WLX_DMABUF_DRM_FORMATS[i];
			entry->drm_modifier = modifiers[j];

			uint16_t *index;
			index = array_add(&dmabuf->indices, sizeof(*index));
			*index = array_len(&dmabuf->indices, sizeof(*index));
		}

		free(gsm_reply);
	}

	int rw_fd, ro_fd;
	if (!dmabuf->table.size || !create_shm_file_pair(dmabuf->table.size, &rw_fd, &ro_fd))
		return 1;

	wlx_dmabuf_table_entry_t *entries;
	entries = mmap(NULL, dmabuf->table.size, PROT_WRITE, MAP_SHARED, rw_fd, 0);
	close(rw_fd);

	if (entries == MAP_FAILED) {
		close(ro_fd);
		return 1;
	}

	memcpy(entries, dmabuf->table.data, dmabuf->table.size);

	if (munmap(entries, dmabuf->table.size) == -1) {
		close(ro_fd);
		return 1;
	}

	close(rw_fd);
	dmabuf->table_fd = ro_fd;
	return 0;
}

int wlx_dmabuf_init(wlx_dmabuf_global_t *dmabuf,
					wlx_server_t		*server)
{
	wl_display_t *wl_display = wlx_server_get_wl(server);

	dmabuf->server = server;
	array_init(&dmabuf->table);
	array_init(&dmabuf->indices);

	if (wlx_dmabuf_init_table(dmabuf)) {
		wlx_dmabuf_free(dmabuf);
		return 1;
	}

	dmabuf->global = wl_global_create(wl_display, &zwp_linux_dmabuf_v1_interface, zwp_linux_dmabuf_v1_interface.version,
									  dmabuf, wl_dmabuf_bind);
	return 0;
}

void wlx_dmabuf_free(wlx_dmabuf_global_t *dmabuf)
{
	array_release(&dmabuf->table);
	array_release(&dmabuf->indices);
	if (dmabuf->table_fd >= 0)
		close(dmabuf->table_fd);
}
