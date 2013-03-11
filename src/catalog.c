#include "mods.h"
#include <string.h>

const static struct colorflavor {
	const char *Term, *gem, *metal, *horse;
} colorflavors[16] = {
	{"black", "", "", ""},
	{"red", "ruby", "", "blood-red"},
	{"green", "chryosite", "", "serpentine"},
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
		.forms = {.tag = "rogue"},
		.asmob = {15, 0, 1},
		.about = {'@', 15}
	}, {
		.forms = {.tag = "drunkard"},
		.asmob = {3, MOB_STUMBLE | MOB_PATHS, 3},
		.about = {'d', 11}
	}, {
		.forms = {.tag = "sot"},
		.asmob = {3, MOB_DUMB, 1},
		.about = {'s', 11}
	}, {
		.forms = {.tag = "hound"},
		.asmob = {1, MOB_PATHS, 4},
		.about = {'h', 3}
	}, {
		.forms = {.tag = "charioteer"},
		.asmob = {5},
		.about = {'r', 15}
	}, {
		.forms = {.tag = "horse"},
		.asmob = {7},
		.about = {'h', 11}
	}, {
		.forms = {.tag = "pawn"},
		.asmob = {1},
		.about = {'P', 15}
	}, {
		.forms = {.tag = "knight"},
		.asmob = {8},
		.about = {'K', 15}
	}, {
		.forms = {.tag = "rook"},
		.asmob = {5},
		.about = {'R', 15}
	}, {
		.forms = {.tag = "bishop"},
		.asmob = {5},
		.about = {'B', 15}
	}, {
		.forms = {.tag = "queen"},
		.asmob = {5, MOB_PATHS, 10},
		.about = {'Q', 15}
	}, {
		.forms = {.tag = "king"},
		.asmob = {1, MOB_PATHS | MOB_ROYAL_CAPTURE, 15},
		.about = {'K', 15}
	},
	{{'\0'}}
};


const static mondef itemdefs[] = {
	{
		.forms = {.tag = "the Turk's turban"},
		.asitem = {.hacktype = 1},
		.about = {'*', 15}
	}, {
		.forms = {.tag = "ring of vulnerability"},
		.asitem = {.hacktype = 2},
		.about = {'*', 15}
	}, {
		.forms = {.tag = "ring of foreknowledge"},
		.asitem = {.hacktype = 3},
		.about = {'*', 15}
	},
	{
		.forms = {.tag = "scroll of teleport"},
		.asitem = {.hacktype = 4},
		.about = {'?', 15}
	},
	{
		.forms = {.tag = "potion of healing"},
		.about = {'!', 15}
	},
	{{'\0'}}
};

const static mondef celldefs[] = {
	// nonterminal
	{
		.ascell = {0},
		.forms = {.tag = "blank"},
		.about = {'?', 15}
	}, {
		.ascell = {0},
		.forms = {.tag = "forest"},
		.about = {',', 3}
	},
	// these ones actually exist:
	{
		.ascell = {0},
		.forms = {.tag = "grass"},
		.about = {',', 2}
	}, {
		.ascell = {CELL_BLOCK | CELL_PROMOTE_ON_STEP, "grass"},
		.forms = {.tag = "brush"},
		.about = {'"', 2}
	}, {
		.ascell = {CELL_STOP | CELL_BLOCK},
		.forms = {.tag = "rock", .plural = "rocks"},
		.about = {'#', 7}
	}, {
		.ascell = {CELL_STOP | CELL_BLOCK},
		.forms = {.tag = "tree", .plural = "trees"},
		.about = {'&', 2}
	}, {
		.ascell = {0},
		.forms = {.tag = "floor", .plural = "floors"},
		.about = {'.', 7}
	}, {
		.ascell = {CELL_STOP | CELL_BLOCK},
		.forms = {.tag = "wall", .plural = "walls"},
		.about = {'#', 7}
	}, {
		.ascell = {CELL_KILL},
		.forms = {.tag = "magma"},
		.about = {'~', 11 + 1 * 16}
	}, {
		.ascell = {CELL_STOP},
		.forms = {.tag = "water"},
		.about = {'~', 4}
	}, {
		.ascell = {CELL_BLOCK},
		.forms = {.tag = "door", .plural = "doors"},
		.about = {'+', 9}
	}, {
		.ascell = {CELL_STOP},
		.forms = {.tag = "window", .plural = "windows"},
		.about = {'"', 12}
	},
	{{'\0'}}
};

static const mondef *lookup(int whichlist, const char *tag) {
	int i;
	const mondef *list = mondefs;
	if (whichlist == 1) list = itemdefs;
	if (whichlist == 2) list = celldefs;

	for (i = 0; list[i].about.glyph != '\0'; i++) {
		if (strcmp(list[i].forms.tag, tag) == 0) {
			return list + i;
		}
	}

	return 0; // change this
}


struct catalog_t Catalog = {
	lookup,
	0, 1, 2
};

