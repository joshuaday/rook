#include <stdint.h>

typedef struct rng {
	uint32_t seed, count; // just for bookkeeping
	uint32_t a, b, c, d;
} rng;

enum cellflags {
	CELL_STOP = 1,
	CELL_KILL = 2,
	CELL_BLOCK = 4,
	CELL_PROMOTE_ON_STEP = 8
};

enum mobflags {
	MOB_STUMBLE = 1,
	MOB_PATHS = 2,
	MOB_DUMB = 4,
	MOB_ATTACKS_FRIENDS = 8,
	MOB_MOVE_ON_CAPTURE = 16,
	MOB_ROYAL_CAPTURE = 32
};

typedef struct mondef {
	struct asabout_t {
		int glyph, color;
		const char *name, *description;
	} about;
	struct ascell_t {
		int flags;
		const char *promotion;
	} ascell;
	struct asmob_t {
		int hp, mflags, damage;
	} asmob;
	struct asitem_t {
		int hacktype;
	} asitem;
} mondef;

typedef struct agent {
	const mondef *def;
	int x, y, flags;
	int hp, maxhp;
} agent;

typedef struct layer {
	int width, height;
	int x1, y1, x2, y2;

	int oob;
	int buffer[0];
} layer;

typedef struct message {
	int turn_number;
	char *text;
	int length;
	struct message *prev;

	int fork_count;
} message;

typedef struct chronicle {
	struct message *last;
	int turn_number;
} chronicle;

typedef struct gameboard {
	int width, height, size; // size = width * height, for ease
	mondef **cells;
	agent **mob; // one mob or item per cell
} gameboard;


extern struct term_t {
	int (*init)();
	void (*plot)(int, int, int, int);
	void (*print)(int, int, int, const char *);
	void (*clear)( );
	int (*get)();
	void (*refresh)();

	int width, height;
} Term;

extern struct rng_t {
	rng (*seed)(uint32_t);
	uint32_t (*randint)(rng *, uint32_t);
} Rng;

extern struct game_t {
	void (*loop)();
} Game;


extern struct board_t {
	gameboard (*new)(int, int);
	mondef *(*get)(gameboard *, int, int);
	mondef *(*set)(gameboard *, int, int, const mondef *);
	void (*generate)(gameboard *, rng *);
	void (*fov)(gameboard *, agent *, layer *);
	void (*dijkstra)(gameboard *, agent *, layer *);
} Board;

extern struct catalog_t {
	const mondef *(*lookup)(int, const char *);
	int Monster, Item, Cell;
} Catalog;

extern struct chronicle_t {
	chronicle *(*fork)(chronicle *);
	void (*post)(chronicle *, const char *);
	void (*free)(chronicle *);
} Chronicle;

extern struct builder_t {
	struct builder_t (*clear)();
	struct builder_t (*write)(const char *);
	char *(*read)();
	void (*publish)(chronicle *);
} Builder;

extern struct layer_t {
	layer *(*new)(int, int);
	void (*fill)(layer *, int);
	void (*set)(layer *, int, int, int);
	int (*get)(layer *, int, int);
	void (*recenter)(layer *, int, int);
} Layer;

