/*
 * Remember: i_sf = ceil(f_sf), i.e., for upscale: ceil(f_out / f_in), for downscale: ceil (f_in / f_out)
 * num_phases = i_sf * pG
 * phase increases by num_phases * (1 / f_sf), modulo num_phases, it will never go beyond or equal to num_phases
 * so, if f_sf = 1, 2, 3 etc, sf = 1, 2, 3 respectively
 *     phase will propagate as (0, 0, ...), (0, pG, 0, pG, ...), (0, pG, 2 * pG, 0, pG, 2 * pG, ...) respectively
 * if f_sf = say, 1.7 (=17 / 10), sf = 2, and phase will propagate as 
 *     [
 *       pG * 0.000,
 *       pG * 1.176,
 *       pG * 0.353 [rolled off from pG * 4.353],
 *       pG * 1.529,
 *       pG * 0.706,
 *       pG * 1.882,
 *       pG * 1.059,
 *       pG * 0.235,
 *       pG * 1.412,
 *       pG * 0.588,
 *       pG * 1.765,
 *       pG * 0.941,
 *       pG * 0.118,
 *       pG * 1.294,
 *       pG * 0.471,
 *       pG * 1.647,
 *       pG * 0.823,
 *       pG * 0.000 [roll back to 0 at 17]
 *     ]
 * When this is quantized, with say pG = 4, it will be
 *     [
 *       0,
 *       5,
 *       1,
 *       6,
 *       3,
 *       0, [quantized to 0]
 *       4,
 *       1,
 *       6,
 *       2,
 *       7,
 *       4,
 *       0, [quantized to 0]
 *       5,
 *       2,
 *       7,
 *       3,
 *       0 [true 0 at 17]
 *     ]
 *
 * the pixels are always indiced by round(o_pixel / f_sf)
 */


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
#include <time.h>
#include "utils.h"

struct hist {
	uint64_t min;
	uint64_t max;
	int count;
};

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
			bank[k] = func(p / (double)num_phases, idx);
		}
	}

	return banks;
}

int16_t **quantize_filter_int16_t(double **coeffs, uint32_t numtaps, uint32_t num_phases,
		uint16_t quantization_cap)
{
	int i, j;
	int16_t **int_coeffs = calloc(sizeof(int16_t *), num_phases);

	for(i = 0; i < num_phases; i++) {
		int_coeffs[i] = calloc(numtaps, sizeof(int16_t));
		for(j = 0; j < numtaps; j++)
			int_coeffs[i][j] = (int16_t)round(coeffs[i][j] * quantization_cap);
	}

	return int_coeffs;
}

int16_t **mat(double *v, int vn, double *h, int hn, int q)
{
	int i, j;
	double sum = 0;
	int16_t **ret = malloc(vn * sizeof(int16_t *));
	for(i = 0; i < vn; i++)
		for(j = 0; j < hn; j++)
			sum += v[i] * h[j];

	for(i = 0; i < vn; i++) {
		ret[i] = malloc(hn * sizeof(int16_t));
		for(j = 0; j < hn; j++) {
			ret[i][j] = (int16_t)round(((v[i] * h[j]) / sum) * q);
		}
	}

	return ret;
}

void calculate_phase_info_float(int out_loc, int *in_loc, int *phase, int out_len, int in_len, int num_phases)
{
	double FIR = ((double)in_len) / out_len;
	*phase = (int)(round((out_loc * num_phases * FIR))) % num_phases;
	*in_loc = out_loc * FIR;
}

void calculate_phase_info_int(int out_loc, int *in_loc, int *phase, int out_len, int in_len, int num_phases)
{
	int FIR = (in_len << 10) / out_len;
	*phase = ((out_loc * num_phases * FIR) >> 10) % num_phases;
	*in_loc = (out_loc * FIR) >> 10;
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
	int sf_h, num_phases_h, sf_v, num_phases_v;
	bool conv_2d = false, print_filters = false;
	bool use_multithread = false;
	long number_of_processors = sysconf(_SC_NPROCESSORS_ONLN);
	struct chunk *chunks;
	int num_chunks = 0;
	pthread_t *ths = NULL;
	int16_t ***banks_hv = NULL;
	int iter = 1, iter_cnt = 0;;
	uint64_t max_nsecs_diff = 0;
	uint64_t min_nsecs_diff = 0xffffffffffffffffull;
	uint64_t avg_nsecs_diff = 0;
	int nsecs_bucket_cnt = 32;
	uint64_t min_diff = 0;
	uint64_t max_diff = 32000000ull;
	uint64_t diff_delta = (uint64_t)((max_diff - min_diff) / (float)nsecs_bucket_cnt);
	struct hist *nsecs_buckets;
	struct timespec start, end;
	int cnt;
	void (*phase_info_calc_func)(int, int *, int *, int, int, int);

	phase_info_calc_func = calculate_phase_info_float;

	while((opt = getopt(argc, argv, "1f:w:h:W:H:b:B:q:Q:g:G:t:T:2vmi:")) != -1) {
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
			case 't':
				type_h = optarg;
				break;
			case 'T':
				type_v = optarg;
				break;
			case 'm':
				use_multithread = true;
				break;
			case '2':
				conv_2d = true;
				break;
			case 'v':
				print_filters = true;
				break;
			case 'i':
				iter = atoi(optarg);
				break;
			case '1':
				phase_info_calc_func = calculate_phase_info_int;
				break;
			default:
				printf("unsupported option %c\n", opt);
				exit(0);
		}
	}

	if(number_of_processors % 2)
		use_multithread = false;

	if(!filename_common || in_width == -1 || in_height == -1) {
		printf("missing one or more mandatory options: \"-f filename-prefix\", \"-h <height>\", \"-w <width>\", \"-s <h scaling factor>\", \"-S <v scaling factor>\"\n");
		exit(0);
	}

	if(out_width == -1)
		out_width = in_width;

	if(out_height == -1)
		out_height = in_height;

	sf_h = (out_width > in_width) ? (int)ceil(out_width / (double)in_width) : (int)ceil(in_width / (double)out_width);
	sf_v = (out_height > in_height) ? (int)ceil(out_height / (double)in_height) : (int)ceil(in_height / (double)out_height);
	num_phases_h = sf_h * phase_gran_h;
	num_phases_v = sf_v * phase_gran_v;

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

	if(conv_2d) {
		banks_hv = malloc(sizeof(int16_t **) * num_phases_h * num_phases_v);
		for(i = 0; i < num_phases_v; i++)
			for(j = 0; j < num_phases_h; j++)
				banks_hv[i * num_phases_h + j] = mat(banks_d_v[i], v_bank_len, banks_d_h[j], h_bank_len, 1 << quant_v);
	}

	if(print_filters) {
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
	}

	if(max_diff < 0xffffffffffffffffull)
		nsecs_buckets = malloc((nsecs_bucket_cnt + 1) * sizeof(struct hist));
	else
		nsecs_buckets = malloc((nsecs_bucket_cnt) * sizeof(struct hist));

	nsecs_buckets[0].min = min_diff;
	nsecs_buckets[0].max = nsecs_buckets[0].min + diff_delta;
	nsecs_buckets[0].count = 0;

	for(cnt = 1; cnt < nsecs_bucket_cnt; cnt++) {
		nsecs_buckets[cnt].count = 0;
		nsecs_buckets[cnt].min = nsecs_buckets[cnt - 1].max;
		nsecs_buckets[cnt].max = nsecs_buckets[cnt].min + diff_delta;
	}

	if(max_diff < 0xffffffffffffffffull) {
		nsecs_buckets[cnt].min = nsecs_buckets[cnt - 1].max;
		nsecs_buckets[cnt].max = 0xffffffffffffffffull;
		nsecs_buckets[cnt].count = 0;

		nsecs_bucket_cnt++;
	}

perf_loop:
	clock_gettime(CLOCK_MONOTONIC, &start);

	if(!conv_2d) {
		for(i = 0; i < in_height; i++) {
			for(j = 0; j < out_width; j++) {
				int in_pixel, iphase;

				phase_info_calc_func(j, &in_pixel, &iphase, out_width, in_width, num_phases_h);

				mid_data[i * out_width + j] = convolve_one_uint8_t(banks_h[iphase],
						h_bank_len, in_data + i * in_width, in_width, 1, in_pixel, quant_h);
				mid_data[out_width * in_height + i * out_width + j] = convolve_one_uint8_t(banks_h[iphase],
						h_bank_len, in_data + in_width * in_height + i * in_width, in_width, 1,
						in_pixel, quant_h);
				mid_data[2 * out_width * in_height + i * out_width + j] = convolve_one_uint8_t(banks_h[iphase],
						h_bank_len, in_data + 2 * in_width * in_height + i * in_width, in_width, 1,
						in_pixel, quant_h);
			}
		}

		for(i = 0; i < out_width; i++) {
			for(j = 0; j < out_height; j++) {
				int in_pixel, iphase;
				phase_info_calc_func(j, &in_pixel, &iphase, out_height, in_height, num_phases_v);

				out_data[j * out_width + i] = convolve_one_uint8_t(banks_v[iphase],
						v_bank_len, mid_data + i, in_height, out_width, in_pixel, quant_v);
				out_data[out_width * out_height + j * out_width + i] = convolve_one_uint8_t(banks_v[iphase],
						v_bank_len, mid_data + out_width * in_height + i, in_height, out_width,
						in_pixel, quant_v);
				out_data[2 * out_width * out_height + j * out_width + i] = convolve_one_uint8_t(banks_v[iphase],
						v_bank_len, mid_data + 2 * out_width * in_height + i, in_height, out_width,
						in_pixel, quant_v);
			}
		}
	} else { 
		for(i = 0; i < out_height; i++) {
			for(j = 0; j < out_width; j++) {
				int in_pixel_h, iphase_h;
				int in_pixel_v, iphase_v;
				int16_t **bank;

				phase_info_calc_func(j, &in_pixel_h, &iphase_h, out_width, in_width, num_phases_h);
				phase_info_calc_func(i, &in_pixel_v, &iphase_v, out_height, in_height, num_phases_v);
				bank = banks_hv[iphase_v * num_phases_h + iphase_h];

				out_data[i * out_width + j] = convolve_one_uint8_t_2d(bank, v_bank_len, h_bank_len, in_data,
						in_height, in_width, in_width, 1, in_pixel_v, in_pixel_h, quant_v);
				out_data[out_width * out_height + i * out_width + j] = convolve_one_uint8_t_2d(bank,
						v_bank_len, h_bank_len, in_data + in_width * in_height,
						in_height, in_width, in_width, 1, in_pixel_v, in_pixel_h, quant_v);
				out_data[2 * out_width * out_height + i * out_width + j] = convolve_one_uint8_t_2d(bank,
						v_bank_len, h_bank_len, in_data + 2 * in_width * in_height,
						in_height, in_width, in_width, 1, in_pixel_v, in_pixel_h, quant_v);

			}
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &end);

	iter_cnt++;

	uint64_t nsecs_start = start.tv_sec * 1000000000ull + start.tv_nsec;
	uint64_t nsecs_end   =   end.tv_sec * 1000000000ull +   end.tv_nsec;

	uint64_t nsecs_diff = nsecs_end - nsecs_start;

	if(nsecs_diff > max_nsecs_diff)
		max_nsecs_diff = nsecs_diff;
	if(nsecs_diff < min_nsecs_diff)
		min_nsecs_diff = nsecs_diff;

	avg_nsecs_diff *= (iter_cnt - 1);
	avg_nsecs_diff += nsecs_diff;
	avg_nsecs_diff = (uint64_t)((float)avg_nsecs_diff / iter_cnt);

	for(cnt = 0; cnt < nsecs_bucket_cnt; cnt++)
		if(nsecs_diff >= nsecs_buckets[cnt].min && nsecs_diff < nsecs_buckets[cnt].max) {
			nsecs_buckets[cnt].count++;
			break;
		}

	if(iter_cnt < iter)
		goto perf_loop;


	printf("Total %u  iterations: min = %lu, max = %lu, avg = %lu\n",
			iter, min_nsecs_diff, max_nsecs_diff, avg_nsecs_diff);
	for(cnt = 0; cnt < nsecs_bucket_cnt; cnt++)
		printf("%20lu - %20lu: %5.2f%%\n", nsecs_buckets[cnt].min, nsecs_buckets[cnt].max,
				(nsecs_buckets[cnt].count * 100) /(float)iter);

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
				"-i", arsprintf("%d", iter),
				NULL);
	} else {
		waitpid(pid, NULL, 0);
	}
}
