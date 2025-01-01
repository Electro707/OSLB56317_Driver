import serial
from matplotlib import pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
import struct

nFFT = 512

ser = serial.Serial('/dev/ttyACM0', 115200, timeout=3)


fig, ax = plt.subplots()

x = np.linspace(0, 48000, nFFT)

line1, = ax.plot(x, np.zeros(nFFT), 'r')
ax.set_xlim(-100, 48100)

def update(frame):
    ser.flushInput()
    d = ser.read(nFFT*2)
    d = struct.unpack('<512h', d)

    y = d
    line1.set_data(x, y)

    ax.set_ylim(min(y)*1.1, max(y)*1.1)

    return line1,

ani = FuncAnimation(
    fig, update,
    frames=100, interval=10,
    blit=True)

plt.show()
