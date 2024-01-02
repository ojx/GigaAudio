# Arduino GigaAudio library

This is a simple Arduino library used for detecting and playing back WAV audio files on the Arduino GIGA. The file should be placed in the root of an attached USB drive.

By Default, the analog output signal will be to the 3.5mm jack and pin __dac0__.

Do not use the Arduino's built in `delay` function as this library requires the use of non-blocking function.