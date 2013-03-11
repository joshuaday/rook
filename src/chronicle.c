#include "mods.h"
#include <stdlib.h>
#include <string.h>

chronicle *chronicle_fork(chronicle *parent) {
	chronicle *chron = malloc(sizeof(chronicle));
	if (parent && parent->last) {
		chron->last = parent->last;
		parent->last->fork_count++;
		chron->count = parent->count;
		chron->turn_number = 1 + parent->turn_number;
	} else {
		chron->last = NULL;
		chron->turn_number = 0;
		chron->count = 0;
	}
	
	return chron;
}

void chronicle_post (chronicle *chron, const char *text) {
	message *msg = malloc(sizeof(message));

	int length = strlen(text);
	msg->length = length;
	msg->text = malloc(length + 1);
	msg->turn_number = chron->turn_number;
	msg->fork_count = 0;

	msg->prev = chron->last;
	chron->last = msg;
	chron->count++;

	memcpy(msg->text, text, length + 1);
}

void chronicle_free (chronicle *chron) {
	message *msg = chron->last;

	free(chron);
	while (msg != NULL && !msg->fork_count) {
		message *prev = msg->prev;
		free(msg);
		msg = prev;
	}
	if (msg != NULL) {
		msg->fork_count--;
	}
}

struct chronicle_t Chronicle = {
	chronicle_fork,
	chronicle_post,
	chronicle_free
};

