import numpy as np
from scipy.fftpack import fft
import matplotlib.pyplot as plot

fft_len = 40000

def showplots(v, reference=None, logfft=False, marker=False, grid=False):

	if v['array'].shape[0] > fft_len:
		raise Exception('Signal too large')

	if reference is not None and reference['array'].shape[0] > fft_len:
		raise Exception('Reference signal too large')

	v_fft = np.abs(fft(v['array'], n=fft_len))
	if logfft:
		v_fft = np.log10(v_fft)

	if reference is not None:
		r_fft = np.abs(fft(reference['array'], n=fft_len))
		if logfft:
			r_fft = np.log10(r_fft)

	freqs = np.arange(-1, 1, 2.0 / fft_len)
	
	mark = None
	if marker:
		mark = 11

	plot.figure()

	vplot = plot.subplot(211)

	vplot.plot(np.arange(0, v['array'].shape[0], 1), v['array'], label=v['name'], marker=mark)
	vplot.set_xlabel("N")
	vplot.set_ylabel("Value")
	vplot.legend()

	if grid:
		vplot.minorticks_on()
		vplot.grid(b=True, which='major', color='b', linestyle='--')
		vplot.grid(b=True, which='minor', color='r', linestyle='--')
		vplot.set_axisbelow(False)

	fplot = plot.subplot(212)

	fplot.plot(freqs, v_fft, label=str(fft_len) + '-point FFT(' + v['name'] + ')')
	if reference is not None:
		fplot.plot(freqs, r_fft, label=str(fft_len) + '-point FFT(' + reference['name'] + ')')

	fplot.set_xlabel("Normalized frequency")
	if logfft:
		fplot.set_ylabel("log10(abs(value))")
	else:
		fplot.set_ylabel("abs(value)")
	fplot.legend()

	fplot.minorticks_on()
	fplot.grid(b=True, which='major', color='b', linestyle='--')
	fplot.grid(b=True, which='minor', color='r', linestyle='--')
	fplot.set_axisbelow(False)


	plot.show()


time = np.arange(0, 40, 0.1)
values = np.sin(time) + 0.5 * np.sin(2 * time) + 0.25 * np.sin(4 * time) + 0.33 * np.sin(8 * time)

interl = np.zeros(values.shape[0] * 2, dtype=values.dtype)
interl[0::2] = values

showplots({'array': values, 'name': 'base'}, reference={'array' : interl, 'name' : 'upsampled x 2'}, logfft=False, marker=True, grid=False)

