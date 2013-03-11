#include "mods.h"
#include <stdlib.h>
#include <ncurses.h>

#define COLORING(fg,bg) (((fg) & 0x0f) | (((bg) & 0x07) << 4))
#define COLOR_FG(color,fg) (((fg) & 0x0f) + ((color) & 0x70))
#define COLOR_BG(color,bg) (((color) & 0x0f) + (((bg) & 0x07) << 4))
#define COLOR_INDEX(color) (1 + ((color)&0x07) + (((color) >> 1) & 0x38))
#define COLOR_ATTR(color) (COLOR_PAIR(COLOR_INDEX(color)) | (((color)&0x08) ? A_BOLD : 0))

static struct { int curses, color; } videomode = { 0, 0 } ;

static void preparecolor ( ) {
	static int pairParts[8] = {
		COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
		COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
	};
	
	int fg, bg; 
	for (bg=0; bg<8; bg++) {
		for (fg=0; fg<8; fg++) {
			init_pair(
				COLOR_INDEX(COLORING(fg, bg)),
				pairParts[fg], pairParts[bg]
			);
		}
	}
}

static void cleanup( ) {
	if (videomode.curses) {
		clear();
		refresh();
		endwin();
	}
}

static int curses_init( ) {
	if (videomode.curses) return 0;
	
	// isterm?
	initscr( );
	if (!has_colors( )) {
		endwin( );
		fprintf (stderr, "Your terminal has no color support.\n");
		return 1;
	}
	
	start_color( );
	clear( );
	curs_set( 0 );
	refresh( );
	leaveok(stdscr, TRUE);
	preparecolor( );
	cbreak( );
	noecho( );
	meta(stdscr, TRUE);
	keypad(stdscr, TRUE);

	mousemask(BUTTON1_PRESSED | BUTTON1_RELEASED | REPORT_MOUSE_POSITION, NULL);
	mouseinterval(0); //do no click processing, thank you
	
	videomode.curses = 1;

	getmaxyx(stdscr, Term.height, Term.width);

	atexit(cleanup);

	return 1;
}

static void plot(int x, int y, int color, int ch) {
	attrset(COLOR_ATTR(color));
	mvaddch(y, x, ch);
}

static void print(int x, int y, int color, const char *ch) {
	while (*ch) {
		plot(x, y, color, *ch);
		x++;
		ch++;
	}
}

static int getkey( ) {
	while (1) {
		int got = getch();
		if (got == KEY_RESIZE) {
			getmaxyx(stdscr, Term.height, Term.width);
		} else {
			return got;
		}
	}
}

static void term_clear( ) {
	clear( );
}

struct term_t Term = {
	curses_init,
	plot,
	print,
	term_clear,
	getkey,
	refresh
};

