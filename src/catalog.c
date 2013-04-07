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
		.asmob = {15, 0, "1d6 - 1d3", .speed = "60"},
		.about = {'@', 15}
	}, {
		.forms = {.tag = "drunkard"},
		.asmob = {21, MOB_PATHS, "85% 3+2d3", .stumble = "1/2"},
		.about = {'d', 11}
	}, {
		.forms = {.tag = "sot"},
		.asmob = {9, MOB_DUMB, "90% 3"},
		.about = {'s', 11}
	}, {
		.forms = {.tag = "hound"},
		.asmob = {4, MOB_PATHS, "(8d2) - 8"},
		.about = {'h', 3}
	}, {
		.forms = {.tag = "ox"},
		.asmob = {48, MOB_PATHS, "95% 1", .speed = "120"},
		.about = {'o', 7}
	}, {
		.forms = {.tag = "archer"},
		.asmob = {4, MOB_PATHS, "(2d2) - 2", "75% 1d3"},
		.about = {'a', 13}
	}, {
		.forms = {.tag = "charioteer"},
		.asmob = {5},
		.about = {'r', 15}
	}, {
		.forms = {.tag = "horse"},
		.asmob = {15, MOB_PATHS, "33% 1d2", .speed = "30"},
		.about = {'h', 11}
	}, {
		.forms = {.tag = "wasp"},
		.asmob = {1, MOB_PATHS | MOB_FLIES, "2/3", .speed = "20", .sting = "20% (60 * 1d3)"},
		.about = {'w', 11}
	}, {
		.forms = {.tag = "spider"},
		.asmob = {6, MOB_PATHS | MOB_FLIES, "1", .speed = "((1/3) (10 * 2d3)) : 30 * 2d5", .sting = "10 * 1d4"},
		.about = {'s', 15}
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
		.asmob = {21, MOB_PATHS, "10"},
		.about = {'Q', 15}
	}, {
		.forms = {.tag = "king"},
		.asmob = {15, MOB_PATHS | MOB_ROYAL_CAPTURE, "15 + 6d12"},
		.about = {'K', 15}
	},
	{{'\0'}}
};


const static mondef itemdefs[] = {
	{
		.forms = {.tag = "* turk", .name = "the orb of the Turk"},
		.asitem = {.magic = "wt"},
		.about = {'*', 15}
	}, {
		.forms = {.tag = "* vuln", .name = "ring of vulnerability"},
		.asitem = {.magic = "wv"},
		.about = {'*', 15}
	}, {
		.forms = {.tag = "* life", .name = "ring of lifesaving"},
		.asitem = {.magic = "wl"},
		.about = {'*', 15}
	}, {
		.forms = {.tag = "- knife", .name = "knife"},
		.asitem = {.magic = "w-", .stab = "5 + 1d6"},
		.about = {'-', 15}
	}, {
		.forms = {.tag = "- axe", .name = "great axe"},
		.asitem = {.magic = "w-", .stab = "33% 4d8"},
		.about = {'-', 15}
	}, {
		.forms = {.tag = "/ blink", .name = "staff of blinking"},
		.asitem = {.magic = "wqb"},
		.about = {'/', 15}
	}, {
		.forms = {.tag = "/ death", .name = "staff of death"},
		.asitem = {.magic = "wqd"},
		.about = {'/', 15}
	}, {
		.forms = {.tag = "? tele", .name = "scroll of teleportation"},
		.asitem = {.magic = "?t"},
		.about = {'?', 15}
	}, {
		.forms = {.tag = "? charge", .name = "scroll of recharging"},
		.asitem = {.magic = "?c"},
		.about = {'?', 15}
	}, {
		.forms = {.tag = "? undo", .name = "scroll of reversing"},
		.asitem = {.magic = "?u"},
		.about = {'?', 15}
	}, {
		.forms = {.tag = "! heal", .name = "potion of healing"},
		.asitem = {.magic = "!h"},
		.about = {'!', 15}
	}, {
		.forms = {.tag = "! fire", .name = "potion of immolation"},
		.asitem = {.magic = "!f"},
		.about = {'!', 15}
	}, {
		.forms = {.tag = "! para", .name = "potion of paralysis"},
		.asitem = {.magic = "!p"},
		.about = {'!', 15}
	}, {
		.forms = {.tag = "! fast", .name = "potion of haste"},
		.asitem = {.magic = "!H"},
		.about = {'!', 15}
	}, {
		.forms = {.tag = "! slow", .name = "potion of slowness"},
		.asitem = {.magic = "!S"},
		.about = {'!', 15}
	}, {
		{'\0'}
	}
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
		.ascell = {CELL_BLOCK},
		.forms = {.tag = "brush"},
		.about = {'"', 2}
	}, {
		.ascell = { },
		.forms = {.tag = "ash", .plural = "ashes"},
		.about = {';', 7}
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

