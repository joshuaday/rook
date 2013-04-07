#define NDIRS (8)
#define NMOVES (9)
#define NITEMS (6)
#define MAXUNDO (4)

enum futures {
	FUTURE_MOVE = 0,
	FUTURE_WAIT = NDIRS,
	FUTURE_FIRE = NMOVES,
	FUTURE_APPLY = NMOVES + NDIRS,
	NCHOICES = NMOVES + NDIRS + NITEMS
};

enum commands {
	cmd_move,
	cmd_wait,
	cmd_apply,
	cmd_remove,
	cmd_equip,
	cmd_drop,
	cmd_fire,

	cmd_quit,
	cmd_reset,
	cmd_hypothetical,
};

enum agentflags {
	AGENT_PLAYER = 1,
	AGENT_WORN = 2,
	AGENT_VULNERABLE = 4,
	AGENT_CAPTURES = 8,
	AGENT_ISITEM = 16,
	AGENT_SHAMEFUL = 32
};

const static struct binding {
	int command;
	int key;
	int dx, dy;
} bindings[] = {
	{cmd_move, 'h', -1,  0},
	{cmd_move, 'j',  0,  1},
	{cmd_move, 'k',  0, -1},
	{cmd_move, 'l',  1,  0},
	{cmd_move, 'y', -1, -1},
	{cmd_move, 'u',  1, -1},
	{cmd_move, 'b', -1,  1},
	{cmd_move, 'n',  1,  1},
	{cmd_wait, '.',  1,  1},

	{cmd_move, '4', -1,  0},
	{cmd_move, '2',  0,  1},
	{cmd_move, '8',  0, -1},
	{cmd_move, '6',  1,  0},
	{cmd_move, '7', -1, -1},
	{cmd_move, '9',  1, -1},
	{cmd_move, '1', -1,  1},
	{cmd_move, '3',  1,  1},
	{cmd_wait, '5',  1,  1},

	{cmd_apply, '\n'},
	{cmd_drop,  '-'},

	{cmd_apply, 'a'},
	{cmd_remove,'r'},
	{cmd_fire,'f'},
	{cmd_equip, 'e'},
	{cmd_drop,  'd'},

	{cmd_quit, 'Q'},
	{cmd_reset, 'R'},
	{cmd_hypothetical, 'H'},
	{0, 0, 0, 0}
};

struct command {
	int future;

	int command;
	int dx, dy, dir;
};

struct layout {
	int left, top, width, height;
	int shiftx, shifty;
	int lastItem;
};


typedef struct agentlist {
	agent *buffer;
	int max, count;
} agentlist;

typedef struct worldstate {
	int locked; // set to 1 when we're just testing what might happen; then we fork and set to 0 and try again for real movement
	int donotfree; // set to 1 for any worldstate that is referred to by multiple futures (invalid_future, particularly)

	int isvalid, ischecked, iswait, victory;
	int turk_active, turk_fake;
	int totaltime;

	rng randomness;
	agentlist agents; // I do not like this anymore
	agent *player; // this is a pointer into the agentlist's buffer, or NULL if the player is dead
	struct layout *layout;
	gameboard *board;
	chronicle *chron;

	layer *player_fov, *player_dijkstra, *player_dijkstra_range; // player_fov is managed externally, player_dijkstra internally
	agent inventory[NITEMS];
} worldstate;

typedef struct outcomes {
	int checkmate, request;

	worldstate *undo[MAXUNDO];
	worldstate *prior;
	worldstate *future[NCHOICES];
} outcomes;


void findsafety(worldstate *state, int *xbest, int *ybest, int kind);
agent *agent_at(worldstate *state, int x, int y);

