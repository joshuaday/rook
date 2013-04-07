#include <stdint.h>

typedef struct rng {
	uint32_t seed, count; // just for bookkeeping
	uint32_t a, b, c, d;
} rng;

enum nameflags {
	NAME_ONE = 0,
	NAME_PLURAL = 1,
	NAME_THE = 2,
	NAME_A = 4,
	NAME_CAPS = 8
};

enum cellflags {
	CELL_STOP = 1,
	CELL_KILL = 2,
	CELL_BLOCK = 4,
	CELL_PROMOTE_ON_STEP = 8
};

enum mobflags {
	MOB_PATHS = 2,
	MOB_DUMB = 4,
	MOB_ATTACKS_FRIENDS = 8,
	MOB_MOVE_ON_CAPTURE = 16,
	MOB_ROYAL_CAPTURE = 32,
	MOB_FLIES = 64
};

enum effects {
	outcome_invalid,
	outcome_like_waiting,
	outcome_needs_forking
};

typedef struct mondef {
	struct forms_t {
		const char *tag;
		const char *name, *plural;
		const char *desc;
	} forms;
	struct asabout_t {
		int glyph, color;
	} about;
	struct ascell_t {
		int flags;
		const char *promotion;
	} ascell;
	struct asmob_t {
		int hp, mflags;
		const char *stab, *range;
		const char *stumble, *sting;
		const char *speed;
	} asmob;
	struct asitem_t {
		const char *magic;
		const char *stab;
	} asitem;
} mondef;

typedef struct agent {
	const mondef *def;
	int x, y, flags;
	int hp, maxhp;

	int time;
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
	int turn_number, count;
} chronicle;

typedef struct gameboard {
	int width, height, size; // size = width * height, for ease
	mondef **cells;

	int fork_count;
} gameboard;


extern const struct direction {
	char *name;
	int key;
	int dx, dy;
} dirs[9];

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
	int (*roll)(rng *, const char *);
} Rng;

extern struct game_t {
	void (*loop)();
} Game;


extern struct board_t {
	gameboard *(*new)(int, int);
	gameboard *(*fork)(gameboard *);
	void (*free)(gameboard *);

	mondef *(*get)(gameboard *, int, int);
	mondef *(*set)(gameboard *, int, int, const mondef *);
	void (*generate)(gameboard *, rng *);
	void (*fov)(gameboard *, agent *, layer *);
	void (*dijkstra)(gameboard *, agent *, int, layer *);
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
	struct builder_t (*name)(const mondef *, int);
	char *(*read)();
	void (*publish)(chronicle *);
	void (*enable)(int);
} Builder;

extern struct layer_t {
	layer *(*new)(int, int);
	void (*fill)(layer *, int);
	void (*set)(layer *, int, int, int);
	int (*get)(layer *, int, int);
	void (*recenter)(layer *, int, int);

	// add fork and free and all will be well and awesome
} Layer;

struct worldstate;
extern struct items_t {
	enum effects (*check_apply_effect)(struct worldstate *, agent *, int);
	enum effects (*do_apply_effect)(struct worldstate *, agent *, int);
	enum effects (*check_fire_effect)(struct worldstate *, agent *, int, int);
	enum effects (*do_fire_effect)(struct worldstate *, agent *, int, int);
} Items;

