#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPIFFS.h>
#include <vector>
#include "chip8.h"

#define SCREEN_WIDTH	128
#define SCREEN_HEIGHT	64
#define PIXEL_SCALE		2
// How many lines of text can fit on the screen
#define MAX_LINES		8

#define SPEAKER_PIN		13

// How often should io (drawing to the screen, polling keys, playing sound) be handled
#define IO_FREQ 60

// How many instructions per second should be attempted
#define TARGET_FREQ 500

// Uncomment if you want to use two cores
// #define THREADED

#ifdef THREADED
TaskHandle_t ioTask;
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

uint8_t rom[4096];
size_t rom_size = 0;
Chip8 *vm;

long slowdown = 0;
ulong last_tick = 0;

const int nRows = 4;
const int nColumns = 4;
int rows[] = {15,16,17,18}; // Row pins
int columns[] = {7,6,5,4}; // Column pins
bool keys[nRows*nColumns];

void pollKeys(bool* keys) {
	for (size_t i = 0; i < nColumns; i++)
	{
		digitalWrite(columns[i], LOW);
		for (size_t j = 0; j < nRows; j++)
		{
			if (digitalRead(rows[j]) == LOW) {
				keys[i+j*nColumns] = true;
			} else {
				keys[i+j*nColumns] = false;
			}
		}
		digitalWrite(columns[i], HIGH); 
	}
}

void drawScreen() {
	if (vm->screen_drawn) {
		vm->screen_drawn = false;
		for (size_t i = 0; i < 64; i++)
		{
			for (size_t j = 0; j < 32; j++)
			{
				display.fillRect(i*PIXEL_SCALE,j*PIXEL_SCALE,PIXEL_SCALE,PIXEL_SCALE,vm->screen[j*64+i] ? WHITE : BLACK);
			}
		}  
		display.display();
	}
}

bool playing = false; // Is the speaker on or off
void playSound() {
	if (playing && !vm->playing_sound) {
		analogWrite(SPEAKER_PIN, 0);
		playing = false;
	} else if (!playing && vm->playing_sound) {
		analogWrite(SPEAKER_PIN, 8);
		playing = true;
	}
}

void io() {
	pollKeys(keys);
	playSound();
	drawScreen();
}

void ioLoop(void * parameter = NULL) {
	for(;;) {
		io();
		// delayMicroseconds(1000000/IO_FREQ); // 60 Hz
	}
}

std::vector<String> files;
void setup() {
	Wire.begin(11,10);
	Serial.begin(115200);
	
	// Pin initialization
	pinMode(SPEAKER_PIN,OUTPUT);
	for (size_t i = 0; i < nRows; i++)
	{
		pinMode(rows[i], INPUT_PULLUP);
	}
	for (size_t i = 0; i < nColumns; i++)
	{
		pinMode(columns[i], OUTPUT);
		digitalWrite(columns[i], HIGH); 
	}

	if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
		Serial.println(F("SSD1306 allocation failed"));
		for(;;);
	}
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.clearDisplay();
	
	if(!SPIFFS.begin(true)){
		Serial.println("An Error has occurred while mounting SPIFFS");
		for(;;);
	}
	File root = SPIFFS.open("/");

	// Listing all files
	File file = root.openNextFile();
	while(file){
		files.push_back(file.name());
		file = root.openNextFile();
	}

	display.println(" 1 - up\n 2 - select\n 3 - down\nPress 1 to start");
	display.printf("Target frequency: %i\n", TARGET_FREQ);
	display.printf("Refresh rate: %i\n", IO_FREQ);
#ifdef THREADED
	display.println("Running dual core");
#else
	display.println("Running single core");
#endif
	display.display();

	// Picking a file
	int selected = 0;
	while (true) {
		pollKeys(keys);
		
		// Input handling
		if (keys[0]) selected--;
		else if (keys[2]) selected++;
		else if (keys[1]) {
			display.clearDisplay();
			display.setCursor(0, 0);
			display.println("Loading " + files.at(selected));
			display.display();
			file = SPIFFS.open("/" + files.at(selected));
			break;
		}
		else {
			delay(50);
			continue; // Don't draw if no button was pressed
		}

		selected = max(min(selected,(int)files.size()-1),0);
		display.clearDisplay();
		display.setCursor(0, 0);

		for (size_t i = (selected/MAX_LINES)*MAX_LINES; i < files.size(); i++)
		{
			if (i == selected) display.print("-> ");
			display.println(files.at(i));
		}
		display.display(); 
		delay(250);
	}
	
	rom_size = file.size();
	file.read(rom,rom_size);
	file.close();
	Serial.printf("Read %i bytes from '/%s'\n", rom_size, files[selected].c_str());

	vm = new Chip8(rom,rom_size,keys);

#ifdef THREADED
	xTaskCreatePinnedToCore(
		ioLoop,
		"io",
		4096, // You might need to increase this depending on your display
		NULL,
		0,
		&ioTask,
		0);
#endif
}

void loop() {
	ulong start = micros();
#ifndef THREADED
	if (start - last_tick > 1000000/IO_FREQ) { // 60Hz
		io();
		last_tick = start;
	}
#endif
	vm->cycle(); // Execute one instruction
	ulong end = micros();
	
	// Cathing up or slowing down to the target frequency
	if (end-start < 1000000/TARGET_FREQ) {
		if (slowdown > 0) slowdown -= (1000000/TARGET_FREQ)-(end-start);
		else delayMicroseconds((1000000/TARGET_FREQ)-(end-start));
	} else {
		slowdown += end-start-1000000/TARGET_FREQ;
	}
}