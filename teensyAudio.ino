// Teensy Audio Library
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <Bounce.h>

// LCD driver
#include "lcd/LCD_Driver.h"
#include "lcd/DEV_Config.h"
#include "lcd/GUI_Paint.h"
#include "lcd/fonts.h"

AudioSynthWaveform    waveform1;
AudioOutputI2S        i2s1;
AudioConnection       patchCord1(waveform1, 0, i2s1, 0);
AudioConnection       patchCord2(waveform1, 0, i2s1, 1);
AudioControlSGTL5000  sgtl5000_1;

void setup() {
  AudioMemory(10);
  Serial.begin(115200);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.3);
}

void loop() {
}
