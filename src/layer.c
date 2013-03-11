#include "mods.h"
#include <stdlib.h>

layer *layer_new(int w, int h) {
	layer *bob = malloc(sizeof(layer) + (sizeof(int) * w * h)); // these +1s are hacks and shouldn't be here

	if (bob == NULL) return NULL;

	bob->width = w;
	bob->height = h;

	bob->x1 = 0;
	bob->y1 = 0;
	bob->x2 = w - 1;
	bob->y2 = h - 1;

	bob->oob = 0;

	Layer.fill(bob, 0);
	return bob;
}

void layer_fill(layer *layer, int value) {
	int i, length = layer->width * layer->height;
	for (i = 0; i < length; i++) {
		layer->buffer[i] = value;
	}
}

void layer_set(layer *layer, int x, int y, int value) {
	if (x < layer->x1 || y < layer->y1 || x > layer->x2 || y > layer->y2) {
		return;
	} else {
		int index = (x - layer->x1) + (y - layer->y1) * layer->width;
		layer->buffer[index] = value;
	}
}

int layer_get(layer *layer, int x, int y) {
	if (x < layer->x1 || y < layer->y1 || x > layer->x2 || y > layer->y2) {
		return layer->oob;
	} else {
		int index = (x - layer->x1) + (y - layer->y1) * layer->width;
		return layer->buffer[index];
	}
}

void layer_recenter(layer *layer, int x, int y) {
	layer->x1 = x - layer->width / 2;
	layer->y1 = y - layer->height / 2;

	layer->x2 = layer->x1 + layer->width - 1;
	layer->y2 = layer->y1 + layer->height - 1;
}

struct layer_t Layer = {
	layer_new,
	layer_fill,
	layer_set,
	layer_get,
	layer_recenter
};

