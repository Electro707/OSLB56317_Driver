# OSLB56317-XX LED Driver

This project is to drive the OSLB56317 16x16 LED matrix I got from my trip to Japan (from [Akizuki](https://maps.app.goo.gl/3vW3UY29P5pvr1BS6))

![Image](.misc/img1.jpeg)

# Known Issues
- I goofed up on the RP2040's SPI wiring to the constant current led driver, so for now it is emulated in software
    - todo: make this a PIO block, or even a PIO block to drive the entire display?
- In the CAD file, there is no easy way to take it apart other than pushing a small flat head between the display and the case. With PLA, the case is maluable enough to bend
    - todo: add some small slot eventually to be able to easily remove the display assembly

# todo, maybe
- Develop a PIO block for the LED matrix
- Create more fun demo codes (animation?)
- Create a USB audio spectrum analyzer

# License
This project is licensed under GPLv3. See [LICENSE.md](LICENSE.md) for details.

