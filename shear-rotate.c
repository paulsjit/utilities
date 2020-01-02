#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <getopt.h>

int method = 2; // 0 = NEAREST NEIGHBOUR, 1 = ROUND COORD, 2 = BILINEAR

int main(int argc, char **argv)
{
	int angle = atoi(argv[1]);
	float rad = (angle / 180.0) * M_PI;
	int width = -1, height = -1;
	int owidth = -1, oheight = -1;
	uint8_t *ibuf, *obuf;
	int isize, osize;
	int ifd, ofd;
	int plane, r, c, i;
	float fact;

	width = 640;
	height = 480;

	fact = -cos(rad) * (float)height / 2 + sin(rad) * (float)width / 2;

	owidth = ceil(width * cosf(rad) + height * sinf(rad));
	oheight = 1 + ceil(width * sinf(rad) + height * cosf(rad)) + 1;
	printf("o = %d %d\n", owidth, oheight);

	isize = width * height * 3;
	osize = owidth * oheight * 3;

	posix_memalign((void **)&ibuf, 32, isize);
	posix_memalign((void **)&obuf, 32, osize);

	srand(time(0));
	uint8_t __y, __u, __v;

	__y = rand() % 256;
	__u = rand() % 256;
	__v = rand() % 256;

	ifd = open("image.raw", O_RDONLY);
	ofd = open("image.raw-out", O_WRONLY | O_TRUNC | O_CREAT, 0664);

	read(ifd, ibuf, isize);
	close(ifd);
	
	plane = 0;
	do {
		for(r = 0; r < oheight; r++) {
			int _r = (r - fact + 1) - (float)oheight / 2;
			//int _r = -2 * (r + 1 + fact);  
			for(c = 0; c < owidth; c++) {
				float fx = c * cos(rad) - _r * sin(rad);
				float fy = c * sin(rad) + _r * cos(rad);

				int ifx, ify;
				uint8_t comp;

				if(method == 0) {
					float xl = floor(fx);
					float xr = xl + 1; 
					float yt = floor(fy);
					float yb = yt + 1;

					if((fx - xl) > (xr - fx))
						ifx = xr;
					else
						ifx = xl;

					if((fy - yt) > (yb - fy))
						ify = yb;
					else
						ify = yt;
				} else if(method == 1) {
					ifx = round(fx);
					ify = round(fy);
				} else if(method == 2) {
					float xl = floor(fx);
					float xr = xl + 1;
					float yt = floor(fy);
					float yb = yt + 1;

#define get_comp(x, y, p) ((((x) >= 0) && ((y) >= 0) && ((x) < width) && ((y) < height)) ? ((ibuf + plane * width * height)[(y) * width + (x)]) : (((p) == 0) ? __y : (((p) == 1) ? __u : __v)))
					uint8_t ctl = get_comp((int)xl, (int)yt, plane);
					uint8_t ctr = get_comp((int)xr, (int)yt, plane);
					uint8_t cbl = get_comp((int)xl, (int)yb, plane);
					uint8_t cbr = get_comp((int)xr, (int)yb, plane);

					float ftl = (1 - (fx - xl)) * (1 - (fy - yt));
					float ftr = (1 - (xr - fx)) * (1 - (fy - yt));
					float fbl = (1 - (fx - xl)) * (1 - (yb - fy));
					float fbr = (1 - (xr - fx)) * (1 - (yb - fy));

					comp = ftl * ctl + ftr * ctr + fbl * cbl + fbr * cbr;
				}



				if(method == 0 || method == 1) {
					if(ifx < 0 || ifx >= width || ify < 0 || ify >= height) {
						switch(plane) {
							case 0: comp = __y; break;
							case 1: comp = __u; break;
							case 2: comp = __v; break;
						}
					} else {
						comp = (ibuf + plane * width * height)[ify * width + ifx];
					}
				}

				(obuf + plane * owidth * oheight)[r * owidth + c] = comp;
			}
		}
		plane++;
	} while(plane < 3);

	write(ofd, obuf, osize);
	close(ofd);


}
