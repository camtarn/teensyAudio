// Teensy Audio Library
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <Bounce.h>

// LCD driver
#include "src/lcd/LCD_Driver.h"
#include "src/lcd/DEV_Config.h"
#include "src/lcd/GUI_Paint.h"
#include "src/lcd/fonts.h"

#include "src/notes.h"

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
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  button3.update();
  button4.update();
  button5.update();
  button6.update();

  // Teensy Audio library
  AudioMemory(10);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.3);
  waveform1.begin(WAVEFORM_SINE);

  // LCD screen
  Config_Init();
  LCD_Init();
  LCD_SetBacklight(100);
  Paint_Clear(BLACK);
  Paint_DrawString_EN(10, 10, "Hello world!", &Font24, BLACK, WHITE);
}

void loop() {
  button3.update();
  button4.update();
  button5.update();
  button6.update();
  float amplitude = 1;
  float frequency;
  if (button3.read() == HIGH) {
    frequency = notes[60].frequency;
  } else if (button4.read() == HIGH) {
    frequency = notes[62].frequency;
  } else if (button5.read() == HIGH) {
    frequency = notes[64].frequency;
  } else if (button6.read() == HIGH) {
    frequency = notes[67].frequency;
  } else {
    amplitude = 0;
    frequency = 0;
  }
  waveform1.amplitude(amplitude);
  waveform1.frequency(frequency);
}
