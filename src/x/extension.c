#include "extension.h"

#include <xcb/randr.h>
#include <xcb/xinput.h>
#include <xcb/xkb.h>

xcb_extension_t *X_EXTENSION_IDS[] = {
	&xcb_randr_id,
	&xcb_input_id,
	&xcb_xkb_id,
};

void x_extension_holder_init(x_extension_holder_t *x_ext_holder,
							 xcb_connection_t	  *x_display)
{
	x_extension_t					  *exts = (x_extension_t *)x_ext_holder;
	const xcb_query_extension_reply_t *reply;
	for (int i = 0; i < X_EXTENSION_C; i++) {
		reply = xcb_get_extension_data(x_display, X_EXTENSION_IDS[i]);
		exts[i].present = reply->present;
		exts[i].m_opcode = reply->major_opcode;
		exts[i].f_event = reply->first_event;
		exts[i].f_error = reply->first_error;
	}
}
