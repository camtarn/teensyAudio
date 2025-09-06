// Standard C libraries
#include <string.h>

// Arduino libraries
#include <Wire.h>
#include <SPI.h>

// Adafruit MCP23017 library
#include <Adafruit_MCP23X17.h>

// Teensy Audio library
#include <Audio.h>

// Bounce2 library
#include <Bounce2.h>

// Bounce2mcp library
// (fork of Bounce2 for keyButtons attached to an MCP23* chip)
#include <Bounce2mcp.h>

// LCD driver
#include "src/lcd/LCD_Driver.h"
#include "src/lcd/GUI_Paint.h"

// Project files
#include "src/notes.h"
#include "src/images.h"

// IO extender
Adafruit_MCP23X17 mcp;

// Regular non-keyboard keyButtons
Bounce buttonOk = Bounce(3, 15);
Bounce buttonBack = Bounce(4, 15);
Bounce buttonUp = Bounce(5, 15);
Bounce buttonDown = Bounce(6, 15);
BounceMcp buttonLeft = BounceMcp();
BounceMcp buttonRight = BounceMcp();

// Maps button number to MIDI note
const int BUTTON_MAPPINGS[][2] = {
  { 0, 60 }, { 1, 61 }, { 2, 62 }, { 3, 63 }, { 4, 64 }, { 5, 65 }, { 6, 66 }, { 7, 67 }, { 8, 68 }, { 9, 69 }, { 10, 70 }, { 11, 71 }, { 12, 72 }
};
const int POLYPHONY = 4;

// Keyboard buttons
const int KEY_BUTTON_COUNT = 13;
struct keyButton {
  int pin;
  BounceMcp bounce;
};
struct keyButton keyButtons[] = {
  { 0, BounceMcp() },
  { 1, BounceMcp() },
  { 2, BounceMcp() },
  { 3, BounceMcp() },
  { 4, BounceMcp() },
  { 5, BounceMcp() },
  { 6, BounceMcp() },
  { 7, BounceMcp() },
  { 8, BounceMcp() },
  { 9, BounceMcp() },
  { 10, BounceMcp() },
  { 11, BounceMcp() },
  { 12, BounceMcp() },
  { 13, BounceMcp() }
};
int pressedKeyButtons[KEY_BUTTON_COUNT];

// Audio system objects
AudioSynthWaveform waveform1;
AudioSynthWaveform waveform2;
AudioSynthWaveform waveform3;
AudioSynthWaveform waveform4;
AudioSynthWaveform *waveforms[] = { &waveform1, &waveform2, &waveform3, &waveform4 };
AudioSynthWaveformSine lfo1;
AudioMixer4 mixer;
AudioFilterStateVariable filter1;
AudioOutputI2S i2s1;
AudioControlSGTL5000 sgtl5000_1;
AudioConnection patchCords[] = {
  { filter1, 0, i2s1, 0 },
  { filter1, 0, i2s1, 1 },
  { waveform1, 0, mixer, 0 },
  { waveform2, 0, mixer, 1 },
  { waveform3, 0, mixer, 2 },
  { waveform4, 0, mixer, 3 },
  { mixer, 0, filter1, 0 },
  { lfo1, 0, filter1, 1 }
};

// Polyphony note assignment system
struct activeNote {
  int keyButtonNumber;
  int noteNumber;
  unsigned long beganAt;
  int releasing;
};
struct activeNote activeNotes[POLYPHONY];

void setup() {
  // Onboard hardware
  Serial.begin(115200);
  Serial.println("Starting up");

  // IO extender
  // Use I2C bus 1 on pins 16 and 17
  if (!mcp.begin_I2C(MCP23XXX_ADDR, &Wire1)) {
    Serial.println("Error initializing IO extender I2C - stopping");
    while (1) {};
  }
  // No pullup resistors means we need to talk slowly (100KHz)
  Wire1.setClock(100000);
  // Attach debouncers to MCP pins
  buttonLeft.attach(mcp, 13, 15);
  buttonRight.attach(mcp, 14, 15);
  for (int i = 0; i < KEY_BUTTON_COUNT; i++) {
    keyButtons[i].bounce.attach(mcp, keyButtons[i].pin, 15);
  }

  // Teensy Audio library
  AudioMemory(20);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.3);
  waveform1.begin(WAVEFORM_SAWTOOTH);
  waveform2.begin(WAVEFORM_SAWTOOTH);
  waveform3.begin(WAVEFORM_SAWTOOTH);
  waveform4.begin(WAVEFORM_SAWTOOTH);
  lfo1.amplitude(1);
  lfo1.frequency(0.2);
  filter1.frequency(1000);
  filter1.resonance(1.5);
  filter1.octaveControl(1);

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
  for (int i = 0; i < KEY_BUTTON_COUNT; i++) {
    pressedKeyButtons[i] = 0;
  }
  int nextIdx = 0;
  for (int i = 0; i < KEY_BUTTON_COUNT; i++) {
    keyButtons[i].bounce.update();
    // keyButtons are connected to ground when pressed, with a pullup on the input
    if (keyButtons[i].bounce.read() == LOW) {
      pressedKeyButtons[nextIdx] = keyButtons[i].pin;
      ++nextIdx;
    }
  }
}

void playAudio() {
  // Assign notes to polyphony voices
  // If more keys are pressed than we have polyphony, drop oldest key presses
  // first, using key pitch to break ties when multiple keys are pressed at
  // once (highest notes win)
  // FIXME: in order to work with MIDI, we should actually make this use note on/note off events.
  // When we see a note on, we should check if we have any free voices. If not, we first check
  // whether a voice for that note is already in its release phase, and if so, reuse that voice. 
  // If we can't do that, we stop the voice which has the oldest beganAt time, or if all the notes
  // have the same beganAt, we stop the lowest note. When we see a noteOff event, we set that note
  // to release, and clear it from the active voices when its release phase is done.

  // Determine which notes we want to play
  struct activeNote proposedActiveNotes[KEY_BUTTON_COUNT];
  for (int i = 0; i < KEY_BUTTON_COUNT; i++) {
    // Both key buttons and notes use 0 to indicate no button/no note
    proposedActiveNotes[i].keyButtonNumber = pressedKeyButtons[i];
    proposedActiveNotes[i].noteNumber = getNoteNumber(pressedKeyButtons[i]);
    proposedActiveNotes[i].releasing = 0;
  }

  // Figure out if we have enough voices to play those notes

  // float amplitude = 1;
  // float frequency;
  // if (button3.read() == LOW) {
  //   frequency = notes[60].frequency;
  // } else if (button4.read() == LOW) {
  //   frequency = notes[62].frequency;
  // } else if (button5.read() == LOW) {
  //   frequency = notes[64].frequency;
  // } else if (button6.read() == LOW) {
  //   frequency = notes[67].frequency;
  // } else {
  //   amplitude = 0;
  //   frequency = 0;
  // }
  // waveform1.amplitude(amplitude);
  // waveform1.frequency(frequency);
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
