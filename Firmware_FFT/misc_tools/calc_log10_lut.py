import numpy as np
from matplotlib import pyplot as plt

x = np.arange(0, 256, 1)

scale = (np.log10((x/20)+1)/ np.log10(256/20))
# scale = (  (10**(x/100)-1) / (10**(256/100)-1) )
scale *= (2**4)

print(x)
print(scale)

plt.plot(x, scale)
plt.show()

to_print = f"const uint16_t linToLog[256] = {{"
for m in scale:
    to_print += f'{int(m)},'

to_print = to_print[:-1]
to_print += "};"
print(to_print)
