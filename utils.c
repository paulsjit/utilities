#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "utils.h"

uint32_t gcd(uint32_t a, uint32_t b)
{
	uint32_t res;

	if(!a || !b) {
		util_printf("%s: None of the numbers can be 0\n", __func__);
		util_fatal(0);
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

int zziter_next(struct zigzag_iterator *z, uint32_t *x, uint32_t *y)
{
	if(z->x >= z->hmax ||  z->y >= z->vmax)
		z->reset_needed = true;

	if(z->reset_needed)
		return -1;

	*x = z->x; *y = z->y;

	if(z->x < z->hmax && z->y < z->vmax) {
		if(z->diag_down) {
			z->x--;
			z->y++;
			if(z->x < 0 && z->y < z->vmax) {
				z->x++;
				z->diag_down = false;
				z->diag_up = true;
			} else if(z->y == z->vmax) {
				z->x++; z->x++;
				z->y--;
				z->diag_down = false;
				z->diag_up = true;
			}
		} else if(z->diag_up) {
			z->x++;
			z->y--;
			if(z->y < 0 && z->x < z->hmax) {
				z->y++;
				z->diag_up = false;
				z->diag_down = true;
			} else if(z->x == z->hmax) {
				z->y++; z->y++;
				z->x--;
				z->diag_up = false;
				z->diag_down = true;
			}

		}
	}

	return 0;
}

struct perm_iterator {
	void **values;
	int num_lists;
	int *max_indices;
	int *curr_indices;
	int *sizes;
	bool reset_needed;
};

void perm_iter_init(struct perm_iterator *p, uint32_t num_lists)
{
	p->num_lists = num_lists;
	p->values = calloc(num_lists, sizeof(void *));

	p->sizes = calloc(num_lists, sizeof(int));
	p->max_indices = calloc(num_lists, sizeof(int));
	p->curr_indices = calloc(num_lists, sizeof(int));
	p->reset_needed = true;
}

void perm_iter_reset(struct perm_iterator *p)
{
	if(any_equal_to(uintptr_t, p->values, p->num_lists, NULL)) {
		util_printf("%s: not all lists are initialised\n", __func__);
		util_fatal(0);
	}

	memset(p->curr_indices, 0, p->num_lists * sizeof(int));
	p->reset_needed = false;
}
void _perm_iter_add_str(struct perm_iterator *p, uint32_t idx, size_t argc, ...)
{
	va_list ap;
	int i;
	char **store = NULL;

	if(idx >= p->num_lists){
		util_printf("%s: idx out of bounds\n", __func__);
		util_fatal(0);
	}

	if(p->values[idx]) {
		util_printf("%s: list already initialised\n", __func__);
		util_fatal(0);
	}

	va_start(ap, argc);

	p->values[idx] = calloc(sizeof(char *), argc);
	store = p->values[idx];

	for (i = 0; i < argc; i++) {
		store[i] = va_arg(ap, char *);
	}
	va_end(ap);

	p->sizes[idx] = sizeof(char *);
	p->max_indices[idx] = argc;
}

void _perm_iter_add_uint32_t(struct perm_iterator *p, uint32_t idx, size_t argc, ...)
{
	va_list ap;
	int i;
	uint32_t *store = NULL;

	if(idx >= p->num_lists){
		util_printf("%s: idx out of bounds\n", __func__);
		util_fatal(0);
	}

	if(p->values[idx]) {
		util_printf("%s: list already initialised\n", __func__);
		util_fatal(0);
	}

	va_start(ap, argc);

	p->values[idx] = calloc(sizeof(uint32_t), argc);
	store = p->values[idx];

	for (i = 0; i < argc; i++) {
		store[i] = va_arg(ap, uint32_t);
	}
	va_end(ap);

	p->sizes[idx] = sizeof(uint32_t);
	p->max_indices[idx] = argc;
}

void perm_iter_add_range_uint32_t(struct perm_iterator *p, uint32_t idx, uint32_t min, uint32_t max, uint32_t incr)
{
	uint32_t i;
	uint32_t *store = NULL;

	if(idx >= p->num_lists){
		util_printf("%s: idx out of bounds\n", __func__);
		util_fatal(0);
	}

	if(p->values[idx]) {
		util_printf("%s: list already initialised\n", __func__);
		util_fatal(0);
	}

	for(i = min; i < max; i+= incr) {
		p->values[idx] = realloc(p->values[idx], (p->max_indices[idx] + 1) * sizeof(uint32_t));
		store = p->values[idx];
		store[p->max_indices[idx]] = i;
		p->max_indices[idx]++;
	}

	p->sizes[idx] = sizeof(uint32_t);
}

void perm_iter_get_values(struct perm_iterator *p, ...)
{
	va_list ap;
	int i;
	void *store = NULL;
	uint8_t *load = NULL;

	if(p->reset_needed)
		return;

	va_start(ap, p);

	for(i = 0; i < p->num_lists; i++) {
		store = va_arg(ap, void *);
		load = p->values[i];
		memcpy(store, load + (p->curr_indices[i] * p->sizes[i]), p->sizes[i]);
	}

	va_end(ap);
}

int _perm_iter_next(struct perm_iterator *p, uint32_t idx)
{
	uint32_t last = idx;

	p->curr_indices[idx]++;
	if(p->curr_indices[idx] >= p->max_indices[idx]) {
		p->curr_indices[idx] = 0;
		if(idx < p->num_lists)
			last = _perm_iter_next(p, idx + 1);
		else {
			last++;
			p->reset_needed = true;
		}
	}

	return last;
}

int perm_iter_next(struct perm_iterator *p)
{
	if(p->reset_needed)
		return -1;

	return _perm_iter_next(p, 0) < p->num_lists ? 0 : -1;
}

#if defined(PERM_ITER_TEST_CODE)
int main()
{
	struct perm_iterator perm;

	perm_iter_init(&perm, 7);

	perm_iter_add_str(&perm, 0, "hello", "world");
	perm_iter_add_uint32_t(&perm, 1, 0, 1, 2, 3);
	perm_iter_add_uint32_t(&perm, 2, 4, 5);
	perm_iter_add_uint32_t(&perm, 3, 6, 7, 8);
	perm_iter_add_uint32_t(&perm, 4, 9);
	perm_iter_add_uint32_t(&perm, 5, 10, 11, 12, 13);
	perm_iter_add_range_uint32_t(&perm, 6, 0, 16, 1);

	perm_iter_reset(&perm);

	uint32_t v0, v1, v2, v3, v4, v5;
	char *str;

	do {
		perm_iter_get_values(&perm, &str, &v0, &v1, &v2, &v3, &v4, &v5);
		printf("%s-%u-%u-%u-%u-%u-%u\n",
				str, v0, v1, v2, v3, v4, v5);
	} while(perm_iter_next(&perm) != -1);
}
#endif
