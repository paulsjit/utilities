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

int16_t convolve_one(int16_t *filter, int filter_len, uint8_t *in, int data_len, int index)
{
	int i, j;
	int lindex = index - filter_len / 2;
	int16_t out = 0;
	int16_t temp[filter_len];

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

void gen_filter(int16_t **filter, int filter_len, int factor)
{
	double x;
	int l;

	*filter = calloc(sizeof(int16_t), filter_len);

	for(x = -(filter_len / (2.0 * factor)) * M_PI, l = 0; l < filter_len; l++, x += ((filter_len / (2.0 * factor)) * M_PI / (filter_len / 2.0)))
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

static inline uint8_t clamp(int16_t in)
{

	if(in >= 256)
		return 0xff;
	else if(in < 0)
		return 0;
	return in;
}

void filter_horz(uint8_t *in, uint8_t *out, int in_width, int out_width, int comm_height, int bank_len)
{
	int16_t *filter;
	int16_t **banks;
	int filter_len;
	int up_factor, down_factor, factor;
	int i, j;
	uint8_t *out_line, *in_line;

	if(in_width == out_width) {
		memcpy(out, in, in_width * comm_height);
		return;
	}

	up_factor = out_width / gcd(in_width, out_width);
	down_factor = in_width / gcd(in_width, out_width);

	factor = max(up_factor, down_factor);

	gen_filter(&filter, up_factor * bank_len, factor);

	banks = calloc(up_factor, sizeof(int16_t *));
	for(i = 0; i < up_factor; i++) {
		banks[i] = calloc(bank_len, sizeof(int16_t));
		for(j = 0; j < bank_len; j++)
			banks[i][j] = filter[i + j * up_factor];
	}

	for(j = 0; j < comm_height; j++) {
		in_line = in + j * in_width;
		out_line = out + j * out_width;
		for(i = 0; i < out_width; i++) {
			int up_index = i * down_factor;
			int bank = up_index % up_factor;
			int index = up_index / up_factor;
			out_line[i] = clamp(convolve_one(banks[bank], bank_len, in_line, in_width, index) >> 5);
		}
	}
}

void transpose(uint8_t *in, uint8_t *out, int w, int h)
{
	int r, c;
	uint8_t temp;

	for(r = 0; r < h; r++)
		for(c = 0; c < w; c++) {
			out[c * h + r] = in[r * w + c];
		}
}

int main(int argc, char **argv)
{
	int i;
	int ifd, ofd;
	int isize, osize, msize;
	int opt;
	int out_width = -1, out_height = -1;
	int in_width = -1, in_height = -1;
	int h_bank_len = 5, v_bank_len = 5;
	uint8_t *in_data, *out_data, *mid_data, *mid2, *out2;
	char *filename_common = NULL, *iname, *oname;

	while((opt = getopt(argc, argv, "f:w:h:W:H:b:B:")) != -1) {
		switch(opt) {
			case 'f':
				filename_common = optarg;
				asprintf(&iname, "%s.raw", filename_common);
				asprintf(&oname, "%s-scaled.raw", filename_common);
				break;
			case 'b':
				h_bank_len = atoi(optarg);
				break;
			case 'B':
				v_bank_len = atoi(optarg);
				break;
			case 'w':
				in_width = atoi(optarg);
				break;
			case 'h':
				in_height = atoi(optarg);
				break;
			case 'W':
				out_width = atoi(optarg);
				break;
			case 'H':
				out_height = atoi(optarg);
				break;
			default:
				printf("unsupported option %c\n", opt);
				exit(0);
		}
	}

	if(!filename_common || in_width == -1 || in_height == -1) {
		printf("missing one or more mandatory options: \"-f filename-prefix\", \"-h <height>\", \"-w <width>\"\n");
		exit(0);
	}

	if(out_width == -1)
		out_width = in_width;

	if(out_height == -1)
		out_height = in_height;

	ifd = open(iname, O_RDONLY);
	if(ifd < 0) {
		printf("could not open \"%s\" for reading\n", iname);
		exit(0);
	}

	ofd = open(oname, O_WRONLY | O_TRUNC | O_CREAT, 0664);
	if(ofd < 0) {
		printf("could not open \"%s\" for writing\n", oname);
		exit(0);
	}

	isize = in_width * in_height * 3;
	msize = in_height * out_width * 3;
	osize = out_height * out_width * 3;

	posix_memalign((void **)&in_data, 32, isize);
	posix_memalign((void **)&mid_data, 32, msize);
	posix_memalign((void **)&out_data, 32, osize);
	posix_memalign((void **)&mid2, 32, msize);
	posix_memalign((void **)&out2, 32, osize);

	read(ifd, in_data, isize);
	close(ifd);

	filter_horz(in_data, mid_data, in_width, out_width, in_height, h_bank_len);
	filter_horz(in_data + (in_width * in_height), mid_data + (out_width * in_height), in_width, out_width, in_height, h_bank_len);
	filter_horz(in_data + ( 2 * in_width * in_height), mid_data + (2 * out_width * in_height), in_width, out_width, in_height, h_bank_len);

	transpose(mid_data, mid2, out_width, in_height);
	transpose(mid_data + (out_width * in_height), mid2 + (out_width * in_height), out_width, in_height);
	transpose(mid_data + (2 * out_width * in_height), mid2 + (2 * out_width * in_height), out_width, in_height);

	filter_horz(mid2, out_data, in_height, out_height, out_width, v_bank_len);
	filter_horz(mid2 + (out_width * in_height), out_data + (out_width * out_height), in_height, out_height, out_width, v_bank_len);
	filter_horz(mid2 + (2 * out_width * in_height), out_data + (2 * out_width * out_height), in_height, out_height, out_width, v_bank_len);

	transpose(out_data, out2, out_height, out_width);
	transpose(out_data + (out_width * out_height), out2 + (out_width * out_height), out_height, out_width);
	transpose(out_data + (2 * out_width * out_height), out2 + (2 * out_width * out_height), out_height, out_width);

	write(ofd, out2, osize);
	close(ofd);
}

