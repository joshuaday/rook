#include "mods.h"
#include <stdlib.h>

static gameboard *board_new(int width, int height) {
	gameboard *init = malloc(sizeof(gameboard));

	init->width = width;
	init->height = height;
	init->size = init->width * init->height;

	init->cells = malloc(sizeof(mondef *) * init->size);
	init->fork_count = 0;
	
	// not checking cells or mob for null -- address that

	int i;
	const mondef *nothing = Catalog.lookup(Catalog.Cell, "blank");
	for (i = 0; i < init->size; i++) {
		init->cells[i] = nothing;
	}

	return init;
}

static gameboard *board_fork(gameboard *old) {
	gameboard *init = malloc(sizeof(gameboard));

	init->width = old->width;
	init->height = old->height;
	init->size = init->width * init->height;

	init->cells = malloc(sizeof(mondef *) * init->size);
	init->fork_count = 0;
	
	int i;
	for (i = 0; i < init->size; i++) {
		init->cells[i] = old->cells[i];
	}

	return init;
}

static void board_free(gameboard *old) {
	if (old->fork_count == 0) {
		free(old->cells);
		free(old);
	} else {
		old->fork_count--;
	}
}

static mondef *get(gameboard *board, int x, int y) {
	if (x < 0 || y < 0 || x >= board->width || y >= board->height) {
		return 0; //Catalog.Unseen;
	}

	int index = x + y * board->width;
	return board->cells[index];
}

static mondef *set(gameboard *board, int x, int y, const mondef *cell) {
	if (cell == NULL) return 0;

	if (x < 0 || y < 0 || x >= board->width || y >= board->height) {
		return NULL; //Catalog.Unseen;
	}

	int index = x + y * board->width;
	mondef *oldcell = board->cells[index];
	board->cells[index] = cell;

	return oldcell;
}


typedef struct fovhead {
	int idx, len;
	double angle[2000];
	int blocked[2000];
} fovhead;

fovhead losbuf[2];

int fovhead_zoomto(fovhead *dest, fovhead *src, double close, int block) {
	// do not try to make sense of this -- only a few hours to go --
	// lots of thrashing -- not good code -- 
	int haslos = src->len == 0;
	double endangle = 0.0;

	while (src->idx < src->len) {
		endangle = src->angle[src->idx];
		int breakout = 0;
		if (endangle >= close) {
			endangle = close;
			breakout = 1;
		}

		if (!src->blocked[src->idx]) {
			haslos = 1;
		}

		if (!block) {
			dest->angle[dest->idx] = endangle;
			dest->blocked[dest->idx] = src->blocked[src->idx];
			dest->idx++;
		}

		if (breakout) {
			break;
		} else {
			src->idx++;
		}
	}
	
	if (block) {
		dest->angle[dest->idx] = close;
		dest->blocked[dest->idx] = 1;
		dest->idx++;
	} else {
		if (endangle < close) {
			dest->angle[dest->idx] = close;
			dest->blocked[dest->idx] = 0;
			dest->idx++;
		}
	}

	return haslos;
}

static void fov(gameboard *board, agent *viewer, layer *output) {
	// toggle off, see: fov open, fov close, each loop around the spiral
	double x, y, z, t;
	fovhead *src, *dest;

	int range = (output->height > output->width ? output->height : output->width) / 2;

	if (viewer == NULL || board == NULL || output == NULL) {
		return;
	}

	src = losbuf + 0;
	dest = losbuf + 1;
	src->len = 0;

	// even to odd is an open range

	Layer.recenter(output, viewer->x, viewer->y);
	Layer.fill(output, 0);
	Layer.set(output, viewer->x, viewer->y, 1);

	for (z = 1; z < range; z += 1.0) {
		x = viewer->x - z;
		y = viewer->y - z;

		int side;
		double sidelength = 2.0 * z;
		double length = 4.0 * sidelength;
		double cellnumber = 0.0;

		src->idx = 0;
		dest->idx = 0;

		for (side = 0; side < 4; side++) {
			if (side == 3.0) sidelength += 1.0;
			for (t = 0.0; t < sidelength; t += 1.0) {
				double close = (cellnumber + 0.5) / length;

				mondef *cell = get(board, x, y);
				int cellisopaque = (cell == NULL || (cell->ascell.flags & CELL_BLOCK));
				int haslos = fovhead_zoomto(dest, src, close, cellisopaque);
				Layer.set(output, x, y, haslos);
				
				if (side == 0) x++;
				if (side == 1) y++;
				if (side == 2) x--;
				if (side == 3) y--;

				cellnumber += 1.0;
			}
		}

		dest->len = dest->idx;

		// swap!
		fovhead *swap = src;
		src = dest;
		dest = swap;
	}
}

struct frontier_t {
	int x, y, score;
} frontier[2000];

#define PUSH_FRONTIER_SCORE(x, y, s) frontier[nfrontier++] = (struct frontier_t) {(x), (y), (s)}
#define PUSH_FRONTIER(x, y) frontier[nfrontier++] = (struct frontier_t) {(x), (y), 0}

// this is a stripped-down form of dijkstra based on constant costs
static void dijkstra(gameboard *board, agent *target, int style, layer *out) {
	int nfrontier = 0;
	
	if (board == NULL || target == NULL || out == NULL) {
		return;
	}

	int maxpath = out->width + out->height;
	
	out->oob = -1;
	Layer.recenter(out, target->x, target->y);
	Layer.fill(out, maxpath);

	// instead of doing this here, we can expose a queue, but that will have some overhead
	if (style) {
		// note that there's some post-processing for the archer ai, too
		int dir, i;
		for (dir = 0; dir < 8; dir++) {
			int x = target->x, y = target->y;
			for (i = 1; i < 5; i++) {
				x += dirs[dir].dx;
				y += dirs[dir].dy;
				
				const mondef *it = Board.get(board, x, y);
				if (it == NULL || (it->ascell.flags & (CELL_BLOCK))) {
					break;
				}

				if (i > 1) {
					PUSH_FRONTIER_SCORE(x, y, 0);
				}
			}
		}
	} else {
		PUSH_FRONTIER_SCORE(target->x, target->y, 0);
	}

	while (nfrontier > 0) {
		int idx = 0;
		int x = frontier[idx].x;
		int y = frontier[idx].y;
		int newscore = frontier[idx].score;
		// frontier[idx] = frontier[--nfrontier]; // wrong

		// temporary as hell
		int i;
		nfrontier = nfrontier - 1;
		for (i = idx; i < nfrontier; i++) {
			frontier[i] = frontier[i+1];
		}

		const mondef *it = Board.get(board, x, y);
		if (it == NULL || (it->ascell.flags & (CELL_STOP | CELL_KILL))) {
			// impassable
			Layer.set(out, x, y, 1 + maxpath);
		} else {
			int oldscore = Layer.get(out, x, y);
			if (newscore < oldscore) { // we don't already have a better path here;
				Layer.set(out, x, y, newscore);
				PUSH_FRONTIER_SCORE(x + 1, y, newscore + 1);
				PUSH_FRONTIER_SCORE(x - 1, y, newscore + 1);
				PUSH_FRONTIER_SCORE(x, y + 1, newscore + 1);
				PUSH_FRONTIER_SCORE(x, y - 1, newscore + 1);
			}
		}
	}

	out->oob = maxpath + 1;
}

static void makepond(gameboard *board, rng *r, int size, const char *fluid_name) {
	const mondef *blank = Catalog.lookup(Catalog.Cell, "blank");
	const mondef *fluid = Catalog.lookup(Catalog.Cell, fluid_name);

	int x, y;
	int nfrontier = 0;

	while (size > 0) {
		x = Rng.randint(r, board->width);
		y = Rng.randint(r, board->height);
		if (Board.get(board, x, y) == blank) {
			break;
		} else {
			// try again
			size--;
		}
	}

	while (size > 0) {
		if (Board.get(board, x, y) == blank) {
			Board.set(board, x, y, fluid);
			PUSH_FRONTIER(x + 1, y);
			PUSH_FRONTIER(x - 1, y);
			PUSH_FRONTIER(x, y + 1);
			PUSH_FRONTIER(x, y - 1);
			size--;
		}
		if (nfrontier == 0) break;
		if (size <= 0) break;
		
		int idx = Rng.randint(r, nfrontier);
		x = frontier[idx].x;
		y = frontier[idx].y;
		frontier[idx] = frontier[--nfrontier];
	}
}

static void makehouse(gameboard *board, rng *r) {
	const mondef *blank = Catalog.lookup(Catalog.Cell, "blank");

	int x, y, dx, dy;
	int w = 13, h = 9;

	while (w > 4 && h > 4) {
		int invalid = 0;

		x = Rng.randint(r, board->width);
		y = Rng.randint(r, board->height);
		for (dx = 0; dx < w && !invalid; dx++) {
			for (dy = 0; dy < h && !invalid; dy++) {
				if (Board.get(board, x + dx, y + dy) != blank) {
					invalid = 1;
				}
			}
		}
		
		if (!invalid) {
			break;
		} else {
			// try again
			if (Rng.randint(r, w + h) > h) {
				w--;
			} else {
				h--;
			}
		}
	}

	if (w <= 4 || h <= 4) {
		return;
	}

	const mondef *grass = Catalog.lookup(Catalog.Cell, "grass");
	const mondef *wall = Catalog.lookup(Catalog.Cell, "wall");
	const mondef *floor = Catalog.lookup(Catalog.Cell, "floor");
	const mondef *door = Catalog.lookup(Catalog.Cell, "door");
	const mondef *window = Catalog.lookup(Catalog.Cell, "window");

	int ndoors = 0;

	
	x += 1; y += 1; w -= 2; h -= 2;
	for (dx = 0; dx < w; dx++) {
		for (dy = 0; dy < h; dy++) {
			int sidecount = 0;
			if (dx == 0 || dx == w - 1) sidecount++;
			if (dy == 0 || dy == h - 1) sidecount++;

			if (sidecount == 1) Board.set(board, x + dx, y + dy, grass);
		}
	}

	while (ndoors == 0) {
		for (dx = 0; dx < w; dx++) {
			for (dy = 0; dy < h; dy++) {
				int sidecount = 0;
				if (dx == 0 || dx == w - 1) sidecount++;
				if (dy == 0 || dy == h - 1) sidecount++;

				if (sidecount == 0) {
					Board.set(board, x + dx, y + dy, floor);
				} else if (sidecount == 1) {
					int tp = Rng.randint(r, 6);
					if (tp == 0) {
						Board.set(board, x + dx, y + dy, door);
						ndoors++;
					} else if (tp == 1) {
						Board.set(board, x + dx, y + dy, window);
					} else {
						Board.set(board, x + dx, y + dy, wall);
					}
				} else if (sidecount == 2) {
					Board.set(board, x + dx, y + dy, wall);
				}
			}
		}
	}
}

static void generate(gameboard *board, rng *r) {
	int i;
	const mondef *blank = Catalog.lookup(Catalog.Cell, "blank");
	const mondef *forest = Catalog.lookup(Catalog.Cell, "forest");
	const mondef *grass = Catalog.lookup(Catalog.Cell, "grass");
	const mondef *brush = Catalog.lookup(Catalog.Cell, "brush");
	const mondef *tree = Catalog.lookup(Catalog.Cell, "tree");

	for (i = 0; i < board->size / 300; i++) {
		makepond(board, r, 15, "brush");
	}

	for (i = 0; i < board->size / 300; i++) {
		makepond(board, r, 45, "forest");
	}

	for (i = 0; i < board->size / 300; i++) {
		makepond(board, r, 30, "water");
	}

	for (i = 0; i < board->size / 200; i++) {
		makehouse(board, r);
	}

	for (i = 0; i < board->size; i++) {
		if (board->cells[i] == forest) {
			int rn = Rng.randint(r, 30);
			if (rn < 4) {
				board->cells[i] = tree;
			} else if (rn < 9) {
				board->cells[i] = brush;
			} else {
				board->cells[i] = grass;
			}
		} else if (board->cells[i] == blank) {
			int rn = Rng.randint(r, 30);
			if (rn == 0) {
				board->cells[i] = tree;
			}
		}
	}

	for (i = 0; i < board->size; i++) {
		if (board->cells[i] == blank) board->cells[i] = grass;
	}
}

struct board_t Board = {
	board_new,
	board_fork,
	board_free,

	get,
	set,
	generate,
	fov,
	dijkstra
};

