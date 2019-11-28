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

                if not reference['align']:
                        r_time = np.arange(0, reference['array'].shape[0], 1)
                else:
                        r_time = np.arange(0, v['array'].shape[0], v['array'].shape[0] / float(reference['array'].shape[0]))

	freqs = np.arange(-1, 1, 2.0 / fft_len)
	
	mark = None
	if marker:
		mark = 11

	plot.figure()

	vplot = plot.subplot(211)

	vplot.plot(np.arange(0, v['array'].shape[0], 1), v['array'], label=v['name'], marker=mark)
        if reference is not None:
                vplot.plot(r_time, reference['array'], label=reference['name'], marker=mark)
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


#time = np.arange(0, 40, 0.1)
#values = np.sin(time) + 0.5 * np.sin(2 * time) + 0.25 * np.sin(4 * time) + 0.33 * np.sin(8 * time)

seed = np.asarray([0, 0.5, 1, 0.5])
values = seed
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
#values = np.concatenate((values, np.zeros(5)))

interl = np.zeros(values.shape[0] * 2, dtype=values.dtype)
interl[0::2] = values

subl = values[0::2]

showplots({'array': values, 'name': 'base'}, reference={'array' : interl, 'name' : 'upsampled x 2', 'align' : True}, logfft=False, marker=True, grid=False)
showplots({'array': values, 'name': 'base'}, reference={'array' : subl, 'name' : 'subsampled x 2', 'align' : True}, logfft=False, marker=True, grid=False)

#gran = 0.5

#interp_filter = np.arange(0, 1, gran);
#interp_filter = np.concatenate((interp_filter, np.arange(1, 0, -gran)))

low = np.arange(-8 * np.pi, 0, 8 * np.pi / 16)
sinc = np.sin(low)/low
interp_filter = sinc
interp_filter = np.concatenate((interp_filter, np.full(1, 1)))
interp_filter = np.concatenate((interp_filter, np.flip(sinc)))
interp_filter = interp_filter[0:len(interp_filter) - 1]

showplots({'array': interp_filter, 'name': 'interpolation filter'}, logfft=True, marker=True, grid=False)

upsl = np.convolve(interp_filter, interl)
showplots({'array': upsl, 'name': 'upsampled'}, logfft=False, marker=False, grid=False)
