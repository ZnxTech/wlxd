#ifndef WLX_UTILS_LIST_H
#define WLX_UTILS_LIST_H

#include <wayland-util.h>

typedef struct wl_list list_t;

#define list_init        wl_list_init
#define list_insert      wl_list_insert
#define list_insert_list wl_list_insert_list
#define list_empty       wl_list_empty
#define list_length      wl_list_length
#define list_remove      wl_list_remove

#define list_container_of          wl_container_of
#define list_for_each              wl_list_for_each
#define list_for_each_safe         wl_list_for_each_safe
#define list_for_each_reverse      wl_list_for_each_reverse
#define list_for_each_reverse_safe wl_list_for_each_reverse_safe

#endif // WLX_UTILS_LIST_H
