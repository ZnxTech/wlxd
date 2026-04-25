#include "shm.h"

#include <drm/drm_fourcc.h>
#include <stdlib.h>

#include "../wl/buffer.h"
#include "../x/ext.h"

static const uint32_t WLX_SHM_FORMATS[] = {
	WL_SHM_FORMAT_RGB565,
	WL_SHM_FORMAT_XRGB8888,
	WL_SHM_FORMAT_ARGB2101010,
	WL_SHM_FORMAT_ARGB8888,
};

#define WLX_SHM_FORMATN (sizeof(WLX_SHM_FORMATS)/sizeof(*WLX_SHM_FORMATS))

static void shm_format_get_depth_bpp(uint32_t shm_format,
									 uint8_t *depth,
									 uint8_t *bpp)
{
	switch (shm_format) {
	case WL_SHM_FORMAT_RGB565:
		*depth = 16;
		*bpp = 16;
		break;

	case WL_SHM_FORMAT_XRGB8888:
		*depth = 24;
		*bpp = 32;
		break;

	case WL_SHM_FORMAT_ARGB2101010:
		*depth = 30;
		*bpp = 32;
		break;

	case WL_SHM_FORMAT_ARGB8888:
		*depth = 32;
		*bpp = 32;
		break;

	default:
		*depth = 0;
		*bpp = 0;
	}
}

static uint32_t shm_format_get_drm_format(uint32_t shm_format)
{
	switch (shm_format) {
	case WL_SHM_FORMAT_XRGB8888:
		return DRM_FORMAT_XRGB8888;

	case WL_SHM_FORMAT_ARGB8888:
		return DRM_FORMAT_ARGB8888;

	default:
		return shm_format;
	}
}

static uint32_t drm_format_get_shm_format(uint32_t drm_format)
{
	switch (drm_format) {
	case DRM_FORMAT_XRGB8888:
		return WL_SHM_FORMAT_XRGB8888;

	case DRM_FORMAT_ARGB8888:
		return WL_SHM_FORMAT_ARGB8888;

	default:
		return drm_format;
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

// -------- shm pool resource --------

static void wl_shm_pool_resource_destroy(wl_resource_t *resource)
{
	wlx_shm_pool_resource_t *shm_pool = wl_resource_get_user_data(resource);
	xcb_connection_t		*xcb = wlx_get_xcb(shm_pool);

	wlx_buffer_resource_t *buffer, *tmp_buffer;
	list_for_each_safe (buffer, tmp_buffer, &shm_pool->buffers, link) {
		wl_resource_destroy(buffer->resource);
	}

	xcb_shm_detach(xcb, shm_pool->x_seg);
	free(shm_pool);
}

static void wl_shm_pool_request_destroy(wl_client_t	  *client,
										wl_resource_t *resource)
{
	wl_resource_destroy(resource);
}

static void wl_shm_pool_request_resize(wl_client_t	 *client,
									   wl_resource_t *resource,
									   int32_t		  size)
{
	wlx_shm_pool_resource_t *shm_pool = wl_resource_get_user_data(resource);
	xcb_connection_t		*xcb = wlx_get_xcb(shm_pool);
	xcb_window_t			 root = wlx_get_xroot(shm_pool);
	wlx_buffer_resource_t	*buffer;

	if (shm_pool->size >= size)
		return;

	list_for_each (buffer, &shm_pool->buffers, link) {
		xcb_free_pixmap(xcb, buffer->x_pixmap);
		buffer->x_pixmap = 0;
	}

	// this might cause issues later, test when possible

	xcb_shm_detach(xcb, shm_pool->x_seg);
	shm_pool->size = size;
	shm_pool->x_seg = xcb_generate_id(xcb);
	xcb_shm_attach_fd(xcb, shm_pool->x_seg, shm_pool->fd, true);

	list_for_each (buffer, &shm_pool->buffers, link) {
		buffer->x_pixmap = xcb_generate_id(xcb);
		int32_t shm_format = drm_format_get_shm_format(buffer->drm_format);
		uint8_t depth, bpp;
		shm_format_get_depth_bpp(shm_format, &depth, &bpp);
		xcb_shm_create_pixmap(xcb, buffer->x_pixmap, root, buffer->width, buffer->height, depth, shm_pool->x_seg,
							  buffer->offset);
	}
}

static void wl_shm_pool_request_create_buffer(wl_client_t	*client,
											  wl_resource_t *resource,
											  uint32_t		 id,
											  int32_t		 offset,
											  int32_t		 width,
											  int32_t		 height,
											  int32_t		 stride,
											  uint32_t		 format)
{
	wlx_shm_pool_resource_t *shm_pool = wl_resource_get_user_data(resource);
	xcb_connection_t		*xcb = wlx_get_xcb(shm_pool);
	xcb_window_t			 root = wlx_get_xroot(shm_pool);
	uint8_t					 root_depth = wlx_get_xroot_depth(shm_pool);

	if (width <= 0 || height <= 0 || stride <= 0) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE, "invalid width, height and or stride");
		return;
	}

	// x only allows uint16 sizes of width and height
	if (width > UINT16_MAX || height > UINT16_MAX) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE,
							   "width and or height given is bigger than UINT16_MAX");
		return;
	}

	uint8_t depth, bpp;
	shm_format_get_depth_bpp(format, &depth, &bpp);

	bool supported = false;
	for (int i = 0; i < WLX_SHM_FORMATN; i++) {
		if (WLX_SHM_FORMATS[i] == format) {
			supported = true;
			break;
		}
	}

	if (depth > root_depth || !supported) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FORMAT, "unsupported format given");
		return;
	}

	if (bpp * width != stride) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE, "invalid stride given");
		return;
	}

	int32_t size = stride * height;

	if (offset + size > shm_pool->size) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE, "buffer goes out of pool bounds");
		return;
	}

	wlx_buffer_resource_t *buffer = calloc(1, sizeof(*buffer));

	if (!buffer) {
		wl_resource_post_no_memory(resource);
		return;
	}

	buffer->server = shm_pool->server;
	buffer->type = WLX_BUFFER_TYPE_SHM;
	buffer->width = width;
	buffer->height = height;
	buffer->stride = stride;
	buffer->size = size;
	buffer->offset = offset;
	buffer->drm_format = shm_format_get_drm_format(format);
	buffer->x_pixmap = xcb_generate_id(xcb);

	xcb_void_cookie_t cp_cookie;
	cp_cookie =
		xcb_shm_create_pixmap_checked(xcb, buffer->x_pixmap, root, width, height, depth, shm_pool->x_seg, offset);

	if (xcb_request_check(xcb, cp_cookie)) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "x pixmap failed");
		xcb_free_pixmap(xcb, buffer->x_pixmap);
		free(buffer);
		return;
	}

	buffer->resource = wl_resource_create(client, &wl_buffer_interface, wl_buffer_interface.version, id);

	if (!buffer->resource) {
		wl_resource_post_no_memory(resource);
		xcb_free_pixmap(xcb, buffer->x_pixmap);
		free(buffer);
		return;
	}

	wl_resource_set_implementation(buffer->resource, &wl_buffer_impl, buffer, wl_buffer_resource_destroy);
	list_insert(&shm_pool->buffers, &buffer->link);
}

static const struct wl_shm_pool_interface wl_shm_pool_impl = {
	.destroy = wl_shm_pool_request_destroy,
	.resize = wl_shm_pool_request_resize,
	.create_buffer = wl_shm_pool_request_create_buffer,
};

// -------- shm resource --------

static void wl_shm_resource_destroy(wl_resource_t *resource)
{
	// left empty
}

static void wl_shm_request_release(wl_client_t	 *client,
								   wl_resource_t *resource)
{
	wl_shm_resource_destroy(resource);
}

static void wl_shm_request_create_pool(wl_client_t	 *client,
									   wl_resource_t *resource,
									   uint32_t		  id,
									   int32_t		  fd,
									   int32_t		  size)
{
	wlx_shm_global_t		*shm = wl_resource_get_user_data(resource);
	xcb_connection_t		*xcb = wlx_get_xcb(shm);
	wlx_shm_pool_resource_t *shm_pool = calloc(1, sizeof(*shm_pool));

	if (!shm_pool) {
		wl_resource_post_no_memory(resource);
		return;
	}

	shm_pool->server = shm->server;
	list_init(&shm_pool->buffers);

	if (fd < 0) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "invalid fd given");
		return;
	}

	if (size <= 0) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE, "invalid size given");
		return;
	}

	shm_pool->fd = fd;
	shm_pool->size = size;
	shm_pool->x_seg = xcb_generate_id(xcb);

	xcb_void_cookie_t af_cookie;
	af_cookie = xcb_shm_attach_fd_checked(xcb, shm_pool->x_seg, fd, true);

	if (xcb_request_check(xcb, af_cookie)) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "invalid fd given");
		xcb_shm_detach(xcb, shm_pool->x_seg);
		return;
	}

	shm_pool->resource = wl_resource_create(client, &wl_shm_pool_interface, wl_shm_pool_interface.version, id);

	if (!shm_pool->resource) {
		wl_resource_post_no_memory(resource);
		xcb_shm_detach(xcb, shm_pool->x_seg);
		return;
	}

	wl_resource_set_implementation(shm_pool->resource, &wl_shm_pool_impl, shm_pool, wl_shm_pool_resource_destroy);
}

static const struct wl_shm_interface wl_shm_impl = {
	.release = wl_shm_request_release,
	.create_pool = wl_shm_request_create_pool,
};

static void wl_shm_send_formats(wl_resource_t *resource,
								uint8_t		   max_depth)
{
	for (int i = 0; i < WLX_SHM_FORMATN; i++) {
		uint8_t depth, bpp;
		shm_format_get_depth_bpp(WLX_SHM_FORMATS[i], &depth, &bpp);
		if (depth <= max_depth)
			wl_shm_send_format(resource, WLX_SHM_FORMATS[i]);
	}
}

static void wl_shm_bind(wl_client_t *client,
						void		*data,
						uint32_t	 version,
						uint32_t	 id)
{
	wlx_shm_global_t *shm = data;
	wl_resource_t	 *resource = wl_resource_create(client, &wl_shm_interface, version, id);
	uint8_t			  depth = wlx_get_xroot_depth(shm);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &wl_shm_impl, shm, wl_shm_resource_destroy);
	wl_shm_send_formats(resource, depth);
}

// -------- shm global --------

int wlx_shm_global_init(wlx_shm_global_t *shm,
						wlx_server_t	 *server)
{
	x_ext_manager_t	 *ext_manager = wlx_server_get_ext_manager(server);
	xcb_connection_t *xcb = wlx_server_get_xcb(server);
	wl_display_t	 *wl = wlx_server_get_wl(server);

	if (!ext_manager->shm.present)
		goto err_ext;

	xcb_shm_query_version_cookie_t qv_cookie;
	xcb_shm_query_version_reply_t *qv_reply;
	qv_cookie = xcb_shm_query_version(xcb);
	qv_reply = xcb_shm_query_version_reply(xcb, qv_cookie, NULL);

	if (!qv_reply || !qv_reply->shared_pixmaps || qv_reply->pixmap_format != XCB_IMAGE_FORMAT_Z_PIXMAP)
		goto err_query;

	shm->server = server;
	shm->global = wl_global_create(wl, &wl_shm_interface, wl_shm_interface.version, shm, wl_shm_bind);
	free(qv_reply);
	return 0;

err_query:
	free(qv_reply);
err_ext:
	return 1;
}

int wlx_shm_global_free(wlx_shm_global_t *shm)
{
	return 0;
}
