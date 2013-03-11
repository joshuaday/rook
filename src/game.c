#include "mods.h"
#include <stdlib.h>
#include <stdio.h>

#define IS_CHOICE_MOVEMENT(c) ((c) >= 0 && (c) < 8)
#define FRIENDLY_MOBS(a, b) (((a)->flags & AGENT_PLAYER) == ((b)->flags & AGENT_PLAYER))

#define NDIRS (8)
#define APPLY (9)
#define NMOVES (14)
const static struct direction {
	char *name;
	int key;
	int dx, dy;
} dirs[14] = {
	{"northwest", 'y', -1, -1},
	{"north", 'k', 0, -1},
	{"northeast", 'u', 1, -1},
	{"east", 'l', 1, 0},
	{"southeast", 'n', 1, 1},	
	{"south", 'j', 0, 1},
	{"southwest", 'b', -1, 1},
	{"west", 'h', -1, 0},

	{"wait", '.', 0, 0},

	{"apply a", 'a', -2, 4},
	{"apply b", 'b', -2, 5},
	{"apply c", 'c', -2, 6},
	{"apply d", 'd', -2, 7},
	{"apply e", 'e', -2, 8}
};

int reverse_direction[9] = {0, 1, 2, 7, 8, 3, 6, 5, 4};
int *reverse_rows[3] = {&reverse_direction[1], &reverse_direction[4], &reverse_direction[7]};
int **dir_lookup = &reverse_rows[1];


enum commands {
	cmd_move,
	cmd_wait,
	cmd_apply,
	cmd_drop,
	cmd_quit
};

enum agentflags {
	AGENT_PLAYER = 1,
	AGENT_WORN = 2,
	AGENT_VULNERABLE = 4,
	AGENT_CAPTURES = 8,
	AGENT_ISITEM = 16
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
	// {cmd_drop,  '-'},

	{cmd_apply, 'a'},
	// {cmd_drop,  'd'},

	{cmd_quit, 'Q'},
	{0, 0, 0, 0}
};

struct layout {
	int left, top, width, height;
	int shiftx, shifty;
};


typedef struct agentlist {
	agent *buffer;
	int max, count;
} agentlist;

typedef struct worldstate {
	int isvalid, ischecked, victory;
	int orb_active;

	rng randomness;
	agentlist agents;
	agent *player; // this is a pointer into the agentlist's buffer, or NULL if the player is dead
	struct layout *layout;
	gameboard *board;
	chronicle *chron;

	layer *player_fov, *player_dijkstra; // player_fov is managed externally, player_dijkstra internally
	agent inventory[5];
} worldstate;

typedef struct outcomes {
	int checkmate;

	worldstate *prior;
	worldstate *movement[NMOVES];
} outcomes;

static void findsafety(worldstate *state, int *xbest, int *ybest);

static agentlist newagentlist(int max) {
	agentlist dest;
	dest.max = max; //4 + src->count + src->count / 2;
	dest.count = 0;
	dest.buffer = malloc(sizeof(agent) * dest.max);
	return dest;
}

static agentlist copyagentlist(agentlist *src) {
	int i, j;

	agentlist dest = newagentlist(4 + src->count);

	for (i = 0, j = 0; i < src->count; i++) {
		if (src->buffer[i].hp > 0) {
			dest.buffer[j++] = src->buffer[i];
		}
	}

	dest.count = j;

	return dest;
}

static worldstate *newworldstate( ) {
	worldstate *state = malloc(sizeof(worldstate));
	state->randomness = Rng.seed(0);
	state->agents = newagentlist(16);

	state->victory = 0;
	state->ischecked = 0;
	state->isvalid = 1;

	state->player = NULL;
	state->player_fov = NULL;
	state->player_dijkstra = NULL;
	state->layout = NULL;
	state->board = NULL;
	state->chron = Chronicle.fork(NULL);

	int i;
	for (i = 0; i < 5; i++) {
		state->inventory[i] = (agent){NULL};
	}
	state->inventory[0] = (agent){Catalog.lookup(Catalog.Item, "orb of the Turk"), .flags = AGENT_WORN};
	state->inventory[1] = (agent){Catalog.lookup(Catalog.Item, "ring of vulnerability"), .flags = AGENT_WORN};

	state->orb_active = 1;

	return state;
}

static worldstate *copyworldstate(worldstate *old) {
	worldstate *state = malloc(sizeof(worldstate));

	state->randomness = old->randomness;
	state->layout = old->layout;
	state->board = old->board;
	state->orb_active = old->orb_active;

	state->victory = old->victory;
	state->ischecked = old->ischecked;
	state->isvalid = old->isvalid;
	if (old->player_dijkstra != NULL) {
		state->player_dijkstra = Layer.new(old->player_dijkstra->width, old->player_dijkstra->height);
	} else {
		state->player_dijkstra = NULL;
	}

	state->agents = copyagentlist(&old->agents);
	state->player = state->agents.buffer + (old->player - old->agents.buffer);
	state->player_fov = old->player_fov;
	state->chron = Chronicle.fork(old->chron);

	int i;
	for (i = 0; i < 5; i++) {
		state->inventory[i] = old->inventory[i];
	}
	return state;
}

static void freeworldstate(worldstate *old) {
	free(old->agents.buffer);
	Chronicle.free(old->chron);
	if (old->player_dijkstra != NULL) {
		free(old->player_dijkstra);
	}
	free(old);
}

static void agentlist_insert(agentlist *agents, agent *neophyte) {
	if (agents->count == agents->max) {
		int max = agents->max * 2;
		agent *buffer = realloc(agents->buffer, sizeof(agent) * max);
		if (buffer == 0) {

			// no error -- who cares?
			return;
		}

		agents->max = max;
		agents->buffer = buffer;
	}

	agents->buffer[agents->count++] = *neophyte;
}

static void findsafety(worldstate *state, int *xbest, int *ybest) {
	int t, z, x, y, tries = 9, side;

	for (z = 0; z < tries; z++) {
		x = *xbest - z;
		y = *ybest - z;
		int sidelength = 2 * z;
		for (side = 0; side < 4; side++) {
			if (side == 3) sidelength ++;
			for (t = 0; t < sidelength; t++) {
				mondef *cell = Board.get(state->board, x, y);
				if (cell != NULL && !(cell->ascell.flags & (CELL_STOP | CELL_KILL))) {
					int empty = 1, i;
					for (i = 0; i < state->agents.count; i++) {
						agent *a = state->agents.buffer + i;
						if (a->x == x || a->y == y) {
							empty = 0;
							break;
						}
					}
					if (empty) {
						*xbest = x;
						*ybest = y;
						return;
					}
				}
					
				if (side == 0) x++;
				if (side == 1) y++;
				if (side == 2) x--;
				if (side == 3) y--;
			}
		}
	}
}

static void spawn(worldstate *state, const char *tag, int x, int y, int flags) {
	int isitem = 0;
	agent spawnee;
	spawnee.def = Catalog.lookup(Catalog.Monster, tag);

	if (spawnee.def == NULL) {
		spawnee.def = Catalog.lookup(Catalog.Item, tag);
		isitem = 1;
	}

	if (x < 0) x = Rng.randint(&state->randomness, state->board->width);
	if (y < 0) y = Rng.randint(&state->randomness, state->board->height);
	findsafety(state, &x, &y);

	spawnee.x = x;
	spawnee.y = y;
	spawnee.flags = flags;

	spawnee.hp = spawnee.maxhp = spawnee.def->asmob.hp;

	if (isitem) {
		spawnee.hp = 1;
		spawnee.flags |= AGENT_ISITEM;
	}

	agentlist_insert(&state->agents, &spawnee);
}

static agent *ai_find_enemy(worldstate *state, agent *player) {
	agent *target;
	// there are only two teams -- easy!
	if (player->flags & AGENT_PLAYER) {
		// what wants the player with enemy-finding?
		return NULL;
	} else {
		// cheap -- ha!
		target = state->player;
	}

	if (target != NULL) {
		int dx = player->x - target->x, dy = player->y - target->y;
		if (dx >= -10 && dx <= 10 && dy >= -10 && dy <= 10) {
			return target;
		}
	}

	return NULL;
}

static int ai_step_towards(worldstate *state, agent *self, agent *target) {
	if (target != NULL) {
		int dx = target->x - self->x, dy = target-> y - self->y;
		dx = dx < 0 ? -1 : dx > 0 ? 1 : 0;
		dy = dy < 0 ? -1 : dy > 0 ? 1 : 0;
		return dir_lookup[dy][dx];
	}
	
	return NDIRS; // wait
}

static int ai_roll_down(worldstate *state, agent *self, layer *dijkstra) {
	if (dijkstra != NULL) {
		int x = self->x, y = self->y;
		int dir, bestscore, bestdir;
		bestscore = dijkstra->oob;
		bestdir = NDIRS;
		for (dir = 0; dir < NDIRS; dir++) {
			// negate this to get running away!
			int thisscore = Layer.get(dijkstra, x + dirs[dir].dx, y + dirs[dir].dy);
			if (thisscore < bestscore) {
				bestscore = thisscore;
				bestdir = dir;
			}
		}
		return bestdir;
	}

	return NDIRS; // wait
}

static void drawframe(outcomes *the_turk) {
	worldstate *state = the_turk->prior;
	int i, x, y;
	int color = 16 * 7, glyph = ' ';
	for (y = 0; y <= state->layout->height; y++) {
		Term.plot(-1 + state->layout->left, state->layout->top + y, color, glyph);
		Term.plot(state->layout->width + state->layout->left, state->layout->top + y - 1, color, glyph);
	}

	for (x = 0; x <= state->layout->width; x++) {
		Term.plot(state->layout->left + x - 1, -1 + state->layout->top, color, glyph);
		Term.plot(state->layout->left + x, state->layout->height + state->layout->top, color, glyph);
	}

	Term.print(state->layout->left + 1, state->layout->top - 1, color, "Rook");

	char hpbuffer[50];
	sprintf(hpbuffer, "HP: %d/%d", state->player->hp, state->player->maxhp);

	Term.print(state->layout->left + 1, state->layout->height + state->layout->top, color, hpbuffer);

	x = state->layout->left + state->layout->width + 7;
	y = 4;

	Term.print(x - 4, y - 2, 12, "VALID MOVES");

	for (i = 0; i < NMOVES; i++) {
		// illustrate the moves in the compass rose
		if (!the_turk->movement[i]->isvalid) color = 0;
		else if (the_turk->movement[i]->ischecked) color = 11 + 16 * 1;
		else color = 15;
		Term.plot(x + dirs[i].dx, y + dirs[i].dy, color, dirs[i].key);
	}

	Term.print(x - 4, y + 3, 12, "INVENTORY (a to apply)");
	// print out inventory
	for (i = 0; i < 5; i++) { // can you tell that I'm running short on time?
		agent *item = &state->inventory[i];
		if (item->def != NULL) {
			Term.plot(x - 1, y + 4 + i, 15, ')');

			if ((item->flags & AGENT_WORN)) {
				sprintf(hpbuffer, "%s (worn)", item->def->about.name);
				
			} else {
				sprintf(hpbuffer, "%s", item->def->about.name);
			}
			Term.print(x + 2, y + 4 + i, 15, hpbuffer);
		}
	}

	// write out our chronicle! (give the chronicle a window, like the game screen!)
	int message_left = state->layout->left + state->layout->width + 5;

	Term.print(message_left - 2, 24 - 10, 12, "MESSAGES");
	message *msg = the_turk->prior->chron->last;
	int turn = the_turk->prior->chron->turn_number;
	for (i = 0; msg != NULL && i < 10; i++) {
		int x1 = message_left;
		color = turn == msg->turn_number ? 15 : 8;

		y = 24 - i;
		for (x = 0; x < msg->length; x++) {
			Term.plot(x + x1, y, color, msg->text[x]);
		}

		msg = msg->prev;
	}
}

static void drawboard(worldstate *state) {
	int x, y;

	gameboard *board = state->board;
	layer *fov = state->player_fov;
	struct layout *layout = state->layout;

	if (state->player != 0) {
		layout->shiftx = -state->player->x + layout->width / 2;
		layout->shifty = -state->player->y + layout->height / 2;
	}

	for (y = 0; y < layout->height; y++) {
		for (x = 0; x < layout->width; x++) {
			int visible = Layer.get(fov, x - layout->shiftx, y - layout->shifty);
			mondef *cell = Board.get(board, x - layout->shiftx, y - layout->shifty);
			
			if (visible && cell != 0) {
				Term.plot(x + layout->left, y + layout->top, cell->about.color, cell->about.glyph);
			} else {
				Term.plot(x + layout->left, y + layout->top, 0, ' ');
			}
		}
	}
}

static void draw(outcomes *the_turk) {
	int i;

	Term.clear( );

	worldstate *state = the_turk->prior;
	layer *fov = state->player_fov;

	drawboard(state);
	drawframe(the_turk);

	struct layout *layout = state->layout;

	for (i = 0; i < state->agents.count; i++) {
		agent *agent = state->agents.buffer + i;
		int x = agent->x + layout->shiftx, y = agent->y + layout->shifty;

		if (agent->hp > 0 && x >= 0 && y >= 0 && x < layout->width && y < layout->height) {
			int visible = Layer.get(fov, x - layout->shiftx, y - layout->shifty);
			if (visible) {
				Term.plot(x + layout->left, y + layout->top, agent->def->about.color, agent->def->about.glyph);
			}
		}
	}
	
	Term.refresh();
}

static int agent_turn(worldstate *state, agent *player, int choice) {
	int isvalid = 1, issuicide = 0;

	if (player->hp <= 0) return 0;
	
	if (IS_CHOICE_MOVEMENT(choice)) {
		int dx = dirs[choice].dx;
		int dy = dirs[choice].dy;
		int x = player->x + dx;
		int y = player->y + dy;

		mondef *cell = Board.get(state->board, x, y);
		if (cell == 0 || (cell->ascell.flags & CELL_STOP)) {
			return 0;
		} else {
			// check diagonals
			if (dx != 0 && dy != 0) {
				mondef *cell1 = Board.get(state->board, player->x, y);
				mondef *cell2 = Board.get(state->board, x, player->y);
				if ((cell1 == 0 || (cell1->ascell.flags & CELL_STOP)) && (cell2 == 0 || (cell2->ascell.flags & CELL_STOP))) {
					return 0;
				}
			}

			if ((cell->ascell.flags & CELL_KILL)) {
				issuicide = 1;
				player->hp = 0;
				if (player->flags & AGENT_PLAYER) {
					Chronicle.post(state->chron, "You burn.");
				} else {
					Builder.clear().write("The ").write(player->def->about.name).write(" burns.").publish(state->chron);
				}
			}

			// check whether it's attacking something -- awesomely bad
			int i;
			for (i = 0; i < state->agents.count; i++) {
				agent *other = &state->agents.buffer[i];
				if (other == player) continue;
				if (other->hp > 0 && other->x == x && other->y == y) {
					if ((!(player->def->asmob.mflags & MOB_ATTACKS_FRIENDS)) && FRIENDLY_MOBS(player, other)) {
						return 0;
					}

					if (other->flags & AGENT_ISITEM && player->flags & AGENT_PLAYER) {
						int slot;	
						for (slot = 0; slot < 5; slot++) {
							if (state->inventory[slot].def == NULL) {
								state->inventory[slot] = *other;
								other->hp = 0;
								Builder.clear().write("You get the ").write(other->def->about.name).write(".").publish(state->chron);
								player->x = x;
								player->y = y;
								return 1;
							}
						}
					}

					if ((other->flags & AGENT_VULNERABLE)) {
						other->hp = 0;
					} else {
						other->hp -= player->def->asmob.damage;
					}
					int kills = (other->hp <= 0);

					Builder.clear();
					if (player->flags & AGENT_PLAYER) {
						Builder.write("You");
						if (kills) {
							Builder.write(" kill ");
						} else {
							Builder.write(" hit ");
						}
					} else {
						Builder.write("The ").write(player->def->about.name);
						if (kills) {
							Builder.write(" kills ");
						} else {
							Builder.write(" hits ");
						}
					}

					if (other->flags & AGENT_PLAYER) {
						Builder.write("you.");
					} else {
						Builder.write("the ").write(other->def->about.name).write(".");
					}
					Builder.publish(state->chron);

					if (kills && other->def->asmob.mflags & MOB_ROYAL_CAPTURE) {
						state->victory = 1;
					}
					
					// should check whether I have "move on capture"
					if (!(player->def->asmob.mflags & MOB_MOVE_ON_CAPTURE)) {
						return isvalid;
					}
				}
			}

			player->x = x;
			player->y = y;
		}
	} else {
		if (choice >= APPLY) {
			// check whether the player has the item
			agent *item = &state->inventory[choice - APPLY];
			if (item->def != NULL) {
				int itemglyph = item->def->about.glyph;
				if (itemglyph == '*') { // worn
					int hacktype = item->def->asitem.hacktype;
			
					if (item->flags & AGENT_WORN) {
						Builder.clear().write("You remove the ").write(item->def->about.name).publish(state->chron);
						if (hacktype == 1) state->orb_active = 0;
						if (hacktype == 2) state->player->flags &= ~AGENT_VULNERABLE;
						if (hacktype == 3) state->player->flags &= ~AGENT_CAPTURES;
					} else {
						Builder.clear().write("You put on the ").write(item->def->about.name).publish(state->chron);
						if (hacktype == 1) state->orb_active = 1;
						if (hacktype == 2) state->player->flags |= AGENT_VULNERABLE;
						if (hacktype == 3) state->player->flags |= AGENT_CAPTURES;
					}

					item->flags ^= AGENT_WORN;
				} else if (itemglyph == '!') { // potion
					Builder.clear().write("You drink the ").write(item->def->about.name).publish(state->chron);
					player->hp = player->maxhp;
					item->def = NULL;

					return 1;
				} else if (itemglyph == '?') { // scroll
					Builder.clear().write("You read the ").write(item->def->about.name).publish(state->chron);

					int x = player->x + Rng.randint(&state->randomness, 13) - 6, y = player->y + Rng.randint(&state->randomness, 13) - 6;
					findsafety(state, &x, &y);
					player->x = x;
					player->y = y;
					item->def = NULL;

					return 1;
				} else {
					Builder.clear().write("You apply the ").write(item->def->about.name).publish(state->chron);
				}
			} else {
				return 0;
			}
		}
	}

	return isvalid;
}


static void playerturn(worldstate *state, int choice) {
	agent *player = state->player;
	
	state->ischecked = 0;
	state->isvalid = agent_turn(state, player, choice);
}

static void choose_outcome(outcomes *out) {
	int choice = -1;

	int i;
	int key = Term.get();

	for (i = 0; bindings[i].key != 0; i++) {
		if (key == bindings[i].key) {
			struct binding binding = bindings[i];

			if (binding.command == cmd_move) {
				// interpret movement based on context.
				int dir;
				// silly 
				for (dir = 0; dir < NDIRS; dir ++) {
					if (dirs[dir].dx == binding.dx && dirs[dir].dy == binding.dy) {
						choice = dir;
						break;
					}
				}
			} else if (binding.command == cmd_wait) {
				choice = NDIRS; // wait
			} else if (binding.command == cmd_apply) {
				int newkey = Term.get(), item = -1;
				if (newkey >= 'a' && newkey <= 'e') item = newkey - 'a';
				if (newkey >= '1' && newkey <= '5') item = newkey - '1';

				choice = APPLY + item;
			} else if (binding.command == cmd_drop) {
				int newkey = Term.get(), item = -1;
				if (newkey >= 'a' && newkey <= 'e') item = newkey - 'a';
				if (newkey >= '1' && newkey <= '5') item = newkey - '1';

				choice = NDIRS; // wait, after dropping the item -- see?
			} else if (binding.command == cmd_quit) {
				choice = -2;
				exit(1); // remove this evil
				return;
			}

			break;
		}
	}

	if (choice != -1) {
		if (out->movement[choice]->isvalid) {
			if (out->movement[choice]->ischecked) {
				Chronicle.post(out->prior->chron, "That would be certain death!");
			} else {
				freeworldstate(out->prior);
				out->prior = out->movement[choice];
				for (i = 0; i < NMOVES; i++) {
					if (i != choice) {
						freeworldstate(out->movement[i]);
					}
					out->movement[i] = NULL;
				}
			}
		}
	}
}

static void npcturn(worldstate *state) {
	int i;
	agentlist *agents = &state->agents;

	Board.dijkstra(state->board, state->player, state->player_dijkstra);

	for (i = 0; i < agents->count; i++) {
		agent *a = &agents->buffer[i];
		if (a->flags & AGENT_PLAYER) continue;

		agent *target = ai_find_enemy(state, a);
		if (target != NULL) {
			if (a->def->asmob.mflags & MOB_STUMBLE) {
				int stumble = Rng.randint(&state->randomness, NDIRS * 2);
				if (stumble < NDIRS && agent_turn(state, a, stumble)) {
					continue;
				}
			}
			if (a->def->asmob.mflags & MOB_DUMB) {
				agent_turn(state, a, ai_step_towards(state, a, target));
			} else if (a->def->asmob.mflags & MOB_PATHS) {
				agent_turn(state, a, ai_roll_down(state, a, state->player_dijkstra));
			}
		} else {
			agent_turn(state, a, NDIRS); // wait
		}
	}
}

static void compute_outcomes(outcomes *turk) {
	int i, checkmate = 1;
	int orb_active = turk->prior->orb_active;

	for (i = 0; i < NMOVES; i++) {
		// copy the prior worldstate
		turk->movement[i] = copyworldstate(turk->prior);

		playerturn(turk->movement[i], i);
		if (turk->movement[i]->isvalid) {
			npcturn(turk->movement[i]);

			if (orb_active && turk->movement[i]->player->hp <= 0) {
				turk->movement[i]->ischecked = 1;
			} else {
				checkmate = 0;
			}
		}
	}

	turk->checkmate = checkmate;
}

static void find_player(worldstate *state) {
	int i;
	agent *player = NULL;

	for (i = 0; i < state->agents.count; i++) {
		if (state->agents.buffer[i].flags & AGENT_PLAYER) {
			player = state->agents.buffer + i;
		}
	}

	state->player = player;
}

static void loop( ) {
	outcomes the_turk;

	the_turk.checkmate = 0;
	the_turk.prior = newworldstate();

	worldstate *initial = the_turk.prior;

	gameboard board = Board.new(160, 30);

	Board.generate(&board, &initial->randomness);

	layer *player_fov = Layer.new(35, 35);
	initial->player_fov = player_fov;

	struct layout layout = {
		1, 1, 23, 23, 0, 0
	};

	initial->layout = &layout;
	initial->board = &board;
	initial->player_dijkstra = Layer.new(35, 35);
	Chronicle.post(initial->chron, "Welcome to Rook.");
	// Chronicle.post(initial->chron, "You hold the Amulet of the Turk.  You will surely defeat the usurper-king.");

	spawn(initial, "rogue", 6, board.height / 2, AGENT_PLAYER | AGENT_VULNERABLE);

	int i;
	for (i=0; i<10; i++) {
		spawn(initial, "hound", -1, -1, 0);
		spawn(initial, "drunkard", -1, -1, 0);
		spawn(initial, "sot", -1, -1, 0);
	}

	for (i=0;i<5; i++) {
		spawn(initial, "potion of healing", -1, -1, 0);
		spawn(initial, "scroll of teleport", -1, -1, 0);
	}

	spawn(initial, "queen", board.width - 5, board.height / 2 - 2, 0);
	spawn(initial, "king", board.width - 5, board.height / 2, 0);

	find_player(initial);
	while (1) {
		compute_outcomes(&the_turk);
		if (the_turk.checkmate) {
			Chronicle.post(the_turk.prior->chron, "Checkmate!");
		}

		Board.fov(the_turk.prior->board, the_turk.prior->player, the_turk.prior->player_fov);
		draw(&the_turk);
		choose_outcome(&the_turk);

		if (the_turk.prior->player->hp <= 0) {
			Chronicle.post(the_turk.prior->chron, "Why did you take off the orb?");
			compute_outcomes(&the_turk);
			draw(&the_turk);
			Term.get();
			break;
		}
		if (the_turk.prior->victory) {
			Chronicle.post(the_turk.prior->chron, "You win!");
			compute_outcomes(&the_turk);
			draw(&the_turk);
			Term.get();
			break;
		}
	}

	freeworldstate(the_turk.prior);
	free(player_fov);
}

struct game_t Game = {
	loop
};

