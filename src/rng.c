#include "mods.h"
#include <time.h>

#define RAND_MAX_COMBO UINT32_MAX
#define rot(x,k) (((x)<<(k))|((x)>>(32-(k))))

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

struct rng_t Rng = {
	seed,
	randint
};

