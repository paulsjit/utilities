#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

struct set {
	int nc, dc;
};

struct set *sets = NULL;
int num_sets = 0;

bool set_exists(int nc, int dc)
{
	int i;

	if(!num_sets)
		return false;

	for(i = 0; i < num_sets; i++)
		if(sets[i].nc == nc && sets[i].dc == dc)
			return true;

	return false;
}

int gcd(int a, int b)
{
	int res;

	if(!a || !b) {
		printf("ERROR: GCD(a,b), !a || !b\n");
		exit(0);
	}

	if(b > a) {
		res = b;
		b = a;
		a = res;
	}

	while(b) {
		res = b;
		b = a % b;
		a = res;
	}

	return res;
}

void set_append(int nc, int dc)
{
	sets = realloc(sets, (num_sets + 1) * sizeof(struct set));
	sets[num_sets].nc = nc;
	sets[num_sets].dc = dc;
	num_sets++;
}

int compare_set(void *_a, void *_b)
{
	struct set *a = _a, *b = _b;
	float facta = a->nc / (float)a->dc, factb = b->nc / (float)b->dc;

	if(facta < factb)
		return -1;
	return 1;
}

int main(int argc, char **argv)
{
	int num_max = 16;
	int den_max = 16;
	int nc, dc;
	int i;

	if(argc > 1) {
		num_max = atoi(argv[1]);
		den_max = num_max;
	}

	if(argc > 2)
		den_max = atoi(argv[2]);

	for(nc = 1; nc <= num_max; nc++) {
		for(dc = 1; dc <= den_max; dc++) {
			int g = gcd(nc, dc);
			int _nc = nc;
			int _dc = dc;
			if(g > 1) {
				_nc = nc / g;
				_dc = dc / g;
			}

			if(!set_exists(_nc, _dc))
				set_append(_nc, _dc);
		}
	}

	qsort(sets, num_sets, sizeof(struct set), compare_set);

	for(i = 0; i < num_sets; i++)
		printf("%4d: %3d / %3d (%5.06f)\n", i, sets[i].nc, sets[i].dc, sets[i].nc / (float)sets[i].dc);

}
