#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <immintrin.h>
#include <sys/wait.h>

static inline uint8_t clamp(int16_t in)
{

	if(in >= 256)
		return 0xff;
	else if(in < 0)
		return 0;
	return in;
}

struct chunk {
	uint8_t *in;
	uint8_t *out;
	int im_width;
	int im_height;
	int chunk_x;
	int chunk_y;
	int chunk_width;
	int chunk_height;
};

struct hist {
	uint64_t min;
	uint64_t max;
	int count;
};

void *chunkfunc_simd(void *arg)
{
	struct chunk *c = arg;
	uint8_t *iybuf, *iubuf, *ivbuf, *obuf, *iyline, *iuline, *ivline, *oline;
	int istride, ostride;
	int irc, icc, orc, occ;

	istride = c->im_width;
	ostride = c->im_width * 4;

	iybuf = c->in + istride * c->chunk_y + c->chunk_x;
	iubuf = c->in + c->im_width * c->im_height + istride * c->chunk_y + c->chunk_x;
	ivbuf = c->in + 2 * c->im_width * c->im_height + istride * c->chunk_y + c->chunk_x;
	obuf = c->out + ostride * c->chunk_y + c->chunk_x * 4;

	iyline = iybuf;
	iuline = iubuf;
	ivline = ivbuf;
	oline = obuf;

	__m256i y_sub = _mm256_set1_epi8(16);
	__m256i uv_sub = _mm256_set1_epi8(128);

	__m256i ycoeff = _mm256_set1_epi16(74);
	__m256i g_ucoeff = _mm256_set1_epi16(-25);
	__m256i g_vcoeff = _mm256_set1_epi16(-52);
	__m256i b_ucoeff = _mm256_set1_epi16(129);
	__m256i r_vcoeff = _mm256_set1_epi16(102);

	__m256i mask = _mm256_set1_epi16(0xff);
	__m256i zeros = _mm256_setzero_si256();
	__m256i alpha = _mm256_set1_epi8(0xff);

	for(irc = 0, orc = 0; irc < c->chunk_height; irc++, orc++) {
		for(icc = 0, occ = 0; icc < c->chunk_width; icc += 32, occ += (4 * 32)) {
			// load y, u, v
			__m256i y = _mm256_stream_load_si256((__m256i *)(iyline + icc));
			__m256i u = _mm256_stream_load_si256((__m256i *)(iuline + icc));
			__m256i v = _mm256_stream_load_si256((__m256i *)(ivline + icc));

			// y -= 16, u -= 128, v -= 128
			y = _mm256_subs_epu8(y, y_sub);
			u = _mm256_sub_epi8(u, uv_sub);
			v = _mm256_sub_epi8(v, uv_sub);

			// separate y, u and v into 2 streams each
			__m128i y_low  = _mm256_extracti128_si256(y, 0);
			__m128i y_high = _mm256_extracti128_si256(y, 1);

			__m128i u_low  = _mm256_extracti128_si256(u, 0);
			__m128i u_high = _mm256_extracti128_si256(u, 1);

			__m128i v_low  = _mm256_extracti128_si256(v, 0);
			__m128i v_high = _mm256_extracti128_si256(v, 1);

			__m128i r_low, r_high, g_low, g_high, b_low, b_high;

			// first stream
			{
				__m256i r16, g16, b16;
				__m256i r8, g8, b8;

				// convert to 16 bit
				__m256i y16 = _mm256_cvtepu8_epi16(y_low);
				__m256i u16 = _mm256_cvtepi8_epi16(u_low);
				__m256i v16 = _mm256_cvtepi8_epi16(v_low);

				// compute r, g, b (saturated to 0)
				r16 = _mm256_srai_epi16(_mm256_add_epi16(_mm256_mullo_epi16(y16, ycoeff), _mm256_mullo_epi16(v16, r_vcoeff)), 6);

				g16 = _mm256_add_epi16(_mm256_mullo_epi16(y16, ycoeff),  _mm256_mullo_epi16(u16, g_ucoeff));
				g16 =  _mm256_srai_epi16(_mm256_add_epi16(g16,  _mm256_mullo_epi16(v16, g_vcoeff)), 6);

				b16 = _mm256_srai_epi16(_mm256_add_epi16(_mm256_mullo_epi16(y16, ycoeff), _mm256_mullo_epi16(u16, b_ucoeff)), 6);

				//conver 16 bits to (64 bit zero interleaved) 8 bits (saturated to 255)
				r8 = _mm256_packus_epi16(r16, zeros);
				g8 = _mm256_packus_epi16(g16, zeros);
				b8 = _mm256_packus_epi16(b16, zeros);

				//magic, magic ... pack the 8 bits in a __m128i now
				r_low = _mm256_extracti128_si256(r8, 0); 
				r_low =  _mm_or_si128(r_low, _mm256_extracti128_si256(_mm256_bslli_epi128(r8, 8), 1));

				g_low = _mm256_extracti128_si256(g8, 0); 
				g_low =  _mm_or_si128(g_low, _mm256_extracti128_si256(_mm256_bslli_epi128(g8, 8), 1));

				b_low = _mm256_extracti128_si256(b8, 0); 
				b_low =  _mm_or_si128(b_low, _mm256_extracti128_si256(_mm256_bslli_epi128(b8, 8), 1));
			}

			// second stream
			{
				__m256i r16, g16, b16;
				__m256i r8, g8, b8;

				// convert to 16 bit
				__m256i y16 = _mm256_cvtepu8_epi16(y_high);
				__m256i u16 = _mm256_cvtepi8_epi16(u_high);
				__m256i v16 = _mm256_cvtepi8_epi16(v_high);

				// compute r, g, b (saturated to 0)
				r16 = _mm256_srai_epi16(_mm256_add_epi16(_mm256_mullo_epi16(y16, ycoeff), _mm256_mullo_epi16(v16, r_vcoeff)), 6);

				g16 = _mm256_add_epi16(_mm256_mullo_epi16(y16, ycoeff),  _mm256_mullo_epi16(u16, g_ucoeff));
				g16 =  _mm256_srai_epi16(_mm256_add_epi16(g16,  _mm256_mullo_epi16(v16, g_vcoeff)), 6);

				b16 = _mm256_srai_epi16(_mm256_add_epi16(_mm256_mullo_epi16(y16, ycoeff), _mm256_mullo_epi16(u16, b_ucoeff)), 6);

				//conver 16 bits to (64 bit zero interleaved) 8 bits (saturated to 255)
				r8 = _mm256_packus_epi16(r16, zeros);
				g8 = _mm256_packus_epi16(g16, zeros);
				b8 = _mm256_packus_epi16(b16, zeros);

				//magic, magic ... pack the 8 bits in a __m128i now
				r_high = _mm256_extracti128_si256(r8, 0); 
				r_high =  _mm_or_si128(r_high, _mm256_extracti128_si256(_mm256_bslli_epi128(r8, 8), 1));

				g_high = _mm256_extracti128_si256(g8, 0); 
				g_high =  _mm_or_si128(g_high, _mm256_extracti128_si256(_mm256_bslli_epi128(g8, 8), 1));

				b_high = _mm256_extracti128_si256(b8, 0); 
				b_high =  _mm_or_si128(b_high, _mm256_extracti128_si256(_mm256_bslli_epi128(b8, 8), 1));
			}

			__m256i r = _mm256_setzero_si256();
			__m256i g = _mm256_setzero_si256();
			__m256i b = _mm256_setzero_si256();

			__m256i pix8_0 = _mm256_setzero_si256();
			__m256i pix8_1 = _mm256_setzero_si256();
			__m256i pix8_2 = _mm256_setzero_si256();
			__m256i pix8_3 = _mm256_setzero_si256();

			// pack (r_low , r_high), (b_low, b_high)), (g_low, g_high) into __m256i variables
			r = _mm256_inserti128_si256(r, r_low, 0);
			r = _mm256_inserti128_si256(r, r_high, 1);
				
			g = _mm256_inserti128_si256(g, g_low, 0);
			g = _mm256_inserti128_si256(g, g_high, 1);

			b = _mm256_inserti128_si256(b, b_low, 0);
			b = _mm256_inserti128_si256(b, b_high, 1);

			// combine r and g into 16 bit words
			__m256i bit16_0 = _mm256_unpacklo_epi8(r, g);
			__m256i bit16_1 = _mm256_unpackhi_epi8(r, g);
			__m256i bit16_2 = _mm256_unpacklo_epi8(b, alpha);
			__m256i bit16_3 = _mm256_unpackhi_epi8(b, alpha);

			// combine rg and ba into 32 bit words
			__m256i bit32_0 = _mm256_unpacklo_epi16(bit16_0, bit16_2);
			__m256i bit32_1 = _mm256_unpackhi_epi16(bit16_0, bit16_2);
			__m256i bit32_2 = _mm256_unpacklo_epi16(bit16_1, bit16_3);
			__m256i bit32_3 = _mm256_unpackhi_epi16(bit16_1, bit16_3);

			// deinterleave pixels serrated at 128 bit boundaries
			pix8_0 = _mm256_inserti128_si256(pix8_0, _mm256_extracti128_si256(bit32_0, 0), 0);
			pix8_0 = _mm256_inserti128_si256(pix8_0, _mm256_extracti128_si256(bit32_1, 0), 1);

			pix8_1 = _mm256_inserti128_si256(pix8_1, _mm256_extracti128_si256(bit32_2, 0), 0);
			pix8_1 = _mm256_inserti128_si256(pix8_1, _mm256_extracti128_si256(bit32_3, 0), 1);

			pix8_2 = _mm256_inserti128_si256(pix8_2, _mm256_extracti128_si256(bit32_0, 1), 0);
			pix8_2 = _mm256_inserti128_si256(pix8_2, _mm256_extracti128_si256(bit32_1, 1), 1);

			pix8_3 = _mm256_inserti128_si256(pix8_3, _mm256_extracti128_si256(bit32_2, 1), 0);
			pix8_3 = _mm256_inserti128_si256(pix8_3, _mm256_extracti128_si256(bit32_3, 1), 1);


			// write it out
			_mm256_stream_si256((__m256i *)(oline + occ + 0 * 32), pix8_0);
			_mm256_stream_si256((__m256i *)(oline + occ + 1 * 32), pix8_1);
			_mm256_stream_si256((__m256i *)(oline + occ + 2 * 32), pix8_2);
			_mm256_stream_si256((__m256i *)(oline + occ + 3 * 32), pix8_3);

		}

		iyline += istride;
		iuline += istride;
		ivline += istride;
		oline += ostride;
	}

	return NULL;
}

void *chunkfunc_nosimd(void *arg)
{
	struct chunk *c = arg;
	uint8_t *iybuf, *iubuf, *ivbuf, *obuf, *iyline, *iuline, *ivline, *oline;
	int istride, ostride;
	int irc, icc, orc, occ;

	istride = c->im_width;
	ostride = c->im_width * 4;

	iybuf = c->in + istride * c->chunk_y + c->chunk_x;
	iubuf = c->in + c->im_width * c->im_height + istride * c->chunk_y + c->chunk_x;
	ivbuf = c->in + 2 * c->im_width * c->im_height + istride * c->chunk_y + c->chunk_x;
	obuf = c->out + ostride * c->chunk_y + c->chunk_x * 4;

	iyline = iybuf;
	iuline = iubuf;
	ivline = ivbuf;
	oline = obuf;

	for(irc = 0, orc = 0; irc < c->chunk_height; irc++, orc++) {

		for(icc = 0, occ = 0; icc < c->chunk_width; icc++, occ+=4) {

			uint8_t y = iyline[icc];
			uint8_t u = iuline[icc];
			uint8_t v = ivline[icc];

			
			uint8_t y_mod = y - 16;
			int8_t u_mod = u - 128;
			int8_t v_mod = v - 128;

			int16_t ycoeff_r =   74, ycoeff_g =   74, ycoeff_b =   74;
			int16_t ucoeff_r =    0, ucoeff_g = - 25, ucoeff_b =  129;
			int16_t vcoeff_r =  102, vcoeff_g = - 52, vcoeff_b =    0;

			int16_t r = ((ycoeff_r * (uint16_t)y_mod) + (ucoeff_r * (int16_t)u_mod) + (vcoeff_r * (int16_t)v_mod)) >> 6;
			int16_t g = ((ycoeff_g * (uint16_t)y_mod) + (ucoeff_g * (int16_t)u_mod) + (vcoeff_g * (int16_t)v_mod)) >> 6;
			int16_t b = ((ycoeff_b * (uint16_t)y_mod) + (ucoeff_b * (int16_t)u_mod) + (vcoeff_b * (int16_t)v_mod)) >> 6;

			oline[occ] = clamp(r);
			oline[occ + 1] = clamp(g);
			oline[occ + 2] = clamp(b);
			oline[occ + 3] = 0xff;
			

		}

		iyline += istride;
		iuline += istride;
		ivline += istride;
		oline += ostride;
	}

	return NULL;
}

int main(int argc, char **argv)
{
	int ifd, ofd;
	int isize, osize;
	uint8_t *ibuf, *obuf;
	int icnt, ocnt;
	struct timespec start, end;
	int i;
	char *filename_common = NULL, *iname, *oname, *temp_name;
	int width = -1, height = -1;
	bool use_simd = false;
	bool use_multithread = false;
	int opt;
	long number_of_processors = sysconf(_SC_NPROCESSORS_ONLN);
	struct chunk *chunks;
	int num_chunks = 0;
	pthread_t *ths = NULL;
	int cnt;
	pid_t pid;
	void *(*core_function)(void *);
	int iter = 1, iter_cnt = 0;;
	uint64_t max_nsecs_diff = 0;
	uint64_t min_nsecs_diff = 0xffffffffffffffffull;
	uint64_t avg_nsecs_diff = 0;
	int nsecs_bucket_cnt = 32;
	uint64_t min_diff = 0;
	uint64_t max_diff = 32000000ull;
	uint64_t diff_delta = (uint64_t)((max_diff - min_diff) / (float)nsecs_bucket_cnt);
	struct hist *nsecs_buckets;


	while((opt = getopt(argc, argv, "f:w:h:smi:")) != -1) {
		switch(opt) {
			case 'f':
				filename_common = optarg;
				asprintf(&iname, "%s.raw", filename_common);
				asprintf(&temp_name, "%s.raw.conv", filename_common);
				asprintf(&oname, "%s.png", filename_common);
				break;
			case 'w':
				width = atoi(optarg); break;
			case 'h':
				height = atoi(optarg); break;
			case 's':
				use_simd = true; break;
			case 'm':
				use_multithread = true; break;
			case 'i':
				iter = atoi(optarg); break;
			default:
				printf("unsupported option %c\n", opt);
				exit(0);
		}
	}

	if(!filename_common || width == -1 || height == -1) {
		printf("missing one or more mandatory options: \"-f filename-prefix\", \"-h <height>\", \"-w <width>\"\n");
		exit(0);
	}

	if(number_of_processors % 2)
		use_multithread = false;

	if(use_simd && width % 32) {
		printf("width must be multiple of 32\n");
		exit(0);
	}

	if(use_simd && use_multithread && (width / (number_of_processors / 2)) % 32) {
		printf("in MT mode chunk width must be a multiple of 32\n");
		exit(0);
	}

	ifd = open(iname, O_RDONLY);
	if(ifd < 0) {
		printf("could not open \"%s\" for reading\n", iname);
		exit(0);
	}

	ofd = open(temp_name, O_WRONLY | O_TRUNC | O_CREAT, 0664);
	if(ofd < 0) {
		printf("could not open \"%s\" for writing\n", temp_name);
		exit(0);
	}

	isize = width * height * 3;
	osize = height * width * 4;

	posix_memalign((void **)&ibuf, 32, isize);
	posix_memalign((void **)&obuf, 32, osize);

	read(ifd, ibuf, isize);
	close(ifd);

	if(use_simd)
		core_function = chunkfunc_simd;
	else
		core_function = chunkfunc_nosimd;

	if(use_multithread) {
		int i;
		struct chunk *c;
		int n = number_of_processors / 2;

		chunks = calloc(number_of_processors, sizeof(struct chunk));

		c = &chunks[0];

		for(i = 0; i < n; i++) {
			c->in = ibuf;
			c->out = obuf;
			c->im_width = width;
			c->im_height = height;
			c->chunk_y = 0;
			c->chunk_height = height / 2;
			c->chunk_x = i * (width / n);
			c->chunk_width = (width / n);

			c++;
			num_chunks++;
		}

		for(i = 0; i < n; i++) {
			c->in = ibuf;
			c->out = obuf;
			c->im_width = width;
			c->im_height = height;
			c->chunk_y = height / 2;
			c->chunk_height = height / 2;
			c->chunk_x = i * (width / n);
			c->chunk_width = (width / n);

			c++;
			num_chunks++;
		}

		ths = calloc(sizeof(pthread_t), number_of_processors - 1);

	} else {
		struct chunk *c;

		chunks = calloc(1, sizeof(struct chunk));

		c = &chunks[0];

		c->in = ibuf;
		c->out = obuf;
		c->im_width = width;
		c->im_height = height;
		c->chunk_y = 0;
		c->chunk_height = height;
		c->chunk_x = 0;
		c->chunk_width = width;

		num_chunks++;
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

	for(cnt = 0; cnt < num_chunks - 1; cnt++) {
		struct chunk *c = &chunks[cnt];

		pthread_create(&ths[cnt], NULL, core_function, c);
	}

	core_function(&chunks[num_chunks - 1]);

	for(cnt = 0; cnt < num_chunks - 1; cnt++)
		pthread_join(ths[cnt], NULL);

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

	write(ofd, obuf, osize);
	close(ofd);

	if(!(pid = fork())) {
		char *res;

		close(2);
		close(3);

		asprintf(&res, "%ux%u", width, height);

		execlp("ffmpeg", "ffmpeg", "-y", "-vcodec", "rawvideo", "-f",
				"rawvideo", "-pix_fmt", "rgba", "-s",
				res, "-i", temp_name, "-f", "image2", "-vcodec",
				"png", oname, NULL);
	} else {
		waitpid(pid, NULL, 0);
	}

	if(!(pid = fork())) {
		execlp("gnome-open", "gnome-open", oname, NULL);
	} else {
		waitpid(pid, NULL, 0);
	}
}
