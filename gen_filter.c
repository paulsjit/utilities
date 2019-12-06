#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include "utils.h"

static inline uint8_t clamp_uint8_t(uint16_t in)
{

	if(in >= 256)
		return 255;
	return in;
}

static inline int8_t clamp_int8_t(int16_t in)
{

	if(in > 127)
		return 127;
	else if(in < -128)
		return -128;
	return in;
}

int8_t convolve_one_int8_t(int16_t *filter, uint32_t filter_len, int8_t *in,
		uint32_t data_len, uint32_t data_stride, uint32_t data_index,
		uint32_t shift)
{
	int i, j;
	int16_t out = 0;

	for(i = data_index - filter_len / 2, j = 0; j < filter_len; i++, j++) {
		if(i < 0 || i >= data_len)
			continue;
		out += (in[i * data_stride] * filter[filter_len - j -1]);
	}

	return clamp_int8_t(out >> shift);
}

uint8_t convolve_one_uint8_t(int16_t *filter, uint32_t filter_len, uint8_t *in,
		uint32_t data_len, uint32_t data_stride, uint32_t data_index,
		uint32_t shift)
{
	int i, j;
	uint16_t out = 0;

	for(i = data_index - filter_len / 2, j = 0; j < filter_len; i++, j++) {
		if(i < 0 || i >= data_len)
			continue;
		out += (in[i * data_stride] * filter[filter_len - j - 1]);
	}

	return clamp_uint8_t(out >> shift);
}

uint8_t convolve_one_uint8_t_2d(int16_t **filter, uint32_t filter_len_v, uint32_t filter_len_h, uint8_t *in,
		uint32_t data_len_v, uint32_t data_len_h, uint32_t data_stride_v, uint32_t data_stride_h,
		uint32_t data_index_v, uint32_t data_index_h, uint32_t shift)
{
	int iv, jv, ih, jh;
	uint16_t out = 0;

	for(iv = data_index_v - filter_len_v / 2, jv = 0; jv < filter_len_v; iv++, jv++) {
		int16_t *filter_one = filter[filter_len_v - jv - 1];
		uint8_t *in_one = &in[iv * data_stride_v];

		for(ih = data_index_h - filter_len_h / 2, jh = 0; jh < filter_len_h; ih++, jh++) {
			if(iv < 0 || iv >= data_len_v || ih < 0 || ih >= data_len_h)
				continue;
			out += (in_one[ih * data_stride_h] * filter_one[filter_len_h - jh - 1]);
		}
	}
	return clamp_uint8_t(out >> shift);
}


double sinc(double x)
{
	if (x == 0)
		return 1;

	return sin(x) / x;
}

double cubic(double x)
{
	double B = 0;
	double C = 1;
	double ax = fabs(x);

	if (ax > 2)
		return 0;
	else if(ax >= 1)
		return ((-B - 6 * C) * (ax * ax * ax) +
				(6 * B + 30 * C) * (ax * ax) +
				(-12 * B - 48 * C) * (ax) +
				(8 * B + 24 * C)) / 6;
	else
		return ((12 - 9 * B - 6 * C) * (ax * ax * ax) +
				(-18 + 12 * B + 6 * C) * (ax * ax) +
				(6 - 2 * B)) / 6;
}

double linear(double x)
{
	double ax = fabs(x);

	if(ax >= 1)
		return 0;

	return 1 - ax;
}
double cubic_coeff(double phase, int index)
{
	return cubic(phase + index);
}

double linear_coeff(double phase, int index)
{
	return linear(phase + index);
}

double sinc_coeff(double phase, int index)
{
	return sinc((phase * M_PI) + (index * M_PI));
}

double **gen_filter(char *type, int bank_numtaps, int num_phases)
{
	double **banks = calloc(num_phases, sizeof(double *));
	double (*func)(double phase, int index);

	if(!strcmp(type, "linear"))
		func = linear_coeff;
	else if(!strcmp(type, "cubic"))
		func = cubic_coeff;
	else if(!strcmp(type, "sinc"))
		func = sinc_coeff;
	else {
		util_printf("%s: unsupported filter type : %s\n", __func__, type);
		util_fatal(0);
	}

	for(int p = 0; p < num_phases; p++) {
		double *bank;

		banks[p] = calloc(bank_numtaps, sizeof(double));
		bank = banks[p];

		for(int k = 0; k < bank_numtaps; k++) {
			int idx = k - bank_numtaps / 2;
			if(!(bank_numtaps % 2) && bank_numtaps != 2)
				idx++;

			bank[k] = func(p / (double)num_phases, idx);
		}
	}

	return banks;
}

int16_t **quantize_filter_int16_t(double **coeffs, uint32_t numtaps, uint32_t max_phases,
		uint16_t quantization_cap)
{
	int i, j;
	int16_t **int_coeffs = calloc(sizeof(int16_t *), max_phases);

	for(i = 0; i < max_phases; i++) {
		int_coeffs[i] = calloc(numtaps, sizeof(int16_t));
		for(j = 0; j < numtaps; j++)
			int_coeffs[i][j] = (int16_t)(coeffs[i][j] * quantization_cap);
	}

	return int_coeffs;
}

#define MAX_PHASES (320)

int16_t **mat(int16_t *v, int vn, int vq, int16_t *h, int hn, int hq, int q)
{
	int i, j;
	int16_t **ret = malloc(vn * sizeof(int16_t *));
	for(i = 0; i < vn; i++) {
		ret[i] = malloc(hn * sizeof(int16_t));
		for(j = 0; j < hn; j++) {
			ret[i][j] = (int16_t)(((v[i] / (double)(1 << vq))  * (h[j] / (double)(1 << hq))) * (1 << q));
		}
	}

	return ret;
}

int main(int argc, char **argv) {
	int i, j;
	int ifd, ofd;
	int isize, osize, msize;
	int opt;
	int out_width = -1, out_height = -1;
	int in_width = -1, in_height = -1;
	int h_bank_len = 4, v_bank_len = 4;
	int quant_h = 6, quant_v = 6;
	int phase_gran_h = 4, phase_gran_v = 4;
	double **banks_d_h, **banks_d_v;
	int16_t **banks_h, **banks_v;
	uint8_t *in_data, *out_data, *mid_data;
	char *filename_common = NULL, *iname, *oname;
	char *type_h = "sinc", *type_v = "sinc";
	int sf_h = -1, num_phases_h, sf_v = -1, num_phases_v;
	bool conv_2d = false, print_filters = false;

	while((opt = getopt(argc, argv, "f:w:h:W:H:b:B:q:Q:g:G:s:S:t:T:m:v")) != -1) {
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
			case 'q':
				quant_h = atoi(optarg);
				break;
			case 'Q':
				quant_v = atoi(optarg);
				break;
			case 'g':
				phase_gran_h = atoi(optarg);
				break;
			case 'G':
				phase_gran_v = atoi(optarg);
				break;
			case 's':
				sf_h = atoi(optarg);
				break;
			case 'S':
				sf_v = atoi(optarg);
				break;
			case 't':
				type_h = optarg;
				break;
			case 'T':
				type_v = optarg;
				break;
			case 'm':
				conv_2d = true;
				break;
			case 'v':
				print_filters = true;
				break;
			default:
				printf("unsupported option %c\n", opt);
				exit(0);
		}
	}

	num_phases_h = sf_h * phase_gran_h;
	num_phases_v = sf_v * phase_gran_v;

	if(!filename_common || in_width == -1 || in_height == -1 || sf_h == -1 || sf_v == -1) {
		printf("missing one or more mandatory options: \"-f filename-prefix\", \"-h <height>\", \"-w <width>\", \"-s <h scaling factor>\", \"-S <v scaling factor>\"\n");
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
	posix_memalign((void **)&out_data, 32, osize);
	if(!conv_2d)
		posix_memalign((void **)&mid_data, 32, msize);

	read(ifd, in_data, isize);
	close(ifd);

	banks_d_h = gen_filter(type_h, h_bank_len, num_phases_h);
	banks_h = quantize_filter_int16_t(banks_d_h, h_bank_len, num_phases_h, 1 << quant_h);

	banks_d_v = gen_filter(type_v, v_bank_len, num_phases_v);
	banks_v = quantize_filter_int16_t(banks_d_v, v_bank_len, num_phases_v, 1 << quant_v);

	if(!print_filters)
		goto execute;

	for(i = 0; i < num_phases_h; i++) {
		printf("hbank[%3u] : [", i);
		for(j = 0; j < h_bank_len; j++)
			printf("% 1.6lf%s", banks_d_h[i][j], j == h_bank_len - 1 ? "] / [" : " ");
		for(j = 0; j < h_bank_len; j++)
			printf("% 3d%s", banks_h[i][j], j == h_bank_len - 1 ? "]" : " ");
		printf("\n");
	}

	for(i = 0; i < num_phases_v; i++) {
		printf("vbank[%3u] : [", i);
		for(j = 0; j < v_bank_len; j++)
			printf("% 1.6lf%s", banks_d_v[i][j], j == v_bank_len - 1 ? "] / [" : " ");
		for(j = 0; j < v_bank_len; j++)
			printf("% 3d%s", banks_v[i][j], j == v_bank_len - 1 ? "]" : " ");
		printf("\n");
	}

execute:
	if(conv_2d)
		goto convolute_2d;

	for(i = 0; i < in_height; i++) {
		for(j = 0; j < out_width; j++) {
			int in_pixel = (int)(j * (in_width / (double)out_width));
			double phase_diff = j - in_pixel * (out_width / (double)in_width);
			int iphase = (int)round(phase_diff * (num_phases_h / sf_h));


			mid_data[i * out_width + j] = convolve_one_uint8_t(banks_h[iphase], h_bank_len, in_data + i * in_width, in_width, 1, in_pixel, quant_h);
			mid_data[out_width * in_height + i * out_width + j] = convolve_one_uint8_t(banks_h[iphase], h_bank_len, in_data + in_width * in_height + i * in_width, in_width, 1, in_pixel, quant_h);
			mid_data[2 * out_width * in_height + i * out_width + j] = convolve_one_uint8_t(banks_h[iphase], h_bank_len, in_data + 2 * in_width * in_height + i * in_width, in_width, 1, in_pixel, quant_h);
		}
	}

	for(i = 0; i < out_width; i++) {
		for(j = 0; j < out_height; j++) {
			int in_pixel = (int)(j * (in_height / (double)out_height));
			double phase_diff = j - in_pixel * (out_height / (double)in_height);
			int iphase = (int)round(phase_diff * (num_phases_v / sf_v));


			out_data[j * out_width + i] = convolve_one_uint8_t(banks_v[iphase], v_bank_len, mid_data + i, in_height, out_width, in_pixel, quant_v);
			out_data[out_width * out_height + j * out_width + i] = convolve_one_uint8_t(banks_v[iphase], v_bank_len, mid_data + out_width * in_height + i, in_height, out_width, in_pixel, quant_v);
			out_data[2 * out_width * out_height + j * out_width + i] = convolve_one_uint8_t(banks_v[iphase], v_bank_len, mid_data + 2 * out_width * in_height + i, in_height, out_width, in_pixel, quant_v);
		}
	}

	goto done;

convolute_2d:
	for(i = 0; i < out_height; i++) {
		for(j = 0; j < out_width; j++) {
			int in_pixel_h = (int)(j * (in_width / (double)out_width));
			double phase_diff_h = j - in_pixel_h * (out_width / (double)in_width);
			int iphase_h = (int)round(phase_diff_h * (num_phases_h / sf_h));

			int in_pixel_v = (int)(i * (in_height / (double)out_height));
			double phase_diff_v = i - in_pixel_v * (out_height / (double)in_height);
			int iphase_v = (int)round(phase_diff_v * (num_phases_v / sf_v));

			int16_t **bank = mat(banks_v[iphase_v], v_bank_len, quant_v, banks_h[iphase_h], h_bank_len, quant_h, 6);

			out_data[i * out_width + j] = convolve_one_uint8_t_2d(bank, v_bank_len, h_bank_len, in_data,
					in_height, in_width, in_width, 1, in_pixel_v, in_pixel_h, 6);
			out_data[out_width * out_height + i * out_width + j] = convolve_one_uint8_t_2d(bank,
					v_bank_len, h_bank_len, in_data + in_width * in_height,
					in_height, in_width, in_width, 1, in_pixel_v, in_pixel_h, 6);
			out_data[2 * out_width * out_height + i * out_width + j] = convolve_one_uint8_t_2d(bank,
					v_bank_len, h_bank_len, in_data + 2 * in_width * in_height,
					in_height, in_width, in_width, 1, in_pixel_v, in_pixel_h, 6);

		}
	}
done:

	write(ofd, out_data, osize);
	close(ofd);

	pid_t pid;
	if(!(pid = fork())) {
		char *res;

		close(2);
		close(3);

		execlp("./csc_yuv444_to_rgba", "./csc_yuv444_to_rgba",
				"-f", arsprintf("%s-scaled", filename_common),
				"-w", arsprintf("%d", out_width),
				"-h", arsprintf("%d", out_height), "-s", "-m",
				NULL);
	} else {
		waitpid(pid, NULL, 0);
	}
}
