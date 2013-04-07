#include "mods.h"
#include <time.h>
#include <stdio.h>

#define RAND_MAX_COMBO UINT32_MAX
#define rot(x,k) (((x)<<(k))|((x)>>(32-(k))))

/* version 2011 Apr 12 */

static uint32_t rngget(rng *x) {
	x->count++;

    uint32_t e = x->a - rot(x->b, 27);
    x->a = x->b ^ rot(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

static void rnginit(rng *x, uint32_t seed) {
    uint32_t i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
	x->seed = seed;
	x->count = 0;

    for (i = 0; i < 20; ++i) {
        (void) rngget(x);
    }
}


static rng seed(uint32_t seed) {
	if (seed == 0) {
		seed = (uint32_t) time(NULL);
	}

	rng state;
	rnginit(&state, seed);

	return state;
}

uint32_t randint(rng *state, uint32_t range) {
	uint32_t split = RAND_MAX_COMBO / range, roll;
	while ((roll = rngget(state) / split) >= range);

	return roll;
}

static int roll_readnum(const char **dice) {
	int number = 0;
	while (**dice >= '0' && **dice <= '9') {
		number = number * 10 + (**dice - '0');
		(*dice)++;
	}
	return number;
}

static void roll_whitespace(const char **dice) {
	while (**dice != '\0' && **dice <= ' ') {
		(*dice)++;
	}
}

static int roll_expression(rng *state, const char **dice, char terminal) {
	int lhs = 0, operator, rhs = 0;
	roll_whitespace(dice);
	if (**dice == '(') {
		(*dice)++;
		lhs = roll_expression(state, dice, ')');
		if (**dice == ')') (*dice)++;
	}
	if ((**dice >= '0' && **dice <= '9') || **dice == '-') {
		lhs = roll_readnum(dice);
	}
	roll_whitespace(dice);
	if (**dice == terminal || **dice == '\0') return lhs;
	operator = **dice;
	(*dice)++;
	rhs = roll_expression(state, dice, terminal);
	roll_whitespace(dice);

	if (**dice != terminal) {
		fprintf(stderr, "Could not parse dice\n");
		return 0;
	}

	switch (operator) {
		case '+':
			return lhs + rhs;
		case '-':
			return lhs - rhs;
		case 'd': {
			if (lhs == 0) lhs = 1;
			int i, sum = lhs;
			for (i = 0; i < lhs; i++) {
				sum += randint(state, rhs);
			}
			return sum;
		}
		case '=':
			return lhs == rhs;
		case '>':
			return lhs > rhs;
		case '<':
			return lhs < rhs;
		case '?':
			return lhs ? rhs : 0;
		case ':':
			return lhs ? lhs : rhs;
		case '/':
			return (randint(state, rhs) < lhs) ? 1 : 0;
		case '*':
			return lhs * rhs;
		case '%':
			return (randint(state, 100) < lhs) ? rhs : 0;
		default:
			return lhs;
	}
}

static int roll (rng *state, const char *dice) {
	if (dice == NULL) return 0;
	int value = roll_expression(state, &dice, '\0');
	return value > 0 ? value : 0;
}

struct rng_t Rng = {
	seed,
	randint,
	roll
};

