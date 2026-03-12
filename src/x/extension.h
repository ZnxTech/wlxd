#ifndef X_EXTENSION_H
#define X_EXTENSION_H

#include <stdint.h>

#include <xcb/xcb.h>

#define X_EXTENSION_C (sizeof(x_extension_holder_t)/sizeof(x_extension_t))

typedef struct x_extension {
	uint8_t present;
	uint8_t m_opcode;
	uint8_t f_event;
	uint8_t f_error;
} x_extension_t;

typedef struct x_extension_holder {
	x_extension_t randr;
	x_extension_t xinput;
	x_extension_t xkb;
} x_extension_holder_t;

void x_extension_holder_init(x_extension_holder_t *x_ext_holder,
							 xcb_connection_t	  *x_display);

#endif // X_EXTENSION_H
