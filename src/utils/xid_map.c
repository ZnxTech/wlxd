#include "xid_map.h"

#include <stdlib.h>
#include <string.h>

static inline bool xid_node_is_empty(xid_node_t node)
{
	return !node.values && !node.count;
}

static inline bool xid_node_is_contained(xid_node_t node,
										 uint32_t	xid)
{
	return node.xid_min <= xid && xid < node.xid_min + NODE_RANGE;
}

static void xid_node_release(xid_node_t *node)
{
	free(node->values);
	node->values = NULL;
}

static void xid_map_expand(xid_map_t *map)
{
	map->nodes = realloc(map->nodes, map->size * 2 * sizeof(xid_node_t));
	memset(map->nodes + map->size, 0, sizeof(xid_node_t) * map->size);
	map->size *= 2;
}

static xid_node_t *xid_map_get_or_create_node(xid_map_t *map,
											  uint32_t	 xid)
{
	xid_node_t *node = NULL;
	for (size_t i = 0; i < map->size; i++) {
		if (xid_node_is_empty(map->nodes[i]) && !node)
			node = &map->nodes[i];
		else if (xid_node_is_contained(map->nodes[i], xid))
			return &map->nodes[i];
	}

	if (!node) {
		size_t old_size = map->size;
		xid_map_expand(map);
		node = &map->nodes[old_size];
	}

	node->count = 0;
	node->xid_min = xid - (xid % NODE_RANGE);
	node->values = calloc(NODE_RANGE, sizeof(void *));
	return node;
}

static xid_node_t *xid_map_get_node(xid_map_t *map,
									uint32_t   xid)
{
	for (size_t i = 0; i < map->size; i++) {
		if (xid_node_is_contained(map->nodes[i], xid) && !xid_node_is_empty(map->nodes[i]))
			return &map->nodes[i];
	}

	return NULL;
}

void xid_map_init(xid_map_t *map)
{
	map->nodes = calloc(NODE_INITC, sizeof(xid_node_t));
	map->size = NODE_INITC;
}

void xid_map_release(xid_map_t *map)
{
	for (size_t i = 0; i < map->size; i++) {
		if (!xid_node_is_empty(map->nodes[i]))
			free(map->nodes[i].values);
	}

	free(map->nodes);
}

void xid_map_insert(xid_map_t *map,
					uint32_t   xid,
					void	  *data)
{
	xid_node_t *node = xid_map_get_or_create_node(map, xid);
	uint32_t	i = xid % NODE_RANGE;
	if (!node->values[i])
		node->count++;

	node->values[i] = data;
}

void xid_map_remove(xid_map_t *map,
					uint32_t   xid)
{
	xid_node_t *node = xid_map_get_node(map, xid);
	if (!node)
		return;

	uint32_t i = xid % NODE_RANGE;
	if (node->values[i])
		node->count--;

	node->values[i] = NULL;
	if (!node->count)
		xid_node_release(node);
}

void *xid_map_search(xid_map_t *map,
					 uint32_t	xid)
{
	xid_node_t *node = xid_map_get_node(map, xid);
	uint32_t	i = xid % NODE_RANGE;
	return (node) ? node->values[i] : NULL;
}
