import numpy as np
from matplotlib import pyplot as plt

def FtoMel(f):
    return 2595 * np.log10(1+(f/700))

fs = 48000
fftLen = 512
nSegments = 16
shown_maxF = 15e3       # only show up to 10kHz, anything above that is meh


fs /= 2
fftLen //= 2

freqIn = np.linspace(0, fs, fftLen)

melScale = np.linspace(0, FtoMel(shown_maxF), nSegments+2)
melScaleF = 700*(10**(melScale/2595)-1)

print(freqIn)
print(melScale)
print(melScaleF)

# select nearst bins
melScaleBinCenter = np.floor( (fftLen+1)*melScaleF[:-1] / fs)
print(melScaleBinCenter)

melScaleTriangle = np.zeros((nSegments, fftLen))
for i, center in enumerate(melScaleBinCenter):
    if i == 0:
        continue
    center = int(center)
    # http://practicalcryptography.com/miscellaneous/machine-learning/guide-mel-frequency-cepstral-coefficients-mfccs/
    for k in range(fftLen):
        fq = freqIn[k]
        # print(i, center, k, fq)
        # if i != 0:


        if melScaleF[i - 1] < fq and melScaleF[i] > fq:
            melScaleTriangle[i-1][k] = (fq - melScaleF[i-1]) / (melScaleF[i] - melScaleF[i-1])

        if melScaleF[i] < fq and melScaleF[i + 1] > fq:
            melScaleTriangle[i-1][k] = (melScaleF[i+1]-fq) / (melScaleF[i+1] - melScaleF[i])

    plt.plot(freqIn, melScaleTriangle[i-1])
# for f in freqIn:
    # plt.axvline(f, color='black')
for f in melScaleF:
    plt.axvline(f, color='red')
# plt.show()

to_print = f"const q15_t melScale[{nSegments}][{fftLen}] = {{\n"
for m in melScaleTriangle:
    to_print += f"{{ {','.join(['{:d}'.format(int(i*2**15)) for i in m])} }},\n"
to_print = to_print[:-1]
to_print += "};"
print(to_print + '\n\n')

to_print = f"const uint16_t melScaleStartIDx[{nSegments}] = {{"
for m in melScaleTriangle:
    for idx, m_v in enumerate(m):
        if m_v != 0:
            to_print += f'{idx},'
            break

to_print = to_print[:-1]
to_print += "};"
print(to_print)
