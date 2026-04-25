#ifndef UTILS_XID_MAP_H
#define UTILS_XID_MAP_H

#include <stdint.h>

#include <xcb/xcb.h>

#include "../server.h"

// defines the range of the XIDs that the node is made to store
// for e.g. node with an xid_min of 128 will store all XIDs from and including
// 128 to 255 in addition a node's XID range is always snapped to jumps sized
// NODE_RANGE to avoid overlaps e.g. 0 to 127, 128 to 255, 512 to 639, etc.
#define NODE_RANGE 128

// the uninitialized node count that a xid_map_t is initialized with.
#define NODE_INITC 1

typedef struct xid_node {
	uint32_t xid_min;
	uint32_t count;
	void   **values;
} xid_node_t;

typedef struct xid_map {
	xid_node_t *nodes;
	size_t		size;
} xid_map_t;

xid_map_t *wlx_server_get_xid_map(wlx_server_t *server);

#define wlx_get_server(wlx_object) \
    (wlx_server_t *)((uint8_t *)(wlx_object) + offsetof(typeof(*wlx_object), server))

#define wlx_get_xid_map(wlx_object) \
    wlx_server_get_xid_map(wlx_get_server(wlx_object))

void xid_map_init(xid_map_t *map);

void xid_map_release(xid_map_t *map);

void xid_map_insert(xid_map_t *map,
					uint32_t   xid,
					void	  *data);

void xid_map_remove(xid_map_t *map,
					uint32_t   xid);

void *xid_map_search(xid_map_t *map,
					 uint32_t	xid);

#endif // UTILS_XID_MAP_H
