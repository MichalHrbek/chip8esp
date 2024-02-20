# chip8esp
A [CHIP-8](https://en.wikipedia.org/wiki/CHIP-8) interpreter for the ESP32-S3 using the Arduino framework.

## Roms
Upload your roms to the root folder of the SPI flash.

## Performance
Runs well on the ESP32-S3 both single-core and dual-core. I haven't tested any other boards.

## Customization
You can set the maximum instuctions per second with `target_frequency`.  
If you have a different screen you will have to change the display code in `main.cpp`  
You can set it to use one core for io and the other for emulation with `THREADED`  

## Demo
It runs smoothly the gif is just low quality  
![Chip8 breakout running on the ESP32-S3](https://michalhrbek.github.io/images/chip8/esp-demo.gif)