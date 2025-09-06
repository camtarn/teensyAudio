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
  int timeMillis = millis();

  button3.update();
  button4.update();
  button5.update();
  button6.update();

  if (button3.fallingEdge()) Serial.println("Button 3 pressed");
  if (button4.fallingEdge()) Serial.println("Button 4 pressed");
  if (button5.fallingEdge()) Serial.println("Button 5 pressed");
  if (button6.fallingEdge()) Serial.println("Button 6 pressed");

  float amplitude = 1;
  float frequency;
  if (button3.read() == LOW) {
    frequency = notes[60].frequency;
  } else if (button4.read() == LOW) {
    frequency = notes[62].frequency;
  } else if (button5.read() == LOW) {
    frequency = notes[64].frequency;
  } else if (button6.read() == LOW) {
    frequency = notes[67].frequency;
  } else {
    amplitude = 0;
    frequency = 0;
  }
  waveform1.amplitude(amplitude);
  waveform1.frequency(frequency);

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
