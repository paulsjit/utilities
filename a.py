import sys
import numpy as np
from scipy.fftpack import fft
from scipy.signal import firwin
import matplotlib.pyplot as plot

fft_len = 40000

def showplots(v, references=None, logfft=False, marker=False, grid=False):
	if v['array'].shape[0] > fft_len:
		raise Exception('Signal too large')

        if references is not None and all (x for x in map(lambda x: x['array'].shape[0] > fft_len, references)):
		raise Exception('Reference signal too large')

	v_fft = np.abs(fft(v['array'], n=fft_len))
	if logfft:
		v_fft = np.log10(v_fft)

        r_ffts = []
        r_times = []
	if references is not None:
                for r in references:
                        r_fft = np.abs(fft(r['array'], n=fft_len))
		        if logfft:
			        r_fft = np.log10(r_fft)

                        r_ffts.append(r_fft)

                        if not r['align']:
                                r_times.append(np.arange(0, r['array'].shape[0], 1))
                        else:
                                r_times.append(np.arange(0, v['array'].shape[0], v['array'].shape[0] / float(r['array'].shape[0])))

	freqs = np.arange(-1, 1, 2.0 / fft_len)
	
	mark = None
	if marker:
		mark = 11

	plot.figure()

	vplot = plot.subplot(211)

	vplot.plot(np.arange(0, v['array'].shape[0], 1), v['array'], label=v['name'], marker=mark)
        if references is not None:
                for idx, r in zip(range(0, len(references)), references):
                        vplot.plot(r_times[idx], r['array'], label=r['name'], marker=mark)
	vplot.set_xlabel("N")
	vplot.set_ylabel("Value")
	vplot.legend()
        vplot.axhline(color='black', linestyle='--', linewidth=0.5)

	if grid:
		vplot.minorticks_on()
		vplot.grid(b=True, which='major', color='b', linestyle='--')
		vplot.grid(b=True, which='minor', color='r', linestyle='--')
		vplot.set_axisbelow(False)

	fplot = plot.subplot(212)

	fplot.plot(freqs, v_fft, label=str(fft_len) + '-point FFT(' + v['name'] + ')')
	if references is not None:
                for idx, r, r_fft in zip(range(0, len(r_ffts)), references, r_ffts):
		        fplot.plot(freqs, r_fft, label=str(fft_len) + '-point FFT(' + r['name'] + ')')

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

#kalman = firwin(32, 0.25, width=0.005, window='blackman')
#kalman *= 4 

#gran = 0.5

#interp_filter = np.arange(0, 1, gran);
#interp_filter = np.concatenate((interp_filter, np.arange(1, 0, -gran)))
#interp_filter = np.concatenate((interp_filter, np.zeros(32 - len(interp_filter))))
#interp_filter *= 2


#low = np.arange(-4 * np.pi, 0, 4 * np.pi / 16)
#sinc = np.sin(low)/low
#sinc_filter = sinc
#sinc_filter = np.concatenate((sinc_filter, np.full(1, 1)))
#sinc_filter = np.concatenate((sinc_filter, np.flip(sinc)))
#sinc_filter = sinc_filter[0:len(sinc_filter) - 1]

#x = np.arange(-3, 3, 6 / 33.0)
#gauss = np.exp(-x * x)
#gauss = gauss[0:len(gauss) - 1]

#Const = 1.0
#beta = 1
#taps = 16
#t = np.arange((1 - beta) / Const, (1 + beta) / Const + (((2 * beta) / Const) / (taps - 1) / 2), ((2 * beta) / Const) / (taps - 1))
#rcosine_l = 0.5 * (1 + np.cos((np.pi * (Const / 2) / beta) * (t - ((1 - beta) / Const))))
#rcosine = np.concatenate((np.flip(rcosine_l), np.full(1, 1), rcosine_l))
#rcosine = rcosine[0:len(rcosine) - 1]

#rcosine_filter = np.copy(sinc_filter)
#rcosine_filter *= rcosine

#gauss_filter = np.copy(sinc_filter)
#gauss_filter *= gauss

#showplots({'array': kalman, 'name': 'kalman'}, references=[
#    {'array':interp_filter, 'name':'interpolation', 'align':True},
#    {'array':sinc_filter, 'name':'sinc', 'align':True},
#    {'array':gauss_filter, 'name':'gauss', 'align':True},
#    {'array':rcosine_filter, 'name':'rcosine', 'align':True}
#    ], logfft=False, marker=True, grid=False)

#time = np.arange(0, 40, 0.1)
#values = np.sin(time) + 0.5 * np.sin(2 * time) + 0.25 * np.sin(4 * time) + 0.33 * np.sin(8 * time)

numtaps = 128 
expansion = 16
decimation = 11 
cutoff_ex = 1/float(expansion)

low_ex = np.arange(-(numtaps / (2.0 * expansion)) * np.pi, 0, (numtaps / (2.0 * expansion)) * np.pi / (numtaps / 2.0))
sinc_ex = np.sin(low_ex)/low_ex
sinc_ex_filter = sinc_ex
sinc_ex_filter = np.concatenate((sinc_ex_filter, np.full(1, 1)))
sinc_ex_filter = np.concatenate((sinc_ex_filter, np.flip(sinc_ex)))
sinc_ex_filter = sinc_ex_filter[0:len(sinc_ex_filter) - 1]

showplots({'array': sinc_ex_filter, 'name': 'cutoff: ' + str(cutoff_ex)}, logfft=False, marker=False, grid=False)

seed = np.asarray([0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1, 0.875, 0.75, 0.625, 0.5, 0.375, 0.25, 0.125])
values = seed
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, np.zeros(36)))
values += 0.25 * np.sin(np.arange(0, 1000, 10))

interl = np.zeros(values.shape[0] * expansion, dtype=values.dtype)
interl[0::expansion] = values

showplots({'array': values, 'name': 'base'}, references=[{'array' : interl, 'name' : 'upsampled x ' + str(expansion), 'align' : True}], logfft=False, marker=True, grid=False)



upsl = np.convolve(sinc_ex_filter, interl)
showplots({'array': upsl, 'name': 'upsampled-filtered'}, references=[
    {'array' : interl, 'name' : 'upsampled', 'align' : False},
    {'array' : values, 'name' : 'base', 'align' : False}], logfft=False, marker=False, grid=True)

cutoff_dec = 1/float(decimation)

low_dec = np.arange(-(numtaps / (2.0 * decimation)) * np.pi, 0, (numtaps / (2.0 * decimation)) * np.pi / (numtaps / 2.0))
sinc_dec = np.sin(low_dec)/low_dec
sinc_dec_filter = sinc_dec
sinc_dec_filter = np.concatenate((sinc_dec_filter, np.full(1, 1)))
sinc_dec_filter = np.concatenate((sinc_dec_filter, np.flip(sinc_dec)))
sinc_dec_filter = sinc_dec_filter[0:len(sinc_dec_filter) - 1]

showplots({'array': sinc_dec_filter, 'name': 'cutoff: ' + str(cutoff_dec)}, logfft=False, marker=False, grid=False)

out = np.convolve(values, sinc_dec_filter)
showplots({'array':out, 'name':'anti-aliased'}, references=[{'array':values, 'name':'orig', 'align':False}])

subl = out[0::decimation]
showplots({'array': values, 'name': 'base'}, references=[{'array' : subl, 'name' : 'subsampled x ' + str(decimation), 'align' : False}], logfft=False, marker=True, grid=False)

sinc_f = np.convolve(sinc_dec_filter, sinc_ex_filter)

showplots({'array': sinc_f, 'name': 'scale_filter'}, logfft=False, marker=True, grid=True)

part_interl = np.zeros(values.shape[0] * expansion, dtype=values.dtype)
part_interl[0::expansion] = values

part_f = np.convolve(part_interl, sinc_f)

final = part_f[0::decimation]

showplots({'array': final, 'name': 'scaled'}, logfft=False, marker=False, grid=False)
