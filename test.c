#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct v {
	int u, d, *b, **p;
};
#define PASS (0xdeadcafe)
#define ZERO (0xdeadb00b)

int main()
{
	int u, d;
	int filter_len = 5;
	int pmax = 512;
	int umax = 16, dmax = 16;
	int x = 0;
	struct v *v = malloc(sizeof(struct v) * umax * dmax);
	int i, j, k, p;

	for(u = 1; u <= umax; u++)
		for(d = 1; d <= dmax; d++) {
			int *bank_indices = malloc(pmax * sizeof(int));
			int **pix_indices = malloc(filter_len * sizeof(int *));
			for(p = 0; p < filter_len; p++)
				pix_indices[p] = malloc(pmax * sizeof(int));

			if(u == 1 && d == 1) {
				for(p = 0; p < pmax; p++) {
					bank_indices[p] = PASS;
					for(j = 0; j < filter_len; j++) {
						int *parr = pix_indices[j];
						parr[p] = PASS;
					}
				}
			} else {
				for(p = 0; p < pmax; p++) {
					int up_index = p * d;
					bank_indices[p] = up_index % u;

					for(j = 0; j < filter_len; j++) {
						int *parr = pix_indices[j];
						parr[p] = ZERO;
					}

					int index = up_index / u;
					int lindex = index - filter_len / 2;
					if(lindex < 0) {
						for(j = -lindex; j < filter_len; j++) {
							int *parr = pix_indices[j];
							parr[p] = j;
						}
					} else {
						for(j = 0; j < filter_len; j++) {
							int *parr = pix_indices[j];
							parr[p] = lindex + j;
						}
					}
				}

			}

			v[x].u = u;
			v[x].d = d;
			v[x].b = bank_indices;
			v[x].p = pix_indices;
			x++;
		}

	for(i = 0; i < x; i++) {
		printf("u = %u, d = %u, n = %u\n", v[i].u, v[i].d, filter_len);

		printf("--------------------------\n");
		printf("oidx    : ");
		for(j = 0; j < pmax; j++) {
			printf("%4u ", j);
			if(j && j % 16 == 15)
				printf("| ");
		}
		printf("\n");

		printf("bank    : ");
		for(j = 0; j < pmax; j++) {
			char *str;
			bool a = false;
			switch(v[i].b[j]) {
				case PASS:
					str = "   p"; break;
				case ZERO:
					str = "   z"; break;
				default:
					asprintf(&str, "%4u", v[i].b[j]); a = true; break;
			}
			printf("%s ", str);
			if(a)
				free(str);
			if(j && j % 16 == 15)
				printf("| ");
		}
		printf("\n");

		for(k = 0; k < filter_len; k++) {
			printf("iidx[%u] : ", k);
			for(j = 0; j < pmax; j++) {
				char *str;
				bool a = false;
				switch(v[i].p[k][j]) {
					case PASS:
						str = "   p"; break;
					case ZERO:
						str = "   z"; break;
					default:
						asprintf(&str, "%4u", v[i].p[k][j]); a = true; break;
				}
				printf("%s ", str);
				if(a)
					free(str);
				if(j && j % 16 == 15)
					printf("| ");
			}
			printf("\n");
		}
		printf("--------------------------\n");
	}

}
