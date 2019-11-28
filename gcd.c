#include  <stdint.h>

int main(int argc, char **argv)
{
	uint32_t a, b, res;

	if(argc < 3)
		return 0;

	a = atoi(argv[1]);
	b = atoi(argv[2]);

	if(!a || !b)
		return 0;

	if(b > a) {
		res = b;
		b = a;
		a = res;
	}

	printf("GCD(%u, %u) = ", a, b);

	while(b) {
		res = b;
		b = a % b;
		a = res;
	}

	printf("%u\n", res);
	return 0;
}
