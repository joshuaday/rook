#include "mods.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

int main (int argc, char *argv[]) {
	if (Term.init()) {
		Game.loop();
	}

	return 0;
}

