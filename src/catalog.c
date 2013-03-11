#include "mods.h"
#include <string.h>

const static struct colorflavor {
	const char *Term, *gem, *metal, *horse;
} colorflavors[16] = {
	{"black", "", "", ""},
	{"red", "ruby", "", "blood-red"},
	{"green", "chryosite", "", "scaly"},
	{"brown", "carnelian", "", "chestnut"},
	{"blue", "sapphire", "", ""},
	{"purple", "azurite", "", ""},
	{"dark cyan", "chalcedony", "", ""},
	{"grey", "", "silver", "piebald"},

	{"dark grey", "jet", "bronze", "black"},
	{"bright red", "beryl", "", ""},
	{"light green", "peridot", "", ""},
	{"yellow", "citrine", "electrum", "flaxen"},
	{"baby blue", "lapis lazuli", "", ""},
	{"magenta", "amethyst", "", ""},
	{"cyan", "jade", "", ""},
	{"white", "ivory", "platinum", "white"}
};

const static mondef mondefs[] = {
	{
		.asmob = {15, 0, 1},
		.about = {'@', 15, "rogue", "a swarthy looking fellow with a big ego and ambitions to match"}
	}, {
		.asmob = {3, MOB_STUMBLE | MOB_PATHS, 3},
		.about = {'d', 11, "drunkard", "this drunken outlaw of the deep woods stumbles about, hoping to pick a fight"}
	}, {
		.asmob = {3, MOB_DUMB, 1},
		.about = {'s', 11, "sot", "he's spent all his life in opium dens; now he's after you"}
	}, {
		.asmob = {1, MOB_PATHS, 2},
		.about = {'h', 3, "hound", "snarling and frothing, its snout contorted with rage, it eyes your neck like a sirloin steak."}
	}, {
		.asmob = {5},
		.about = {'r', 15, "charioteer", "his horses are at pasture, but no circumstance can rob him of his lance or his aristocratic overconfidence"}
	}, {
		.asmob = {7},
		.about = {'h', 11, "horse", "better to meet the horse than for the horse to meet his master!"}
	}, {
		.asmob = {1},
		.about = {'P', 15, "pawn", "he's not much in a fight, but what he lacks in brawn he makes up in disposability"}
	}, {
		.asmob = {8},
		.about = {'K', 15, "knight", "square jaw, aquiline nose, burnished armor, mounted on a half-wild stallion: if he doesn't kill you, he'll seduce your wife"}
	}, {
		.asmob = {5},
		.about = {'R', 15, "rook", "the greatest of the king's charioteers stands astride the finest of chariots"}
	}, {
		.asmob = {5},
		.about = {'B', 15, "bishop", "this cardinal would sooner die than step out of line"}
	}, {
		.asmob = {5, MOB_PATHS, 10},
		.about = {'Q', 15, "queen", "the king's consort is the power behind the throne"}
	}, {
		.asmob = {1, MOB_PATHS | MOB_ROYAL_CAPTURE, 15},
		.about = {'K', 15, "king", ""}
	},
	{{'\0'}}
};


const static mondef itemdefs[] = {
	{
		.asitem = {.hacktype = 1},
		.about = {'*', 15, "orb of the Turk", "\"the bearer of the orb,\" said the tinker, \"cannae make mistakes.\""}
	}, {
		.asitem = {.hacktype = 2},
		.about = {'*', 15, "ring of vulnerability", "a single hit will kill the wearer of the ring"}
	}, {
		.asitem = {.hacktype = 3},
		.about = {'*', 15, "ring of capture", "the wearer of the ring must advance while killing"}
	},
	{.about = {'/', 15, "dagger", ""}},
	{.about = {'/', 15, "axe", ""}},

	{
		.asitem = {.hacktype = 4},
		.about = {'?', 15, "scroll of teleport", ""}
	}, {
		.asitem = {.hacktype = 5},
		.about = {'?', 15, "scroll of impunity", ""}
	},
	{.about = {'?', 15, "scroll of slaughter", ""}},
	{.about = {'?', 15, "scroll of blur", ""}},

	{
		.about = {'!', 15, "potion of healing", ""}},
	{.about = {'!', 15, "potion of destruction", ""}},
	{{'\0'}}
};

const static mondef celldefs[] = {
	// nonterminal
	{
		.ascell = {0},
		.about = {'?', 15, "blank", ""}
	}, {
		.ascell = {0},
		.about = {',', 3, "forest", ""}
	},
	// these ones actually exist:
	{
		.ascell = {0},
		.about = {',', 2, "grass", ""}
	}, {
		.ascell = {CELL_BLOCK | CELL_PROMOTE_ON_STEP, "grass"},
		.about = {'"', 2, "brush", ""}
	}, {
		.ascell = {CELL_STOP | CELL_BLOCK},
		.about = {'#', 7, "rock", ""}
	}, {
		.ascell = {CELL_STOP | CELL_BLOCK},
		.about = {'&', 2, "tree", ""}
	}, {
		.ascell = {0},
		.about = {'.', 7, "floor", ""}
	}, {
		.ascell = {CELL_STOP | CELL_BLOCK},
		.about = {'#', 7, "wall", ""}
	}, {
		.ascell = {CELL_KILL},
		.about = {'~', 11 + 1 * 16, "magma", ""}
	}, {
		.ascell = {CELL_STOP},
		.about = {'~', 4, "water", ""}
	}, {
		.ascell = {CELL_BLOCK},
		.about = {'+', 9, "door", ""}
	}, {
		.ascell = {CELL_STOP},
		.about = {'"', 12, "window", ""}
	}, {
		.ascell = {0},
		.about = {'<', 7, "down", ""}
	},
	{
		.ascell = {0},
		.about = {'>', 7, "up", ""}
	},
	{{'\0'}}
};

static const mondef *lookup(int whichlist, const char *tag) {
	int i;
	const mondef *list = mondefs;
	if (whichlist == 1) list = itemdefs;
	if (whichlist == 2) list = celldefs;

	for (i = 0; list[i].about.glyph != '\0'; i++) {
		if (strcmp(list[i].about.name, tag) == 0) {
			return list + i;
		}
	}

	return 0; // change this
}


struct catalog_t Catalog = {
	lookup,
	0, 1, 2
};

