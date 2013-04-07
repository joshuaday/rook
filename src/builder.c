#include "mods.h"

// #define IN_BOUNDS(x) ((x) - buffer >= 0 && (x) - buffer < 1024)
#define IN_BOUNDS(x) (1)
static char buffer[1024];
static char *writer;

static int enabled = 1;

static void copy(char **target, const char *text) {
	while (*text && IN_BOUNDS(*target)) {
		*((*target) ++) = *(text ++);
	}
}

static struct builder_t write(const char *text) {
	if (enabled) {
		copy(&writer, text);
	}
	return Builder;
}

static struct builder_t name(const mondef *def, int mode) {
	if (def != 0) { // NULL
		while (1) {
			if (mode == 0) {
				if (def->forms.name != 0) { // NULL
					return write(def->forms.name);
				} else {
					return write(def->forms.tag);
				}
			} else if (mode == 1) {
				if (def->forms.plural != 0) {	// NULL
					return write(def->forms.plural);
				} else {
					mode = 0;
				}
			} else {
				return write("bad mode");
			}
		}
	} else {
		return write("null");
	}
}

static struct builder_t clear( ) {
	writer = buffer;
	return Builder;
}

static char *read( ) {
	if (IN_BOUNDS(writer)) {
		*writer = '\0';
		return buffer;
	} else {
		return "Error in string builder.";
	}
}

static void publish(chronicle *chron) {
	if (enabled) {
		Chronicle.post(chron, read( ));
	}
}

static void enable (int on) {
	enabled = on;
}

struct builder_t Builder = {
	clear,
	write,
	name,
	read,
	publish,
	enable
};

