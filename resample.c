#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>

int16_t convolve_one(int16_t *filter, int filter_len, int16_t *in, int data_len, int index)
{
	int i, j;
	int lindex = index - filter_len / 2;
	int16_t out = 0;
	uint16_t temp[filter_len];

	memset(temp, 0, filter_len * sizeof(int16_t));
	if(lindex < 0) {
		for(j = -lindex; j < filter_len; j++)
			temp[j] = in[j];
	} else if(lindex + filter_len > data_len) {
		for(j = 0; j < data_len - lindex; j++)
			temp[j] = in[lindex + j];
	} else {
		for(j = 0; j < filter_len; j++)
			temp[j] = in[lindex + j];
	}

	out = 0;
	for(j = 0; j < filter_len; j++)
		out += (temp[j] * filter[filter_len - j - 1]);

	return out;
}

double sinc(double x)
{
	if(x == 0)
		return 1;

	return sin(x) / x;
}

void gen_filter(int16_t **filter, int *filter_len, int factor, int bank_len)
{
	double x;
	int l;

	*filter_len = factor * bank_len;

	*filter = calloc(sizeof(int16_t), *filter_len);

	for(x = -(*filter_len / (2.0 * factor)) * M_PI, l = 0; l < *filter_len; l++, x += ((*filter_len / (2.0 * factor)) * M_PI / (*filter_len / 2.0)))
		(*filter)[l] = (int16_t)(sinc(x) * 32);
}

int gcd(int a, int b)
{
	int res;

	if(!a || !b)
		exit(0);
	if(a < 0 || b < 0)
		exit(0);

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

int max(int a, int b)
{
	if(a > b)
		return a;
	return b;
}

int main(int argc, char **argv)
{
	int i, j;
	int ffile, ifile, ofile;
	int opt;
	int filter_len;
	int up_factor, down_factor;
	int out_len = -1;
	int data_len = -1;
	int bank_len = -1;
	int16_t *filter, *in_data, *out_data;
	int16_t **banks;

	while((opt = getopt(argc, argv, "d:o:b:")) != -1) {
		switch(opt) {
			case 'd':
				data_len = atoi(optarg);
				break;
			case 'b':
				bank_len = atoi(optarg);
				break;
			case 'o':
				out_len = atoi(optarg);
				break;
			default:
				printf("unsupported option %c\n", opt);
				exit(0);
		}
	}

	if(out_len < 0 || data_len < 0 || bank_len < 0) {
		printf("give me stuff\n");
		exit(0);
	}

	up_factor = out_len / gcd(data_len, out_len);
	down_factor = data_len / gcd(data_len, out_len);

	srandom(time(0));

	in_data = calloc(data_len, sizeof(int16_t));
	out_data = calloc(out_len, sizeof(int16_t));

	for(i = 0; i < data_len; i++)
		in_data[i] = (random() % 128) - 64;

	gen_filter(&filter, &filter_len, max(up_factor, down_factor), bank_len);

	banks = calloc(up_factor, sizeof(int16_t *));
	for(i = 0; i < up_factor; i++) {
		banks[i] = calloc(bank_len, sizeof(int16_t));
		for(j = 0; j < bank_len; j++)
			banks[i][j] = filter[i + j * up_factor];
	}

	for(i = 0; i < out_len; i++) {
		int up_index = i * down_factor;
		int bank = up_index % up_factor;
		int index = up_index / up_factor;
		out_data[i] = convolve_one(banks[bank], bank_len, in_data, data_len, index) >> 5;
	}

}

