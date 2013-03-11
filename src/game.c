#include "mods.h"
#include <stdlib.h>
#include <stdio.h>

#define IS_CHOICE_MOVEMENT(c) ((c) >= 0 && (c) < 8)
#define FRIENDLY_MOBS(a, b) (((a)->flags & AGENT_PLAYER) == ((b)->flags & AGENT_PLAYER))

#define NDIRS (8)
#define APPLY (9)
#define NMOVES (15)
#define NITEMS (6)
const static struct direction {
	char *name;
	int key;
	int dx, dy;
} dirs[15] = {
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
	{"apply e", 'e', -2, 8},
	{"apply f", 'f', -2, 9},
};

int reverse_direction[9] = {0, 1, 2, 7, 8, 3, 6, 5, 4};
int *reverse_rows[3] = {&reverse_direction[1], &reverse_direction[4], &reverse_direction[7]};
int **dir_lookup = &reverse_rows[1];


enum commands {
	cmd_move,
	cmd_wait,
	cmd_apply,
	cmd_drop,

	cmd_quit,
	cmd_reset,
	cmd_hypothetical,
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
	{cmd_drop,  '-'},

	{cmd_apply, 'a'},
	{cmd_drop,  'd'},

	{cmd_quit, 'Q'},
	{cmd_reset, 'R'},
	{cmd_hypothetical, 'H'},
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
	int turk_active, turk_fake;

	rng randomness;
	agentlist agents;
	agent *player; // this is a pointer into the agentlist's buffer, or NULL if the player is dead
	struct layout *layout;
	gameboard *board;
	chronicle *chron;

	layer *player_fov, *player_dijkstra; // player_fov is managed externally, player_dijkstra internally
	agent inventory[NITEMS];
} worldstate;

typedef struct outcomes {
	int checkmate, request;

	worldstate *prior;
	worldstate *future[NMOVES];
} outcomes;

static void findsafety(worldstate *state, int *xbest, int *ybest, int kind);

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
	for (i = 0; i < NITEMS; i++) {
		state->inventory[i] = (agent){NULL};
	}
	state->inventory[0] = (agent){Catalog.lookup(Catalog.Item, "the Turk's turban"), .flags = AGENT_WORN | AGENT_ISITEM, .hp = 1};
	state->inventory[1] = (agent){Catalog.lookup(Catalog.Item, "ring of vulnerability"), .flags = AGENT_WORN | AGENT_ISITEM, .hp = 1};

	state->turk_active = 1;
	state->turk_fake = 0;

	return state;
}

static worldstate *worldstate_fork(worldstate *old) {
	worldstate *state = malloc(sizeof(worldstate));

	state->randomness = old->randomness;
	state->layout = old->layout;
	state->board = Board.fork(old->board);
	state->turk_active = old->turk_active;
	state->turk_fake = old->turk_fake;

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
	for (i = 0; i < NITEMS; i++) {
		state->inventory[i] = old->inventory[i];
	}
	return state;
}

static void worldstate_free(worldstate *old) {
	free(old->agents.buffer); // I don't trust this anymore.
	Chronicle.free(old->chron);
	Board.free(old->board);

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

static agent *item_at(worldstate *state, int x, int y) {
	int i;
	for (i = 0; i < state->agents.count; i++) {
		agent *a = state->agents.buffer + i;
		if (a->hp > 0 && (a->flags & AGENT_ISITEM) && (a->x == x && a->y == y)) {
			return a;
		}
	}
	return NULL;
}

static agent *agent_at(worldstate *state, int x, int y) {
	int i;
	for (i = 0; i < state->agents.count; i++) {
		agent *a = state->agents.buffer + i;
		if (a->hp > 0 && !(a->flags & AGENT_ISITEM) && (a->x == x && a->y == y)) {
			return a;
		}
	}
	return NULL;
}

static void findsafety(worldstate *state, int *xbest, int *ybest, int kind) {
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
					if ((kind == 1 || agent_at(state, x, y) == NULL) && (kind == 0 || item_at(state, x, y) == NULL)) {
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

static void drop(worldstate *state, int x, int y, agent *source) {
	findsafety(state, &x, &y, (source->flags & AGENT_ISITEM) ? 1 : 0);
	source->x = x;
	source->y = y;
	agentlist_insert(&state->agents, source);
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

	spawnee.flags = flags;

	spawnee.hp = spawnee.maxhp = spawnee.def->asmob.hp;

	if (isitem) {
		spawnee.hp = 1;
		spawnee.flags |= AGENT_ISITEM;
	}

	drop(state, x, y, &spawnee);
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
		int sx = dx < 0 ? -1 : dx > 0 ? 1 : 0;
		int sy = dy < 0 ? -1 : dy > 0 ? 1 : 0;
		int mx = sx * dx;
		int my = sy * dy;

		// let's do this in octants instead!

		if (mx >= my) {
			if (3 * my > mx) {
				dx = sx;
				dy = sy;
			} else {
				dx = sx;
				dy = 0;
			}
		} else {
			if (3 * mx > my) {
				dy = sy;
				dx = sx;
			} else {
				dy = sy;
				dx = 0;
			}
		}
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

static void write_chronicle(worldstate *state, chronicle *chron) {
	// write out our chronicle! (give the chronicle a window, like the game screen!)
	int bottom = state->layout->top + state->layout->height + 1;
	int i, x, y, history = bottom - (10 + NITEMS);
	int message_left = state->layout->left + state->layout->width + 5;

	Term.print(message_left - 2, bottom - history - 1, 12, "MESSAGES");
	message *msg = chron->last;
	int turn = chron->turn_number;

	i = 0;
	for (i = 0; i < history - chron->count; i++) {
		// this is a bad way of clearing the rest of the line, and is only here for item prompts
		int x1 = message_left;
		y = bottom - 1 - i;
		for (x = 0; x < Term.width - x1; x++) {
			Term.plot(x + x1, y, 0, ' ');
		}
	}
	for (; msg != NULL && i < history; i++) {
		int x1 = message_left;
		int color = turn == msg->turn_number ? 15 : 8;

		y = bottom - 1 - i;
		for (x = 0; x < msg->length; x++) {
			Term.plot(x + x1, y, color, msg->text[x]);
		}
		// this is a bad way of clearing the rest of the line, and is only here for item prompts
		for (; x < Term.width - x1; x++) {
			Term.plot(x + x1, y, color, ' ');
		}

		msg = msg->prev;
	}
}

static const char *badgetname(const mondef *def) {
	return def->forms.tag;
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

	if (state->layout->top > 0) {
		Term.print(state->layout->left + 1, state->layout->top - 1, color, "Rook");
	}

	char hpbuffer[50];
	sprintf(hpbuffer, "HP: %d/%d", state->player->hp, state->player->maxhp);
	// sprintf(hpbuffer, "HP: %d/%d", state->agents.count, state->agents.max);

	Term.print(state->layout->left + 1, state->layout->height + state->layout->top, color, hpbuffer);

	x = state->layout->left + state->layout->width + 7;
	y = 4;

	Term.print(x - 4, y - 2, 12, "VALID MOVES");

	for (i = 0; i < NMOVES; i++) {
		// illustrate the moves in the compass rose
		if (!the_turk->future[i]->isvalid) color = 0;
		else if (the_turk->future[i]->ischecked) color = 11 + 16 * 1;
		else color = 15;
		Term.plot(x + dirs[i].dx, y + dirs[i].dy, color, dirs[i].key);
	}

	Term.print(x - 4, y + 3, 12, "INVENTORY (a to apply)");
	// print out inventory
	for (i = 0; i < NITEMS; i++) { // can you tell that I'm running short on time?
		agent *item = &state->inventory[i];
		if (item->def != NULL) {
			Term.plot(x - 1, y + 4 + i, 15, ')');

			if ((item->flags & AGENT_WORN)) {
				sprintf(hpbuffer, "%s (worn)", badgetname(item->def));
			} else {
				sprintf(hpbuffer, "%s", badgetname(item->def));
			}
			Term.print(x + 2, y + 4 + i, 15, hpbuffer);
		}
	}

	write_chronicle(state, the_turk->prior->chron);
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
	int i, kind;

	Term.clear( );

	worldstate *state = the_turk->prior;
	layer *fov = state->player_fov;

	drawboard(state);
	drawframe(the_turk);

	struct layout *layout = state->layout;

	for (kind = 1; kind > -1; kind--) { // start with items
		for (i = 0; i < state->agents.count; i++) {
			agent *agent = state->agents.buffer + i;

			if (agent->hp <= 0) continue;
			if (kind == 0 && (agent->flags & AGENT_ISITEM)) continue;
			if (kind == 1 && !(agent->flags & AGENT_ISITEM)) continue;

			int x = agent->x + layout->shiftx, y = agent->y + layout->shifty;
			if (x >= 0 && y >= 0 && x < layout->width && y < layout->height) {
				int visible = Layer.get(fov, x - layout->shiftx, y - layout->shifty);
				if (visible) {
					Term.plot(x + layout->left, y + layout->top, agent->def->about.color, agent->def->about.glyph);
				}
			}
		}
	}
	
	Term.refresh();
}

static int agent_turn(worldstate *state, agent *player, int choice) {
	int hypothetical = state->turk_fake;
	int issuicide = 0;

	if (player->hp <= 0) return 0;
	
	if (IS_CHOICE_MOVEMENT(choice)) {
		int dx = dirs[choice].dx;
		int dy = dirs[choice].dy;
		int x = player->x + dx;
		int y = player->y + dy;

		mondef *cell = Board.get(state->board, x, y);
		if (cell == NULL || (cell->ascell.flags & CELL_STOP)) {
			if (player->flags & AGENT_PLAYER) {
				if (cell == NULL) {
					Builder.clear().write("You cannot step off the map.").publish(state->chron);
				} else {
					Builder.clear().write("You cannot walk through ").name(cell, NAME_PLURAL).write(".").publish(state->chron);
				}
			}
			return 0;
		} else {
			// check diagonals
			if (dx != 0 && dy != 0) {
				mondef *cell1 = Board.get(state->board, player->x, y);
				mondef *cell2 = Board.get(state->board, x, player->y);
				if ((cell1 == 0 || (cell1->ascell.flags & CELL_STOP)) && (cell2 == 0 || (cell2->ascell.flags & CELL_STOP))) {
					if (player->flags & AGENT_PLAYER) {
						Builder.clear().write("You cannot squeeze through that gap.").publish(state->chron);
					}
					return 0;
				}
			}

			if (hypothetical & player->flags & AGENT_PLAYER) {
				Builder.clear().write("If you move ").write(dirs[choice].name).write(" --").publish(state->chron);
			}

			if ((cell->ascell.flags & CELL_KILL)) {
				issuicide = 1;
				player->hp = 0;
				if (player->flags & AGENT_PLAYER) {
					Chronicle.post(state->chron, "You burn.");
				} else {
					Builder.clear().write("The ").name(player->def, NAME_ONE).write(" burns.").publish(state->chron);
				}
			}

			// check whether it's attacking something -- awesomely bad
			agent *other = agent_at(state, x, y);
			if (other != NULL && other != player) {
				if ((!(player->def->asmob.mflags & MOB_ATTACKS_FRIENDS)) && FRIENDLY_MOBS(player, other)) {
					return 0;
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
					if (hypothetical) {
						if (kills) {
							Builder.write(" would kill ");
						} else {
							Builder.write(" would hit ");
						}
					} else {
						if (kills) {
							Builder.write(" kill ");
						} else {
							Builder.write(" hit ");
						}
					}
				} else {
					Builder.write("The ").name(player->def, NAME_ONE);
					if (hypothetical) {
						if (kills) {
							Builder.write(" would kill ");
						} else {
							Builder.write(" would hit ");
						}
					} else {
						if (kills) {
							Builder.write(" kills ");
						} else {
							Builder.write(" hits ");
						}
					}
				}

				if (other->flags & AGENT_PLAYER) {
					Builder.write("you.");
				} else {
					Builder.write("the ").name(other->def, NAME_ONE).write(".");
				}
				Builder.publish(state->chron);

				if (kills && other->def->asmob.mflags & MOB_ROYAL_CAPTURE) {
					state->victory = 1;
				}
				
				// should check whether I have "move on capture"
				if (!(player->def->asmob.mflags & MOB_MOVE_ON_CAPTURE)) {
					return 1;
				}
			}
			
			if (player->flags & AGENT_PLAYER) {
				agent *item = item_at(state, x, y);
				if (item != NULL) {
					int slot;	
					for (slot = 0; slot < NITEMS; slot++) {
						if (state->inventory[slot].def == NULL) {
							state->inventory[slot] = *item;
							item->hp = 0;
							Builder.clear().write("You get the ").name(item->def, NAME_ONE).write(".").publish(state->chron);
							break;
						}
					}
					if (slot == NITEMS) {
						Builder.clear().write("You have no room for a ").name(item->def, NAME_ONE).write(".").publish(state->chron);
					}
				}
			}

			player->x = x;
			player->y = y;
		}
	} else {
		if (choice == NDIRS) {
			if (hypothetical & player->flags & AGENT_PLAYER) {
				Builder.clear().write("If you stand still --").publish(state->chron);
			}
		}
		if (choice >= APPLY) {
			// check whether the player has the item
			agent *item = &state->inventory[choice - APPLY];
			if (item->def != NULL) {
				int itemglyph = item->def->about.glyph;
				if (itemglyph == '*') { // worn
					int hacktype = item->def->asitem.hacktype;
			
					if (item->flags & AGENT_WORN) {
						Builder.clear().write("You remove the ").name(item->def, NAME_ONE).publish(state->chron);
						if (hacktype == 1) state->turk_active = 0;
						if (hacktype == 2) state->player->flags &= ~AGENT_VULNERABLE;
						if (hacktype == 3) state->player->flags &= ~AGENT_CAPTURES;
					} else {
						Builder.clear().write("You put on the ").name(item->def, NAME_ONE).publish(state->chron);
						if (hacktype == 1) state->turk_active = 1;
						if (hacktype == 2) state->player->flags |= AGENT_VULNERABLE;
						if (hacktype == 3) state->player->flags |= AGENT_CAPTURES;
					}

					item->flags ^= AGENT_WORN;
				} else if (itemglyph == '!') { // potion
					Builder.clear().write("You drink the ").name(item->def, NAME_ONE).publish(state->chron);
					player->hp = player->maxhp;
					item->def = NULL;

					return 1;
				} else if (itemglyph == '?') { // scroll
					Builder.clear().write("You read the ").name(item->def, NAME_ONE).publish(state->chron);

					int wasterng;
					for (wasterng = 0; wasterng < choice - APPLY; wasterng++) {
						// flip a coin n times, where the scroll is item n, so different scrolls can have different effects
						Rng.randint(&state->randomness, 2);
					}
					int x = player->x + Rng.randint(&state->randomness, 17) - 8, y = player->y + Rng.randint(&state->randomness, 17) - 8;
					findsafety(state, &x, &y, 1);
					player->x = x;
					player->y = y;
					item->def = NULL;

					return 1;
				} else {
					Builder.clear().write("You apply the ").name(item->def, NAME_ONE).publish(state->chron);
				}
			} else {
				return 0;
			}
		}
	}

	return 1;
}


static void playerturn(worldstate *state, int choice) {
	agent *player = state->player;
	
	state->ischecked = 0;
	state->isvalid = agent_turn(state, player, choice);
}

static void complain(worldstate *prior, const char *complaint) {
	if (complaint != NULL) {
		chronicle *temp = Chronicle.fork(prior->chron);
		Chronicle.post(temp, complaint);
		write_chronicle(prior, temp);
		Term.refresh();
		Chronicle.free(temp);
	} else {
		write_chronicle(prior, prior->chron);
		Term.refresh();
	}
}

static int pick_item(worldstate *prior, const char *prompt) {
	complain(prior, prompt);
	
	int newkey = Term.get(), item = -1;
	if (newkey >= 'a' && newkey < 'a' + NITEMS) item = newkey - 'a';
	if (newkey >= '1' && newkey < '1' + NITEMS) item = newkey - '1';

	if (item == -1) {
		write_chronicle(prior, prior->chron);
		Term.refresh();
	}

	return item;
}

static int choose_outcome(outcomes *out) {
	int choice = -1;

	while (choice == -1) {
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
					int item = pick_item(out->prior, "Apply what?");

					if (item != -1) {
						choice = APPLY + item;
					} else {
						choice = -1;
					}
				} else if (binding.command == cmd_drop) {
					// check that there's room to drop something
					

					// pick an item
					int itemnum = pick_item(out->prior, "Drop what?");
					choice = NDIRS; // we will wait after dropping the item
					
					// and we've got to spawn the item in the right spot
					// out->future[choice]
					if (itemnum >= 0) {
						agent *item = out->future[choice]->inventory + itemnum;
						if (item->def != NULL) {
							if (item->flags & AGENT_WORN) {
								complain(out->prior, "You can't drop that while you're wearing it.");
								choice = -1;
							} else {
								drop(out->future[choice], out->prior->player->x, out->prior->player->y, item);
								item->def = NULL;
							}
						} else {
							complain(out->prior, "You don't have that.");
							choice = -1;
						}
					} else {
						choice = -1;
					}
				} else if (binding.command == cmd_quit) {
					choice = -2;
					out->request = cmd_quit;
					return 1;
				} else if (binding.command == cmd_reset) {
					choice = -2;
					out->request = cmd_reset;
					return 1;
				}

				break;
			}
		}

		if (choice != -1) {
			if (out->future[choice]->isvalid) {
				if (out->future[choice]->ischecked) {
					if (out->checkmate) {
						worldstate *prior = out->prior;
						out->prior = out->future[choice];
						draw(out);
						out->prior = prior;
					} else {
						complain(out->prior, "That would be certain death!");
						choice = -1;
					}
				} else {
					worldstate_free(out->prior);
					out->prior = out->future[choice];
					for (i = 0; i < NMOVES; i++) {
						if (i != choice) {
							worldstate_free(out->future[i]);
						}
						out->future[i] = NULL;
					}
				}
			} else {
				complain(out->future[choice], NULL);
				choice = -1;
			}
		} else {
			if (out->checkmate) {
				draw(out);
			}
		}
	}

	return 0;
}

static int choose_noncommand(outcomes *out) {
	while (1) {
		int i;
		int key = Term.get();
		for (i = 0; bindings[i].key != 0; i++) {
			if (key == bindings[i].key) {
				struct binding binding = bindings[i];
				
				if (binding.command == cmd_quit) {
					out->request = cmd_quit;
					return 1;
				} else if (binding.command == cmd_reset) {
					out->request = cmd_reset;
					return 1;
				} else if (binding.command == cmd_hypothetical) {
					out->request = cmd_hypothetical;
					return 1;
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
	int turk_active = turk->prior->turk_active;

	for (i = 0; i < NMOVES; i++) {
		// copy the prior worldstate
		turk->future[i] = worldstate_fork(turk->prior);
		turk->future[i]->turk_fake = turk->checkmate;

		playerturn(turk->future[i], i);
		if (turk->future[i]->isvalid) {
			npcturn(turk->future[i]);

			if (turk_active && turk->future[i]->player->hp <= 0) {
				turk->future[i]->ischecked = 1;
			} else {
				checkmate = 0;
			}
		}

		if (turk->future[i]->turk_fake) {
			Chronicle.post(turk->future[i]->chron, "(press space)");
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
	while (1) {
		outcomes the_turk;

		the_turk.checkmate = 0;
		the_turk.request = 0;

		worldstate *initial = newworldstate();
		the_turk.prior = initial;


		gameboard *board = Board.new(160, 30);

		Board.generate(board, &initial->randomness);

		layer *player_fov = Layer.new(35, 35);
		initial->player_fov = player_fov;

		struct layout layout = {
			1, 1, 23, 23, 0, 0
		};

		initial->layout = &layout;
		initial->board = board;
		initial->player_dijkstra = Layer.new(35, 35);
		Chronicle.post(initial->chron, "Welcome to Rook.");
		// Chronicle.post(initial->chron, "You hold the Amulet of the Turk.  You will surely defeat the usurper-king.");

		spawn(initial, "rogue", 6, board->height / 2, AGENT_PLAYER | AGENT_VULNERABLE);

		int i;
		for (i=0; i<14; i++) {
			spawn(initial, "hound", -1, -1, 0);
		}

		for (i=0; i<8; i++) {
			spawn(initial, "drunkard", -1, -1, 0);
			spawn(initial, "sot", -1, -1, 0);
		}

		for (i=0;i<4; i++) {
			spawn(initial, "potion of healing", -1, -1, 0);
			spawn(initial, "scroll of teleport", -1, -1, 0);
		}

		spawn(initial, "queen", board->width - 5, board->height / 2 - 2, 0);
		spawn(initial, "king", board->width - 5, board->height / 2, 0);

		find_player(initial);
		while (1) {
			compute_outcomes(&the_turk);
			if (the_turk.checkmate) {
				Chronicle.post(the_turk.prior->chron, "Checkmate!");
				Board.fov(the_turk.prior->board, the_turk.prior->player, the_turk.prior->player_fov);
				draw(&the_turk);

				choose_noncommand(&the_turk);
				if (the_turk.request == cmd_hypothetical) {
					Chronicle.post(the_turk.prior->chron, "");
					Chronicle.post(the_turk.prior->chron, "What would you like to have done?");
					compute_outcomes(&the_turk);
					draw(&the_turk);

					while (1) {
						choose_outcome(&the_turk);
						if (the_turk.request == cmd_quit) break;
						if (the_turk.request == cmd_reset) break;
					}
				}
				break;
			}

			// this is mangled -- fix it up, but how?
			if (Term.height < 25) {
				layout.top = 0;
				layout.height = Term.height - 1;
			} else {
				layout.top = 1;
				layout.height = 23;
			}

			Board.fov(the_turk.prior->board, the_turk.prior->player, the_turk.prior->player_fov);
			draw(&the_turk);
			if (choose_outcome(&the_turk)) {
				if (the_turk.request == cmd_quit) break;
				if (the_turk.request == cmd_reset) break;
			}

			if (the_turk.prior->player->hp <= 0) {
				Chronicle.post(the_turk.prior->chron, "Why did you relinquish the turban?");
				compute_outcomes(&the_turk);
				draw(&the_turk);
				choose_noncommand(&the_turk);
				break;
			}
			if (the_turk.prior->victory) {
				Chronicle.post(the_turk.prior->chron, "You win!");
				compute_outcomes(&the_turk);
				draw(&the_turk);
				choose_noncommand(&the_turk);
				break;
			}
		}

		worldstate_free(the_turk.prior);
		for (i = 0; i < NMOVES; i++) {
			worldstate_free(the_turk.future[i]);
		}
		free(player_fov);
		
		if (the_turk.request != cmd_reset) {
			break;
		}
	}
}

struct game_t Game = {
	loop
};

