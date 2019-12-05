#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

double sinc(double x)
{
	if (x == 0)
		return 1;

	return sin(x) / x;
}

double func(double phase, int index)
{
	double intr = M_PI;
	double start = phase * M_PI;
	double pos = start + (index * intr);
	double val = sinc(pos);
	return val;
}

double **create_filter_bank(char *name, int bank_numtaps, int sf, int num_phases, int max_phases)
{
	assert(num_phases % sf == 0);

	double **banks = calloc(max_phases, sizeof(double *));

	for(int p = num_phases; p < max_phases; p++) {
		banks[p] = calloc(bank_numtaps, sizeof(double));

		for(int k = 0; k < bank_numtaps; k++)
			banks[p][k] = 0;		
	}

	for(int p = 0; p < num_phases; p++) {
		double *bank;

		banks[p] = calloc(bank_numtaps, sizeof(double));
		bank = banks[p];

		for(int k = 0; k < bank_numtaps; k++) {
			int idx = k - bank_numtaps / 2;
			if(!(bank_numtaps % 2))
				idx++;

			bank[k] = func(p / (double)num_phases, idx);
		}
	}

	return banks;
}

int main(int argc, char **argv)
{
	/*int iw = atoi(argv[1]);
	int ow = atoi(argv[2]);

	if(ow > 128) {
		iw = (int)(128 * (iw / (double)ow));
		ow = 128;
	}

	printf("iw = %d, ow = %d\n", iw, ow);

	for(int i = 0; i < ow; i++) {
		int ipixel = (int)(i * (iw / (double)ow));
		double phase_diff = i - ipixel * (ow / (double)iw);
		printf("op = % 3d, ip = % 3d, bank = % 1.6lf (% 1.6lf), bank(rounded) = % 3d\n", i, ipixel, phase_diff, phase_diff * 16, (int)round(phase_diff * 16));
	}*/


	int sf = 4;
	int phase_gran = 1;
	int num_phases = sf * phase_gran;
	int max_phases = 16;
	int num_banks = 5;
	double **b = create_filter_bank("", num_banks, sf, num_phases, max_phases);

	for(int i = 0; i < max_phases; i++) {
		printf("bank[%02d] = [", i);
		for(int j = 0; j < num_banks; j++)
			printf("% 1.6lf%s", b[i][j], j < num_banks -1 ? " " : "]");
		printf("\n");
	}
}

