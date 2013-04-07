#include "mods.h"
#include "game.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define IS_CHOICE_MOVEMENT(c) ((c) >= 0 && (c) < 8)
#define FRIENDLY_MOBS(a, b) (((a)->flags & AGENT_PLAYER) == ((b)->flags & AGENT_PLAYER))

// get application out of this -- it's silly
const struct direction dirs[9] = {
	{"northwest", 'y', -1, -1},
	{"north", 'k', 0, -1},
	{"northeast", 'u', 1, -1},
	{"east", 'l', 1, 0},
	{"southeast", 'n', 1, 1},	
	{"south", 'j', 0, 1},
	{"southwest", 'b', -1, 1},
	{"west", 'h', -1, 0},

	{"wait", '.', 0, 0}
};

int reverse_direction[9] = {0, 1, 2, 7, 8, 3, 6, 5, 4};
int *reverse_rows[3] = {&reverse_direction[1], &reverse_direction[4], &reverse_direction[7]};
int **dir_lookup = &reverse_rows[1];

worldstate *invalid_future; // used to indicate that a future hasn't actually been forked

static agentlist newagentlist(int max) {
	agentlist dest;
	dest.max = max; //4 + src->count + src->count / 2;
	dest.count = 0;
	dest.buffer = malloc(sizeof(agent) * dest.max);
	return dest;
}

static agentlist forkagentlist(agentlist *src) {
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

	state->locked = 0;
	state->donotfree = 0;
	state->victory = 0;
	state->ischecked = 0;
	state->iswait = 0;
	state->isvalid = 1;
	state->totaltime = 0;

	state->player = NULL;
	state->player_fov = NULL;
	state->player_dijkstra = NULL;
	state->player_dijkstra_range = NULL;
	state->layout = NULL;
	state->board = NULL;
	state->chron = Chronicle.fork(NULL);

	int i;
	for (i = 0; i < NITEMS; i++) {
		state->inventory[i] = (agent){NULL};
	}
	state->inventory[0] = (agent){Catalog.lookup(Catalog.Item, "* turk"), .flags = AGENT_WORN | AGENT_ISITEM, .hp = 1};
	state->inventory[1] = (agent){Catalog.lookup(Catalog.Item, "* vuln"), .flags = AGENT_WORN | AGENT_ISITEM, .hp = 1};
	state->inventory[2] = (agent){Catalog.lookup(Catalog.Item, "- knife"), .flags = AGENT_WORN | AGENT_ISITEM, .hp = 1};
	state->inventory[3] = (agent){Catalog.lookup(Catalog.Item, "/ blink"), .flags = AGENT_ISITEM, .hp = 1};
	state->inventory[4] = (agent){Catalog.lookup(Catalog.Item, "/ death"), .flags = AGENT_ISITEM, .hp = 1};

	state->turk_active = 1;
	state->turk_fake = 0;

	return state;
}

static worldstate *worldstate_fork(worldstate *old) {
	worldstate *state = malloc(sizeof(worldstate));

	state->randomness = old->randomness;
	state->layout = old->layout;
	state->board = Board.fork(old->board);
	state->iswait = old->iswait;
	state->turk_active = old->turk_active;
	state->turk_fake = old->turk_fake;

	state->victory = old->victory;
	state->ischecked = old->ischecked;
	state->locked = old->locked;
	state->donotfree = old->donotfree;
	state->isvalid = old->isvalid;
	state->totaltime = old->totaltime;
	if (old->player_dijkstra != NULL) {
		state->player_dijkstra = Layer.new(old->player_dijkstra->width, old->player_dijkstra->height);
	} else {
		state->player_dijkstra = NULL;
	}

	if (old->player_dijkstra_range != NULL) {
		state->player_dijkstra_range = Layer.new(old->player_dijkstra_range->width, old->player_dijkstra_range->height);
	} else {
		state->player_dijkstra_range = NULL;
	}

	state->agents = forkagentlist(&old->agents);
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
	if (old == NULL || old->donotfree) return;
	free(old->agents.buffer); // I don't trust this anymore.
	Chronicle.free(old->chron);

	if (old->board != NULL) {
		Board.free(old->board);
	}
	if (old->player_dijkstra != NULL) {
		free(old->player_dijkstra);
	}
	if (old->player_dijkstra_range != NULL) {
		free(old->player_dijkstra_range);
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

agent *agent_at(worldstate *state, int x, int y) {
	int i;
	for (i = 0; i < state->agents.count; i++) {
		agent *a = state->agents.buffer + i;
		if (a->hp > 0 && !(a->flags & AGENT_ISITEM) && (a->x == x && a->y == y)) {
			return a;
		}
	}
	return NULL;
}

void findsafety(worldstate *state, int *xbest, int *ybest, int kind) {
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
	spawnee.time = 30;

	if (spawnee.def == NULL) {
		spawnee.def = Catalog.lookup(Catalog.Item, tag);
		isitem = 1;

		if (spawnee.def == NULL) {
			fprintf(stderr, "Could not spawn: %s\n", tag);
		}
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

static int agent_may_move(worldstate *state, agent *player, int dx, int dy) {
	int x = player->x + dx;
	int y = player->y + dy;
	mondef *cell = Board.get(state->board, x, y);
	if (cell == NULL || (cell->ascell.flags & CELL_STOP)) {
		if (player->flags & AGENT_PLAYER) {
			if (!state->locked) {
				if (cell == NULL) {
					Builder.clear().write("You cannot step off the map.").publish(state->chron);
				} else {
					Builder.clear().write("You cannot walk through ").name(cell, NAME_PLURAL).write(".").publish(state->chron);
				}
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
					if (!state->locked) {
						Builder.clear().write("You cannot squeeze through that gap.").publish(state->chron);
					}
				}
				return 0;
			}
		}
	}
	return 1;
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
		int rx = (state->player_dijkstra->width - 1) / 2, ry = (state->player_dijkstra->height - 1) / 2;
		if (dx >= -rx && dx <= rx && dy >= -ry && dy <= ry) {
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
				if (agent_may_move(state, self, dirs[dir].dx, dirs[dir].dy)) {
					bestscore = thisscore;
					bestdir = dir;
				}
			}
		}
		return bestdir;
	}

	return NDIRS; // wait
}

static void write_chronicle(worldstate *state, chronicle *chron) {
	// write out our chronicle! (give the chronicle a window, like the game screen!)
	int bottom = state->layout->top + state->layout->height + 1;
	int i, x, y, history = bottom - state->layout->lastItem;
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
	if (def->forms.name != NULL) return def->forms.name;
	else return def->forms.tag;
}

static struct worldstate *getFuture(outcomes *the_turk, int cmd, int dir) {
	worldstate *future = NULL;

	switch (cmd) {
	case cmd_move: future = the_turk->future[FUTURE_MOVE + dir]; break;
	case cmd_drop:
	case cmd_wait: future = the_turk->future[FUTURE_WAIT]; break;
	case cmd_fire: future = the_turk->future[FUTURE_FIRE + dir]; break;
	case cmd_apply: future = the_turk->future[FUTURE_APPLY + dir]; break; // dir is an item here instead
	default: return NULL;
	}

	if (future->iswait) future = the_turk->future[FUTURE_WAIT];
	return future;
}

static void drawframe(outcomes *the_turk) {
	worldstate *state = the_turk->prior;
	worldstate *future;

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
	sprintf(hpbuffer, "%d", (state->totaltime / 60));
	Term.print(state->layout->left + state->layout->width - strlen(hpbuffer), state->layout->top - 1, color, hpbuffer);

	sprintf(hpbuffer, "HP: %d/%d", state->player->hp, state->player->maxhp);
	Term.print(state->layout->left + 1, state->layout->height + state->layout->top, color, hpbuffer);

	x = state->layout->left + state->layout->width + 7;
	y = 3;

	Term.print(x - 4, y - 2, 12, "VALID MOVES");

	// walking
	for (i = 0; i < NMOVES; i++) {
		// illustrate the moves in the compass rose
		if (dirs[i].key != '\0') {
			future = getFuture(the_turk, cmd_move, i);

			if (!future->isvalid) color = 0;
			else if (future->ischecked) color = 11 + 16 * 1;
			else color = 15;
			Term.plot(x + dirs[i].dx, y + dirs[i].dy, color, dirs[i].key);
		}
	}

	// shooting
	for (i = 0; i < NDIRS; i++) {
		// illustrate the moves in the compass rose
		if (dirs[i].key != '\0') {
			future = getFuture(the_turk, cmd_fire, i);

			if (!future->isvalid) color = 0;
			else if (future->ischecked) color = 11 + 16 * 1;
			else color = 15;
			Term.plot(10 + x + dirs[i].dx, y + dirs[i].dy, color, dirs[i].key);
		}
	}

	int yline = y + 3, cat, catwritten;
	const char *category_messages[] = {"EQUIPMENT", "BELT"};

	for (cat = 0; cat < 2; cat++) {
		catwritten = 0;
		for (i = 0; i < NITEMS; i++) {
			agent *item = &state->inventory[i];
			if (item->def != NULL) {
				if (cat == 0 && !(item->flags & AGENT_WORN)) continue;
				if (cat == 1 && (item->flags & AGENT_WORN)) continue;

				if (!catwritten) {
					Term.print(x - 4, yline++, 12, category_messages[cat]);
					catwritten = 1;
				}

				future = getFuture(the_turk, cmd_apply, i);
				if (!future->isvalid) color = 0;
				else if (future->ischecked) color = 11 + 16 * 1;
				else color = 15;

				Term.plot(x - 2, yline, color, i + 'a');
				Term.plot(x - 1, yline, color, ')');

				if ((item->flags & AGENT_WORN)) {
					int magictype = item->def->asitem.magic[1];
					if (magictype == '-') {
						sprintf(hpbuffer, "%s (wielded)", badgetname(item->def));
					} else if (magictype =='q') {
						sprintf(hpbuffer, "%s (quivered)", badgetname(item->def));
					} else {
						sprintf(hpbuffer, "%s (worn)", badgetname(item->def));
					}
				} else {
					sprintf(hpbuffer, "%s", badgetname(item->def));
				}
				Term.print(x + 2, yline, color, hpbuffer);
				yline ++;
			}
		}
		if (catwritten) yline++;
	}

	state->layout->lastItem = yline + 1;
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


static agent *get_player_weapon(worldstate *state, int itemType) {
	// itemType == 0 for melee, 1 for ranged
	// find a wielded weapon
	//if (itemType == 1) return NULL;

	int i;
	agent *mob = state->player;
	for (i = 0; i < NITEMS; i++) {
		agent *item = state->inventory + i;
		if (item->def != NULL && (item->flags & AGENT_WORN) && (item->def->asitem.magic[0] == 'w')) {
			if (item->def->asitem.magic[1] == itemType) return item;
		}
	}

	return NULL;
}

static const char *get_current_dice(worldstate *state, agent *mob) {
	if ((mob->flags & AGENT_PLAYER)) {
		agent *weapon = get_player_weapon(state, '-');
		if (weapon != NULL) return weapon->def->asitem.stab;
	}

	if (mob->def->asmob.range != NULL) return mob->def->asmob.range;

	return mob->def->asmob.stab;
}

static int agent_attack(worldstate *state, agent *player, agent *other) {
	if ((!(player->def->asmob.mflags & MOB_ATTACKS_FRIENDS)) && FRIENDLY_MOBS(player, other)) {
		return outcome_invalid;
	}

	// override the time specified elsewhere, and be aware that archers don't take turns
	// (except through this) when firing!
	player->time = Rng.roll(&state->randomness, player->def->asmob.speed);
	if (player->time == 0) player->time = 60;

	int damage = Rng.roll(&state->randomness, get_current_dice(state, player));
	if (damage > 0 && (other->flags & AGENT_VULNERABLE)) {
		damage = other->hp;
	}
	if (damage > other->hp) {
		damage = other->hp;
	}
	other->hp -= damage;
	int stung = damage ? Rng.roll(&state->randomness, player->def->asmob.sting) : 0;

	if (stung) { // extend get_current_dice for this
		other->time += stung;
	}

	int verb = 0;
	if (stung) verb = 1;
	if (other->hp <= 0) verb = 2;
	if (damage == 0) verb = 3;
	const char **verbs;

	Builder.clear();
	if (player->flags & AGENT_PLAYER) {
		Builder.write("You");
		verbs = (const char*[4]) {"hit", "sting", "kill", "miss"};
	} else {
		Builder.write("The ").name(player->def, NAME_ONE);
		verbs = (const char*[4]) {"hits", "stings", "kills", "misses"};
	}

	Builder.write(" ").write(verbs[verb]).write(" ");

	if (other->flags & AGENT_PLAYER) {
		Builder.write("you");
	} else {
		Builder.write("the ").name(other->def, NAME_ONE);
	}
	// Builder.write(" (").write(player->def->asmob.damage).write(")");
	if (damage > 0 && other->hp > 0) {
		char hpbuffer[10];
		sprintf(hpbuffer, "%d", damage);
		Builder.write(" for ").write(hpbuffer).write(" hp");
	}

	Builder.write(".");

	Builder.publish(state->chron);

	if ((other->hp <= 0) && other->def->asmob.mflags & MOB_ROYAL_CAPTURE) {
		state->victory = 1;
	}
	
	// should check whether I have "move on capture"
	if (!(player->def->asmob.mflags & MOB_MOVE_ON_CAPTURE)) {
		return outcome_needs_forking;
	}
	return outcome_needs_forking;
}

static enum effects agent_turn(worldstate *state, agent *player, int choice) {
	int locked = state->locked; // when it's locked, we're only looking at the move's validity

	int hypothetical = state->turk_fake;

	if (player->hp <= 0) return outcome_invalid;

	if (!locked) {
		player->time = Rng.roll(&state->randomness, player->def->asmob.speed);
		if (player->time == 0) player->time = 60;
	}
	
	if (IS_CHOICE_MOVEMENT(choice)) {
		int dx = dirs[choice].dx;
		int dy = dirs[choice].dy;
		int x = player->x + dx;
		int y = player->y + dy;
		
		if (!agent_may_move(state, player, dx, dy)) {
			return outcome_invalid;
		} else if (locked) {
			// it's possible that this state is still invalid, but this is enough for most cases
			return outcome_needs_forking;
		}

		mondef *cell = Board.get(state->board, x, y);
		if (hypothetical & player->flags & AGENT_PLAYER) {
			Builder.clear().write("If you move ").write(dirs[choice].name).write(", events proceed as follows.").publish(state->chron);
		}

		if ((cell->ascell.flags & CELL_KILL)) {
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
			return agent_attack(state, player, other);
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

		// check whether the cell has a promotion:
		if ((cell->ascell.flags & CELL_PROMOTE_ON_STEP)) {
			Board.set(state->board, x, y, Catalog.lookup(Catalog.Cell, cell->ascell.promotion));
		}

		player->x = x;
		player->y = y;
	} else {
		if (choice == NDIRS) {
			if (locked) {
				return outcome_needs_forking;
			}
			if (hypothetical & player->flags & AGENT_PLAYER) {
				Builder.clear().write("If you stand still --").publish(state->chron);
			}
		}
		if (choice >= FUTURE_FIRE && choice < FUTURE_APPLY) {
			agent *item = get_player_weapon(state, 'q');
			if (item == NULL) {
				return outcome_invalid;
			}

			int dx = dirs[choice - FUTURE_FIRE].dx;
			int dy = dirs[choice - FUTURE_FIRE].dy;
			
			if (locked) {
				return Items.check_fire_effect(state, item, dx, dy);
			}

			// actually shoot now!
			return Items.do_fire_effect(state, item, dx, dy);
		}
		if (choice >= FUTURE_APPLY) {
			// check whether the player has the item
			agent *item = &state->inventory[choice - FUTURE_APPLY];
			if (item->def != NULL) {
				if (locked) {
					return Items.check_apply_effect(state, item, choice - FUTURE_APPLY);
				}

				return Items.do_apply_effect(state, item, choice - FUTURE_APPLY);
			} else {
				return 0;
			}
		}
	}

	return 1;
}


static void playerturn(outcomes *turk, int choice) {
	agent *player = turk->prior->player;
	enum effects outcome = (turk->prior->locked) ? (outcome_needs_forking) : agent_turn(turk->prior, player, choice);

	if (outcome == outcome_like_waiting) outcome = outcome_needs_forking;

	if (outcome == outcome_needs_forking) {
		turk->future[choice] = worldstate_fork(turk->prior);
		turk->future[choice]->turk_fake = turk->checkmate;

		worldstate *state = turk->future[choice];
		player = state->player;

		state->ischecked = 0;
		state->locked = 0;

		state->isvalid = agent_turn(state, player, choice); // strictly this should be 1, but if not it's robust enough to handle it
	} else if (outcome == outcome_like_waiting) {
		// something something something
	} else {
		turk->future[choice] = invalid_future;
	}
}

static void complain(worldstate *prior, const char *complaint) {
	if (prior -> donotfree) return;
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

static int pick_direction(worldstate *prior, const char *prompt) {
	complain(prior, prompt);
	
	int newkey = Term.get(), choice = -1, i;

	for (i = 0; bindings[i].key != 0; i++) {
		if (newkey == bindings[i].key) {
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
			}
		}
	}

	if (choice == -1) {
		write_chronicle(prior, prior->chron);
		Term.refresh();
	}

	return choice;
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
				} else if (binding.command == cmd_fire) {
					int dir = pick_direction(out->prior, "Zap where?");
					if (dir >= FUTURE_MOVE && dir < FUTURE_WAIT) {
						choice = FUTURE_FIRE + dir;
					} else {
						write_chronicle(out->prior, out->prior->chron);
						Term.refresh();
						choice = -1;
					}
				} else if (binding.command == cmd_remove) {
					int item = pick_item(out->prior, "Remove what?");

					if (item != -1) {
						choice = FUTURE_APPLY + item;
					} else {
						choice = -1;
					}
				} else if (binding.command == cmd_equip) {
					int item = pick_item(out->prior, "Equip what?");

					if (item != -1) {
						choice = FUTURE_APPLY + item;
					} else {
						choice = -1;
					}
				} else if (binding.command == cmd_apply) {
					int item = pick_item(out->prior, "Apply what?");

					if (item != -1) {
						choice = FUTURE_APPLY + item;
					} else {
						choice = -1;
					}
				} else if (binding.command == cmd_drop) {
					// pick an item
					int itemnum = pick_item(out->prior, "Drop what?");
					choice = NDIRS; // we will wait after dropping the item
					
					// and we've got to spawn the item in the right spot
					// out->future[choice]
					if (itemnum >= 0) {
						agent *item = out->future[choice]->inventory + itemnum;
						if (item->def != NULL) {
							if (item->flags & AGENT_WORN) {
								choice = FUTURE_APPLY + itemnum;
								item = out->future[choice]->inventory + itemnum;
								// complain(out->prior, "You can't drop that while you're using it.");
								// choice = -1;
							}

							drop(out->future[choice], out->prior->player->x, out->prior->player->y, item);
							item->def = NULL;
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
					for (i = 0; i < NCHOICES; i++) {
						if (i != choice) {
							worldstate_free(out->future[i]);
						}
						out->future[i] = NULL;
					}
					for (i = 0; i < 12; i++) {
						Rng.randint(&out->prior->randomness, 6); // roll a few dice to waste some randomness
					}
				}
			} else {
				// let's turn this choice into an actual (incomplete) move to complain about it
				playerturn(out, choice);
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

#define ABS_SGN(x, sx) (x) *= ((sx) = (x) > 0 ? 1 : (x) < 0 ? -1 : 0);
static int has_shot(worldstate *state, agent *a, agent *target) {
	int dx, dy, sx, sy;
	dx = target->x - a->x;
	dy = target->y - a->y;
	ABS_SGN(dx, sx);
	ABS_SGN(dy, sy);

	if ((dx == dy) || (dx > 0 && dy == 0) || (dy > 0 && dx == 0)) {
		int i = dx > dy ? dx : dy;
		if (i < 2 || i > 7) {
			return 0;
		}
		for ( ; i > 0; i--) {
			if ((Board.get(state->board, a->x + i * sx, a->y + i * sy)->ascell.flags & CELL_BLOCK)) {
				return 0;
			}
		}
		return 1;
	}

	return 0;
}

static void npcturn(worldstate *state) {
	int i;
	agentlist *agents = &state->agents;

	Board.dijkstra(state->board, state->player, 0, state->player_dijkstra);
	Board.dijkstra(state->board, state->player, 1, state->player_dijkstra_range);

	while (state->player != NULL && state->player->hp > 0) {
		int leasttime = 60 * 60; // sixty turns is forever
		for (i = 0; i < agents->count; i++) {
			agent *a = &agents->buffer[i];
			if (a->def == NULL || a->hp < 1) continue;
			if (a->time < leasttime) leasttime = a->time;
		}

		state->totaltime += leasttime;

		for (i = 0; i < agents->count; i++) {
			agent *a = &agents->buffer[i];
			if (a->def == NULL || a->hp < 1) continue;
			a->time -= leasttime;
		}
		if (state->player->time <= 0) return;

		for (i = 0; i < agents->count; i++) {
			agent *a = &agents->buffer[i];
			if (a->def == NULL || a->hp < 1) continue;

			// it's silly, having the ai right here -- but it's fine
			if (a->time <= 0) {
				agent *target = ai_find_enemy(state, a);
				if (target != NULL) {
					if (a->def->asmob.stumble != NULL) {
						if (Rng.roll(&state->randomness, a->def->asmob.stumble)) {
							int stumble = Rng.randint(&state->randomness, NDIRS);
							if (agent_turn(state, a, stumble)) {
								continue;
							}
						}
					}
					if (a->def->asmob.range != NULL && has_shot(state, a, target)) {
						agent_attack(state, a, target);
						continue;
					}
					if (a->def->asmob.mflags & MOB_DUMB) {
						agent_turn(state, a, ai_step_towards(state, a, target));
					} else if (a->def->asmob.mflags & MOB_PATHS) {
						if (a->def->asmob.range != NULL) {
							agent_turn(state, a, ai_roll_down(state, a, state->player_dijkstra_range));
						} else {
							if (!agent_turn(state, a, ai_roll_down(state, a, state->player_dijkstra))) {
								// can't step where we want to; ai_roll_down should sort all eight options, but right now, let's try to stumble
								int stumble = Rng.randint(&state->randomness, NDIRS);
								agent_turn(state, a, stumble);
							}
						}
					}
				} else {
					agent_turn(state, a, NDIRS); // wait
				}

				if (a->time <= 0) {
					// this is just error catching logic, in case the actor failed to make a valid move
					a->time = 60;
				}
			}
		}
	}
}

static void compute_outcomes(outcomes *turk) {
	int i, checkmate = 1;
	int turk_active = turk->prior->turk_active;

	turk->prior->locked = 1;

	for (i = 0; i < NCHOICES; i++) {
		playerturn(turk, i);
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

	turk->prior->locked = 0;
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
	invalid_future = newworldstate();
	invalid_future->isvalid = 0;
	invalid_future->donotfree = 1;
	
	while (1) {
		outcomes the_turk;

		the_turk.checkmate = 0;
		the_turk.request = 0;

		worldstate *initial = newworldstate();
		the_turk.prior = initial;


		gameboard *board = Board.new(160, 30);

		Board.generate(board, &initial->randomness);

		layer *player_fov = Layer.new(31, 31);
		initial->player_fov = player_fov;

		struct layout layout = {
			1, 1, 23, 23, 0, 0
		};

		initial->layout = &layout;
		initial->board = board;

		// strategic layers are used by the ai -- we can add more to describe,
		// for instance, the defensive coverage of each cell
		initial->player_dijkstra = Layer.new(33, 33);
		initial->player_dijkstra_range = Layer.new(33, 33);
		Chronicle.post(initial->chron, "Welcome to Rook.");

		spawn(initial, "rogue", 6, board->height / 2, AGENT_PLAYER | AGENT_VULNERABLE);

		int i;
		for (i=0; i<4; i++) {
			spawn(initial, "ox", -1, -1, 0);
			spawn(initial, "hound", -1, -1, 0);
			spawn(initial, "horse", -1, -1, 0);
			spawn(initial, "wasp", -1, -1, 0);
			spawn(initial, "spider", -1, -1, 0);
		}

		for (i=0; i<4; i++) {
			spawn(initial, "drunkard", -1, -1, 0);
			spawn(initial, "sot", -1, -1, 0);
			spawn(initial, "archer", -1, -1, 0);
		}

		for (i=0; i<3; i++) {
			spawn(initial, "! heal", -1, -1, 0);
			spawn(initial, "! para", -1, -1, 0);
			spawn(initial, "? tele", -1, -1, 0);
		}

		spawn(initial, "queen", board->width - 5, board->height / 2 - 2, 0);
		spawn(initial, "king", board->width - 5, board->height / 2, 0);

		find_player(initial);
		initial->player->time = 0;
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
				Chronicle.post(the_turk.prior->chron, "Why did you relinquish the orb?");
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
		for (i = 0; i < NCHOICES; i++) {
			worldstate_free(the_turk.future[i]);
		}
		free(player_fov);
		
		if (the_turk.request != cmd_reset) {
			break;
		}
	}

	invalid_future->donotfree = 0;
	worldstate_free(invalid_future);
}

struct game_t Game = {
	loop
};

