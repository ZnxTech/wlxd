#ifndef UTILS_ARRAY_H
#define UTILS_ARRAY_H

#include <wayland-util.h>

typedef struct wl_array array_t;

#define array_init    wl_array_init
#define array_add     wl_array_add
#define array_copy    wl_array_copy
#define array_release wl_array_release

#define array_for_each wl_array_for_each

#endif // UTILS_ARRAY_H
