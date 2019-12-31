#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <getopt.h>

#if 1
int main(int argc, char **argv)
{
	int width = -1, height = -1;
	int fwidth, fheight, ifact;
	int angle = 0;
	float rad;
	float fw, fh, fact;
	int r, c;

	while((c = getopt(argc, argv, "w:h:a:")) != -1) {
		switch(c) {
			case 'w':
				width = atoi(optarg);
				break;
			case 'h':
				height = atoi(optarg);
				break;
			case 'a':
				angle = atoi(optarg);
				break;
		}
	}

	if(width <= 0 || height <= 0) {
		printf("GKHDGAFLJKSG:KLrh\n");
		exit(0);
	}

	rad = (angle / 180.0) * M_PI;
	fw = width * cosf(rad) + height * sinf(rad);
	fh = width * sinf(rad) + height * cosf(rad);
	fwidth  = (int)(ceilf(fw));
	fheight = (int)(ceilf(fh));

	goto jhs;
	printf("\n");
	printf("======= INPUT IMAGE =======\n");
	for(r = 0; r < height; r++) {
		for(c = 0; c < width; c++) {
			printf("%04x ", (r + 1) << 8 | (c + 1));
		}
		printf("\n");
	}

jhs:
	fact = -cos(rad) * height / 2 + sin(rad) * width / 2;

	printf("\n");
	printf("Factor = %f (%d)\n", fact, ifact);
	printf("RES IMAGE : (%f)%d x (%f)%d\n", fw, fwidth, fh, fheight);

	printf("\n");
	printf("======= OUTPUT IMAGE =======\n");

	int i = 0;
	for(r = -(float)fheight / 2 - fact - 1; r < -(float)fheight / 2 - fact + fheight + 1; r++) {
		//printf("[% 03d => % 03d]: ", i++, r);
		for(c = 0; c < fwidth; c++) {
			int fx = (int)round(c * cos(rad) - r * sin(rad));
			int fy = (int)round(c * sin(rad) + r * cos(rad));
			//if(fx < 0 || fx >= width || fy < 0 || fy >= height)
			//	printf("----- ");
			//else
				//printf("%04x ", (fy + 1) << 8 | (fx + 1)); 
				//printf("|% 4d,% 4d ", fx, fy); 
		}
		//printf("|\n");
	}

	for(c = 0; c < fwidth; c+=8) {
		for(r = -(float)fheight / 2 - fact - 1; r < -(float)fheight / 2 - fact + fheight + 1; r+=8) {
			int tlx_l = (int)floor(c * cos(rad) - r * sin(rad));
			int tly_l = (int)floor(c * sin(rad) + r * cos(rad));
			int blx_l = (int)floor(c * cos(rad) - (r + 7) * sin(rad));
			int bly_l = (int)floor(c * sin(rad) + (r + 7) * cos(rad));
			int trx_l = (int)floor((c + 7) * cos(rad) - r * sin(rad));
			int try_l = (int)floor((c + 7) * sin(rad) + r * cos(rad));
			int brx_l = (int)floor((c + 7) * cos(rad) - (r + 7) * sin(rad));
			int bry_l = (int)floor((c + 7) * sin(rad) + (r + 7) * cos(rad));

			int tlx_h = (int)ceil(c * cos(rad) - r * sin(rad));
			int tly_h = (int)ceil(c * sin(rad) + r * cos(rad));
			int blx_h = (int)ceil(c * cos(rad) - (r + 7) * sin(rad));
			int bly_h = (int)ceil(c * sin(rad) + (r + 7) * cos(rad));
			int trx_h = (int)ceil((c + 7) * cos(rad) - r * sin(rad));
			int try_h = (int)ceil((c + 7) * sin(rad) + r * cos(rad));
			int brx_h = (int)ceil((c + 7) * cos(rad) - (r + 7) * sin(rad));
			int bry_h = (int)ceil((c + 7) * sin(rad) + (r + 7) * cos(rad));

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

			int max_x_l = max(max(max(tlx_l, blx_l), trx_l), brx_l);
			int max_y_l = max(max(max(tly_l, bly_l), try_l), bry_l);
			int max_x_h = max(max(max(tlx_h, blx_h), trx_h), brx_h);
			int max_y_h = max(max(max(tly_h, bly_h), try_h), bry_h);
			int min_x_l = min(min(min(tlx_l, blx_l), trx_l), brx_l);
			int min_y_l = min(min(min(tly_l, bly_l), try_l), bry_l);
			int min_x_h = min(min(min(tlx_h, blx_h), trx_h), brx_h);
			int min_y_h = min(min(min(tly_h, bly_h), try_h), bry_h);
			//if(fx < 0 || fx >= width || fy < 0 || fy >= height)
			//	printf("----- ");
			//else
				//printf("%04x ", (fy + 1) << 8 | (fx + 1)); 
			printf("Block [%03d:%03d x %03d:%03d] : x % 4d:% 4d y % 4d:% 4d\n", c, c+7, r, r+7, min(min_x_l, min_x_h), max(max_x_l, max_x_h),
					min(min_y_l, min_y_h), max(max_y_l, max_y_h)); 
		}
	}
}

#else

int main(int argc, char **argv)
{
	int angle = atoi(argv[1]);
	float rad = (angle / 180.0) * M_PI;
	int width = -1, height = -1;
	int s1width = -1, s1height = -1;
	int s2width = -1, s2height = -1;
	int owidth = -1, oheight = -1;
	uint8_t *ibuf, *s1buf, *s2buf, *obuf;
	int isize, s1size, s2size, osize;
	int ifd, s1fd, s2fd, ofd;
	int plane, x, y, i;

	width = 640;
	height = 480;

	s1width = width + height * tanf(rad / 2);
	s1height = height;
	printf("s1 = %d %d\n", s1width, s1height);

	s2width = s1width;
	s2height = s1height + s1width * sinf(rad);
	printf("s2 = %d %d\n", s2width, s2height);

	owidth = s2width + s2height * tanf(rad / 2);
	oheight = s2height;
	printf("o = %d %d\n", owidth, oheight);

	isize = width * height * 3;
	s1size = s1width * s1height * 3;
	s2size = s2width * s2height * 3;
	osize = owidth * oheight * 3;

	posix_memalign((void **)&ibuf, 32, isize);
	posix_memalign((void **)&s1buf, 32, s1size);
	posix_memalign((void **)&s2buf, 32, s2size);
	posix_memalign((void **)&obuf, 32, osize);

	srand(time(0));
	uint8_t __y, __u, __v;

	__y = rand() % 256;
	__u = rand() % 256;
	__v = rand() % 256;
	for(i = 0; i < s1size / 3; i++)
		s1buf[i] = __y;
	for(; i < 2 * s1size / 3; i++)
		s1buf[i] = __u;
	for(; i < s1size; i++)
		s1buf[i] = __v;

	__y = rand() % 256;
	__u = rand() % 256;
	__v = rand() % 256;
	for(i = 0; i < s2size / 3; i++)
		s2buf[i] = __y;
	for(; i < 2 * s2size / 3; i++)
		s2buf[i] = __u;
	for(; i < s2size; i++)
		s2buf[i] = __v;

	__y = rand() % 256;
	__u = rand() % 256;
	__v = rand() % 256;
	for(i = 0; i < osize / 3; i++)
		obuf[i] = __y;
	for(; i < 2 * osize / 3; i++)
		obuf[i] = __u;
	for(; i < osize; i++)
		obuf[i] = __v;

	ifd = open("image.raw", O_RDONLY);
	s1fd = open("image.raw-s1", O_WRONLY | O_TRUNC | O_CREAT, 0664);
	s2fd = open("image.raw-s2", O_WRONLY | O_TRUNC | O_CREAT, 0664);
	ofd = open("image.raw-out", O_WRONLY | O_TRUNC | O_CREAT, 0664);

	read(ifd, ibuf, isize);
	close(ifd);
	
	plane = 0;
	do {
		for(y = 0; y < height; y++) {
			for(x = 0; x < width; x++) {
				int yd = y;
				int xd = tanf(rad / 2) * height + x + (y - height - 1) * tanf(rad / 2);

				if(xd < 0 || xd >= s1width || yd < 0 || yd >= s1height)
					continue;

				(s1buf + plane * s1width * s1height)[yd * s1width + xd] = (ibuf + plane * width * height)[y * width + x];
			}
		}
		plane++;
	} while(plane < 3);
	write(s1fd, s1buf, s1size);
	close(s1fd);

	plane = 0;
	do {
		float yl = 0;
		float yorig = 0;
		float ratio = height / (height * tanf(rad / 2));
		for(x = 0; x < s1width; x++) {
			if(yl < height)
				yl += ratio;
			if(x >= width) {
				yorig += ratio;
				yl -= ratio;
			}

			for(y = (int)yorig + 2; y < (int)yl - 1; y++) {
				int xd = x;
				int yd = y + (width - x - 1) * sinf(rad) - 1;

				//printf("x, y = %d %d; xd, yd = %d, %d\n", x, y, xd, yd);

				if(xd < 0 || xd >= s2width || yd < 0 || yd >= s2height)
					continue;

				(s2buf + plane * s2width * s2height)[yd * s2width + xd] = (s1buf + plane * s1width * s1height)[y * s1width + x];
			}
		}
		plane++;
	} while(plane < 3);
	write(s2fd, s2buf, s2size);
	close(s2fd);

	plane = 0;
	do {
		for(y = 0; y < s2height; y++) {
			for(x = 0; x < s2width; x++) {
				int yd = y;
				int xd = tanf(rad / 2) * s2height + x + (y - s2height - 1) * tanf(rad / 2);

				if(xd < 0 || xd >= owidth || yd < 0 || yd >= oheight)
					continue;

				(obuf + plane * owidth * oheight)[yd * owidth + xd] = (s2buf + plane * s2width * s2height)[y * s2width + x];
			}
		}
		plane++;
	} while(plane < 3);
	write(ofd, obuf, osize);
	close(ofd);

}
#endif
