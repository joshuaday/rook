#include "mods.h"
#include "game.h"
#include <stdlib.h>

static enum effects check_apply_effect(worldstate *state, agent *item, int itemnumber) {
	int magictype = item->def->asitem.magic[0];
	if (magictype == 'w') { // worn, wielded, quivered
		char hacktype = item->def->asitem.magic[1];
		if (hacktype == '-') return outcome_like_waiting;
		if (hacktype == 'q') return outcome_like_waiting;
		else return outcome_needs_forking;
	} else {
		return outcome_needs_forking;
	}
}

static enum effects do_apply_effect(worldstate *state, agent *item, int itemnumber) {
	agent *player = state->player;
	int magictype = item->def->asitem.magic[0];
	int hacktype = item->def->asitem.magic[1];
	if (magictype == 'w') { // worn or wielded
		if (item->flags & AGENT_WORN) {
			Builder.clear().write("You put away the ").name(item->def, NAME_ONE).write(".").publish(state->chron);
			if (hacktype == 't') state->turk_active = 0;
			if (hacktype == 'v') state->player->flags &= ~AGENT_VULNERABLE;
			if (hacktype == 'l') state->player->flags &= ~AGENT_CAPTURES;
		} else {
			Builder.clear().write("You take out the ").name(item->def, NAME_ONE).write(".").publish(state->chron);
			if (hacktype == 't') state->turk_active = 1;
			if (hacktype == 'v') state->player->flags |= AGENT_VULNERABLE;
			if (hacktype == 'l') state->player->flags |= AGENT_CAPTURES;

			if (hacktype == '-' || hacktype == 'q') {
				// unwield the previous weapon or staff
				int i;
				for (i = 0; i < NITEMS; i++) {
					agent *item = state->inventory + i;
					if (item->def != NULL && (item->flags & AGENT_WORN) && (item->def->asitem.magic[1] == hacktype)) {
						item->flags ^= AGENT_WORN;
					}
				}
			}
		}

		item->flags ^= AGENT_WORN;

		return outcome_needs_forking;
	} else if (magictype == '!') { // potion
		Builder.clear().write("You drink the ").name(item->def, NAME_ONE).publish(state->chron);
		if (hacktype == 'h') player->hp = player->maxhp;
		if (hacktype == 'p') player->time += 720;// Rng.roll(&state->randomness, "60 * 1d6");
			
		item->def = NULL;

		return outcome_needs_forking;
	} else if (magictype == '?') { // scroll
		Builder.clear().write("You read the ").name(item->def, NAME_ONE).publish(state->chron);

		int wasterng;
		for (wasterng = 0; wasterng < itemnumber; wasterng++) {
			// flip a coin n times, where the scroll is item n, so different scrolls can have different effects
			Rng.randint(&state->randomness, 2);
		}

		if (hacktype == 't') {
			int x = player->x + Rng.randint(&state->randomness, 17) - 8, y = player->y + Rng.randint(&state->randomness, 17) - 8;
			findsafety(state, &x, &y, 1);
			player->x = x;
			player->y = y;
		}

		item->def = NULL;

		return outcome_needs_forking;
	} else {
		Builder.clear().write("You apply the ").name(item->def, NAME_ONE).publish(state->chron);
		return outcome_needs_forking;
	}
}

#define ABS_SGN(x, sx) (x) *= ((sx) = (x) > 0 ? 1 : (x) < 0 ? -1 : 0);
static void trace_shot(worldstate *state, agent *a, int dx, int dy, int range, char want, int *out_x, int *out_y, agent **out_mob) {
	int i;
	for (i = 1; i <= range; i++) {
		mondef *cell = (Board.get(state->board, a->x + i * dx, a->y + i * dy));
		agent *mob = agent_at(state, a->x + i * dx, a->y + i * dy);
		if (mob != NULL) {
			if (want == '@') {
				*out_x = a->x + i * dx;
				*out_y = a->y + i * dy;
				*out_mob = mob;
			}
			return;
		}

		if (cell == NULL || cell->ascell.flags & CELL_BLOCK) {
			return;
		}

		if (want == ' ') {
			*out_x = a->x + i * dx;
			*out_y = a->y + i * dy;
		}
	}
}


static enum effects check_fire_effect(worldstate *state, agent *item, int dx, int dy) {
	agent *player = state->player;

	int stafftype = item->def->asitem.magic[2];
	if (stafftype == 'b') {
		int x = -1, y = -1;
		trace_shot(state, player, dx, dy, 8, ' ', &x, &y, NULL);
		if (x == -1) return outcome_invalid;
		else return outcome_needs_forking;
	}
	if (stafftype == 'd') {
		agent *mob = NULL;
		int x = -1, y = -1;
		trace_shot(state, player, dx, dy, 8, '@', &x, &y, &mob);
		if (x == -1) return outcome_invalid;
		else return outcome_needs_forking;
	}
	return outcome_like_waiting;
}

static enum effects do_fire_effect(worldstate *state, agent *item, int dx, int dy) {
	agent *player = state->player;

	int stafftype = item->def->asitem.magic[2];
	if (stafftype == 'b') {
		int x = -1, y = -1;
		trace_shot(state, player, dx, dy, 8, ' ', &x, &y, NULL);
		if (x == -1) return outcome_invalid;
		else {
			Chronicle.post(state->chron, "You blink!");
			player->x = x;
			player->y = y;
		}
	}
	if (stafftype == 'd') {
		agent *mob = NULL;
		int x = -1, y = -1;
		trace_shot(state, player, dx, dy, 8, '@', &x, &y, &mob);
		if (x == -1) return outcome_invalid;
		else {
			mob->hp = 0;
			Chronicle.post(state->chron, "Bam!  Dead.");
			return outcome_needs_forking;
		}
	}

	return outcome_like_waiting;
}


struct items_t Items = {
	check_apply_effect,
	do_apply_effect,
	check_fire_effect,
	do_fire_effect
};



