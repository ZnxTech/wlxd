#include "ext.h"

#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/randr.h>
#include <xcb/shm.h>
#include <xcb/xfixes.h>

#define wlx_get_extention_data(xcb, x_ext_manager, ext_name)					\
	const xcb_query_extension_reply_t *ext_name ## _ext;						\
	ext_name ## _ext = xcb_get_extension_data(xcb, &xcb_ ## ext_name ## _id);	\
	x_ext_manager->ext_name.present = ext_name ## _ext->present;				\
	x_ext_manager->ext_name.opcode  = ext_name ## _ext->major_opcode;			\
	x_ext_manager->ext_name.f_error = ext_name ## _ext->first_error;			\
	x_ext_manager->ext_name.f_event = ext_name ## _ext->first_event;

void x_ext_manager_init(x_ext_manager_t *x_ext_manager,
						wlx_server_t	*server)
{
	x_ext_manager->server = server;

	xcb_connection_t				  *xcb = wlx_server_get_xcb(server);
	const xcb_query_extension_reply_t *ext;

	xcb_prefetch_extension_data(xcb, &xcb_dri3_id);
	xcb_prefetch_extension_data(xcb, &xcb_present_id);
	xcb_prefetch_extension_data(xcb, &xcb_randr_id);
	xcb_prefetch_extension_data(xcb, &xcb_shm_id);
	xcb_prefetch_extension_data(xcb, &xcb_xfixes_id);

	wlx_get_extention_data(xcb, x_ext_manager, dri3);
	wlx_get_extention_data(xcb, x_ext_manager, present);
	wlx_get_extention_data(xcb, x_ext_manager, randr);
	wlx_get_extention_data(xcb, x_ext_manager, shm);
	wlx_get_extention_data(xcb, x_ext_manager, xfixes);
}
