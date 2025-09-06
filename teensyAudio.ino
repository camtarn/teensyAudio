// Teensy Audio Library
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

// Bounce v1 library
#include <Bounce.h>

// LCD driver
#include "src/lcd/LCD_Driver.h"
#include "src/lcd/GUI_Paint.h"

// Project files
#include "src/notes.h"
#include "src/images.h"

AudioSynthWaveform waveform1;
AudioOutputI2S i2s1;
AudioConnection patchCord1(waveform1, 0, i2s1, 0);
AudioConnection patchCord2(waveform1, 0, i2s1, 1);
AudioControlSGTL5000 sgtl5000_1;
Bounce button3 = Bounce(3, 15);
Bounce button4 = Bounce(4, 15);
Bounce button5 = Bounce(5, 15);
Bounce button6 = Bounce(6, 15);

void setup() {
  // Onboard hardware
  Serial.begin(115200);
  Serial.println("Starting up");
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  button3.update();
  button4.update();
  button5.update();
  button6.update();
  Serial.printf("Button 3: %d\n", button3.read());
  Serial.printf("Button 4: %d\n", button4.read());
  Serial.printf("Button 5: %d\n", button5.read());
  Serial.printf("Button 6: %d\n", button6.read());

  // Teensy Audio library
  AudioMemory(10);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.3);
  waveform1.begin(WAVEFORM_SINE);

  // LCD screen
  Config_Init();
  LCD_Init();
  LCD_SetBacklight(100);
  Paint_NewImage(LCD_WIDTH, LCD_HEIGHT, 90, WHITE);
  Paint_Clear(BLACK);
}

void loop() {
  scanIO();
  playAudio();
  drawScreen();
}

void scanIO() {
}

void playAudio() {
}

int getNoteNumber(int buttonNumber) {
  for (int i = 0; i < KEY_BUTTON_COUNT; i++) {
    if (BUTTON_MAPPINGS[i][0] == buttonNumber) {
      return BUTTON_MAPPINGS[i][1];
    }
  }
  return 0;
}

void drawScreen() {
  int timeMillis = millis();

  // Update screen once per second
  const char *smiley;
  if (timeMillis % 2000 < 1000) {
    smiley = smileyMouthClosed;
  } else {
    smiley = smileyMouthOpen;
  }
  if (timeMillis % 1000 == 0) {
    for (int x = 0; x < smileyWidth; x++) {
      for (int y = 0; y < smileyHeight; y++) {
        int colour;
        if (smiley[x + (y * smileyWidth)]) {
          colour = CYAN;
        } else {
          colour = BLACK;
        }
        Paint_DrawRectangle(x * 4 + 50, y * 4 + 50, (x + 1) * 4 + 50, (y + 1) * 4 + 50, colour, DOT_PIXEL_1X1, DRAW_FILL_FULL);
      }
    }
  }
}
