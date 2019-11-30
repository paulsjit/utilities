import sys
import numpy as np
from scipy.fftpack import fft
from scipy.signal import firwin
import matplotlib.pyplot as plot
import math

fft_len = 40000

def showplots(v, references=None, logfft=False, marker=False, grid=False, norm=False, fftnorm=True):
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

	vplot.plot(np.arange(0, v['array'].shape[0], 1), v['array'] if not norm else v['array'] / np.amax(v['array']), label=v['name'], marker=mark)
        if references is not None:
                for idx, r in zip(range(0, len(references)), references):
                        vplot.plot(r_times[idx], r['array'] if not norm else r['array'] / np.amax(r['array']), label=r['name'], marker=mark)
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

	fplot.plot(freqs, v_fft if not fftnorm else v_fft / np.amax(v_fft), label=str(fft_len) + '-point FFT(' + v['name'] + ')')
	if references is not None:
                for idx, r, r_fft in zip(range(0, len(r_ffts)), references, r_ffts):
		        fplot.plot(freqs, r_fft if not fftnorm else r_fft / np.amax(r_fft), label=str(fft_len) + '-point FFT(' + r['name'] + ')')

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


def sinc_filter(numtaps, factor):
    low = np.arange(-(numtaps / (2.0 * factor)) * np.pi, 0, (numtaps / (2.0 * factor)) * np.pi / (numtaps / 2.0))
    sinc = np.sin(low)/low
    sinc_filter = sinc
    sinc_filter = np.concatenate((sinc_filter, np.full(1, 1)))
    sinc_filter = np.concatenate((sinc_filter, np.flip(sinc)))
    sinc_filter = sinc_filter[0:len(sinc_filter) - 1]

    return sinc_filter

def gcd(x, y):
   while(y):
       x, y = y, x % y
   return x

numtaps = 256
w = 100
sw = 400
factor = gcd(w, sw)
expansion = sw / factor
decimation = w / factor

print expansion
print decimation

seed = np.asarray([0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1, 0.875, 0.75, 0.625, 0.5, 0.375, 0.25, 0.125])
values = seed
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, np.zeros(36)))
values = np.concatenate((values, seed))
values = np.concatenate((values, seed))
values = np.concatenate((values, np.zeros(18)))
values += 0.25 * np.sin(np.arange(0, 1500, 10))

sinc_f = sinc_filter(numtaps, max(expansion, decimation))

#showplots({'array': sinc_f, 'name': 'filter'}, logfft=False, marker=True, grid=False)

part_interl = np.zeros(values.shape[0] * expansion, dtype=values.dtype)
part_interl[0::expansion] = values

part_f = np.convolve(part_interl, sinc_f, mode='same')

print sinc_f.shape
print part_f.shape
print part_interl.shape

final = part_f[0::decimation]
print final.shape

#showplots({'array': values, 'name': 'base'}, references=[{'array': final, 'name':'final', 'align':True}], logfft=False, marker=True, grid=False)

y = sinc_f
x = np.zeros(part_interl.shape[0])
for idx in range(0, len(x)):
    lidx = idx - len(y) / 2
    if lidx < 0:
        e = np.concatenate((np.zeros(-lidx), part_interl[0:lidx + len(y)]))
    elif lidx + len(y) > len(x):
        e = np.concatenate((part_interl[lidx:len(x)], np.zeros(len(y) - (len(x) - lidx))))
    else:
        e = part_interl[lidx:lidx + len(y)]

    x[idx] = np.sum(np.flip(y) * e)

#showplots({'array': x, 'name': 'crude'}, references=[{'array': part_f, 'align':False, 'name':'filtered'}], logfft=False, marker=True, grid=False)

xfact = 2 
sinc_f2 = sinc_filter(xfact * 5, 8)
showplots({'array': sinc_f2, 'name': 'filter'}, logfft=False, marker=True, grid=False)
fbanks = []

for idx in range(0, xfact):
    fbanks.append(np.asarray(sinc_f2[idx::xfact]))


f2 = np.zeros(len(values) * xfact)
for idx in range(0, xfact):
    f2[idx::xfact] = np.convolve(values, fbanks[idx], mode='same')

fd = np.zeros(values.shape[0] * xfact, dtype=values.dtype)
fd[0::xfact] = values 
fd = np.convolve(fd, sinc_f2, mode='same')

showplots({'array': fd[0:len(fd)], 'name': 'crude'}, references=[{'array': f2[xfact - 1:len(f2)], 'align':False, 'name':'banked'}], logfft=False, marker=True, grid=False)

