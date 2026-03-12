#include "compositor.h"

#include <stdlib.h>

#include "region.h"
#include "surface.h"

#include "../server.h"

// -------- region resource --------

static void wl_region_resource_destroy(wl_resource_t *resource)
{
	wlx_region_resource_t *region = wl_resource_get_user_data(resource);
	list_remove(&region->link);
	array_release(&region->rects);
	free(region);
}

static void wl_region_request_destroy(wl_client_t	*client,
									  wl_resource_t *resource)
{
	wl_resource_destroy(resource);
}

static void wl_region_request_add(wl_client_t	*client,
								  wl_resource_t *resource,
								  int32_t		 x,
								  int32_t		 y,
								  int32_t		 width,
								  int32_t		 height)
{
	wlx_region_resource_t *region = wl_resource_get_user_data(resource);

	wlx_region_rect_t *rect_p = array_add(&region->rects, sizeof(wlx_region_rect_t));
	*rect_p = (wlx_region_rect_t){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
		.add = true,
	};
}

static void wl_region_request_subtract(wl_client_t	 *client,
									   wl_resource_t *resource,
									   int32_t		  x,
									   int32_t		  y,
									   int32_t		  width,
									   int32_t		  height)
{
	wlx_region_resource_t *region = wl_resource_get_user_data(resource);

	wlx_region_rect_t *rect_p = array_add(&region->rects, sizeof(wlx_region_rect_t));
	*rect_p = (wlx_region_rect_t){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
		.add = false,
	};
}

const static struct wl_region_interface wl_region_impl = {
	.destroy = wl_region_request_destroy,
	.add = wl_region_request_add,
	.subtract = wl_region_request_subtract,
};

// -------- surface resource --------

static void wl_surface_resource_destroy(wl_resource_t *resource)
{
	wlx_surface_resource_t *surface = wl_resource_get_user_data(resource);
	list_remove(&surface->link);
	free(surface);
}

static void wl_surface_request_destroy(wl_client_t	 *client,
									   wl_resource_t *resource)
{
	wl_resource_destroy(resource);
}

static void wl_surface_request_attach(wl_client_t	*client,
									  wl_resource_t *resource,
									  wl_resource_t *buffer_resource,
									  int32_t		 x,
									  int32_t		 y)
{
	wlx_surface_resource_t *surface = wl_resource_get_user_data(resource);
	wlx_buffer_resource_t  *buffer = buffer_resource ? wl_resource_get_user_data(buffer_resource) : NULL;
	surface->buffer_attached[0] = buffer;
}

static void wl_surface_request_damage(wl_client_t	*client,
									  wl_resource_t *resource,
									  int32_t		 x,
									  int32_t		 y,
									  int32_t		 width,
									  int32_t		 height)
{
	wlx_surface_resource_t *surface = wl_resource_get_user_data(resource);
	surface->damage_type[0] = WLX_SURFACE_DAMAGE_SURFACE;
	surface->damage_x[0] = x;
	surface->damage_y[0] = y;
	surface->damage_width[0] = width;
	surface->damage_height[0] = height;
}

static void wl_surface_request_damage_buffer(wl_client_t   *client,
											 wl_resource_t *resource,
											 int32_t		x,
											 int32_t		y,
											 int32_t		width,
											 int32_t		height)
{
	wlx_surface_resource_t *surface = wl_resource_get_user_data(resource);
	surface->damage_type[0] = WLX_SURFACE_DAMAGE_BUFFER;
	surface->damage_x[0] = x;
	surface->damage_y[0] = y;
	surface->damage_width[0] = width;
	surface->damage_height[0] = height;
}

static void wl_surface_request_set_opaque_region(wl_client_t   *client,
												 wl_resource_t *resource,
												 wl_resource_t *region_resource)
{
	wlx_surface_resource_t *surface = wl_resource_get_user_data(resource);
	wlx_region_resource_t  *region = region_resource ? wl_resource_get_user_data(region_resource) : NULL;
	surface->opaque_region[0] = region;
}

static void wl_surface_request_set_input_region(wl_client_t	  *client,
												wl_resource_t *resource,
												wl_resource_t *region_resource)
{
	wlx_surface_resource_t *surface = wl_resource_get_user_data(resource);
	wlx_region_resource_t  *region = region_resource ? wl_resource_get_user_data(region_resource) : NULL;
	surface->input_region[0] = region;
}

static void wl_surface_request_offset(wl_client_t	*client,
									  wl_resource_t *resource,
									  int32_t		 x,
									  int32_t		 y)
{
	wlx_surface_resource_t *surface = wl_resource_get_user_data(resource);
	surface->buffer_x[0] = x;
	surface->buffer_y[0] = y;
}

const static struct wl_surface_interface wl_surface_impl = {
	.destroy = wl_surface_request_destroy,
	.attach = wl_surface_request_attach,
	.damage = wl_surface_request_damage,
	.damage_buffer = wl_surface_request_damage_buffer,
	.set_opaque_region = wl_surface_request_set_opaque_region,
	.set_input_region = wl_surface_request_set_input_region,
};

// -------- compositor resource --------

void wl_compositor_resource_destroy(wl_resource_t *resource)
{
	// left empty
}

void wl_compositor_request_create_surface(wl_client_t	*client,
										  wl_resource_t *resource,
										  uint32_t		 id)
{
	wlx_compositor_global_t *compositor = wl_resource_get_user_data(resource);
	wlx_surface_resource_t	*surface = calloc(1, sizeof(wlx_surface_resource_t));
	surface->resource = wl_resource_create(client, &wl_surface_interface, wl_surface_interface.version, id);
	wl_resource_set_implementation(surface->resource, &wl_surface_impl, surface, wl_surface_resource_destroy);
	list_insert(&compositor->surfaces, &surface->link);
}

void wl_compositor_request_create_region(wl_client_t   *client,
										 wl_resource_t *resource,
										 uint32_t		id)
{
	wlx_compositor_global_t *compositor = wl_resource_get_user_data(resource);
	wlx_region_resource_t	*region = calloc(1, sizeof(wlx_region_resource_t));
	array_init(&region->rects);
	region->resource = wl_resource_create(client, &wl_region_interface, wl_region_interface.version, id);
	wl_resource_set_implementation(region->resource, &wl_region_impl, region, wl_region_resource_destroy);
	list_insert(&compositor->regions, &region->link);
}

const static struct wl_compositor_interface wl_compositor_impl = {
	.create_region = wl_compositor_request_create_region,
	.create_surface = wl_compositor_request_create_surface,
};

static void wl_compositor_bind(wl_client_t *client,
							   void		   *data,
							   uint32_t		version,
							   uint32_t		id)
{
	wlx_compositor_global_t *compositor = data;
	wl_resource_t			*resource = wl_resource_create(client, &wl_compositor_interface, version, id);
	wl_resource_set_implementation(resource, &wl_compositor_impl, compositor, wl_compositor_resource_destroy);
}

// -------- compositor global --------

int wlx_compositor_init(wlx_compositor_global_t *compositor,
						wlx_server_t			*server)
{
	compositor->server = server;
	list_init(&compositor->regions);
	list_init(&compositor->surfaces);
	compositor->global = wl_global_create(server->wl_display, &wl_compositor_interface, wl_compositor_interface.version, compositor, wl_compositor_bind);
	return 0;
}

void wlx_compositor_free(wlx_compositor_global_t *compositor)
{
	wl_global_destroy(compositor->global);

	wlx_region_resource_t *region, *tmp_region;
	list_for_each_safe (region, tmp_region, &compositor->regions, link)
		wl_resource_destroy(region->resource);

	wlx_surface_resource_t *surface, *tmp_surface;
	list_for_each_safe (surface, tmp_surface, &compositor->surfaces, link)
		wl_resource_destroy(surface->resource);
}
