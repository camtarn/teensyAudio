// Standard C libraries
#include <limits.h>

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
const unsigned int KEY_BUTTON_MAPPINGS[][2] = {
  { 0, 60 }, { 1, 61 }, { 2, 62 }, { 3, 63 }, { 4, 64 }, { 5, 65 }, { 6, 66 }, { 7, 67 }, { 8, 68 }, { 9, 69 }, { 10, 70 }, { 11, 71 }, { 12, 72 }
};

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
const int POLYPHONY = 4;
struct voice {
  int active;
  struct note note;
  unsigned long beganAt;
  int releasing;
  unsigned long releasedAt;
};
struct voice voices[POLYPHONY];

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
    keyButtons[i].bounce.update();
    if (keyButtons[i].bounce.fell() || keyButtons[i].bounce.rose()) {
      struct note note = getNoteForKey(keyButtons[i].pin);
      // keyButtons are connected to ground when pressed, with a pullup on the input
      if (keyButtons[i].bounce.fell()) {
        sendNoteOn(note);
      } else {
        sendNoteOff(note);
      }
    }
  }
}

struct note getNoteForKey(unsigned int buttonNumber) {
  if (buttonNumber >= KEY_BUTTON_COUNT) {
    // Defend against misconfigurations
    Serial.println("Requested a nonexistent button mapping - stopping");
    while (1) {};
  }
  for (int i = 0; i < KEY_BUTTON_COUNT; i++) {
    if (KEY_BUTTON_MAPPINGS[i][0] == buttonNumber) {
      unsigned int noteNumber = KEY_BUTTON_MAPPINGS[i][1];
      if (noteNumber > NOTE_COUNT) {
        // Defend against misconfigurations
        Serial.println("Requested a nonexistent note mapping - stopping");
        while (1) {};
      }
      return NOTES[noteNumber];
    }
  }
  // Defend against misconfigurations
  Serial.println("Didn't find a mapping for this button - stopping");
  while (1) {};
}

// Assign notes to polyphony voices
void sendNoteOn(struct note note) {
  // We don't allow multiple voices to play the same note.
  // If there is already a voice with this note active, retrigger it by altering its onset time
  for (int i = 0; i < POLYPHONY; i++) {
    if (voices[i].note.noteNumber == note.noteNumber) {
      voices[i].beganAt = millis();
      return;
    }
  }

  // This is a new note.
  // Try finding an inactive voice first
  for (int i = 0; i < POLYPHONY; i++) {
    if (!voices[i].active) {
      voices[i] = activateVoice(note);
      return;
    }
  }

  // If more keys are pressed than we have polyphony:
  // - Find notes which are already releasing:
  // -- Drop the note which has been releasing for the longest time
  // -- If there are multiple oldest notes with the same release time, use key pitch to break the tie
  // - If no notes are releasing:
  // -- Drop the note which started first
  // -- If multiple oldest notes have the same onset time, use key pitch to break the tie
  // First, find releasing voices
  struct voice *releasingVoices[POLYPHONY];
  int releasingVoiceCount = 0;
  for (int i = 0; i < POLYPHONY; i++) {
    if (voices[i].releasing) {
      releasingVoices[releasingVoiceCount] = &voices[i];
      ++releasingVoiceCount;
    }
  }
  if (releasingVoiceCount > 0) {
    // If we find some releasing voices, find the oldest release time
    unsigned long oldestReleaseTime = ULONG_MAX;
    for (int i = 0; i < releasingVoiceCount; i++) {
      if (releasingVoices[i]->releasedAt < oldestReleaseTime) {
        oldestReleaseTime = releasingVoices[i]->releasedAt;
      }
    }
    // Then find any voices which released at that time
    struct voice *oldestReleasingVoices[POLYPHONY];
    int oldestReleasingVoiceCount = 0;
    for (int i = 0; i < releasingVoiceCount; i++) {
      if (releasingVoices[i]->releasedAt == oldestReleaseTime) {
        oldestReleasingVoices[oldestReleasingVoiceCount] = releasingVoices[i];
        ++oldestReleasingVoiceCount;
      }
    }
    // If there's only one voice releasing, reuse that voice
    if (oldestReleasingVoiceCount == 1) {
      *oldestReleasingVoices[0] = activateVoice(note);
      return;
    }

    // Otherwise, find the voice with the lowest note number
    unsigned int lowestNote = UINT_MAX;
    struct voice *lowestOldestReleasingVoice;
    for (int i = 0; i < oldestReleasingVoiceCount; i++) {
      if (oldestReleasingVoices[i]->note.noteNumber < lowestNote) {
        lowestNote = oldestReleasingVoices[i]->note.noteNumber;
        lowestOldestReleasingVoice = oldestReleasingVoices[i];
      }
    }
    *lowestOldestReleasingVoice = activateVoice(note);
    return;
  }

  // We don't have any voices in their release phase, so we're going to have to reuse an active voice
  // First, find the oldest onset time
  unsigned long oldestOnsetTime = ULONG_MAX;
  for (int i = 0; i < POLYPHONY; i++) {
    if (voices[i].beganAt < oldestOnsetTime) {
      oldestOnsetTime = voices[i].beganAt;
    }
  }
  // Then find any voices which started at that time
  struct voice *oldestVoices[POLYPHONY];
  int oldestVoiceCount = 0;
  for (int i = 0; i < POLYPHONY; i++) {
    if (voices[i].beganAt == oldestOnsetTime) {
      oldestVoices[oldestVoiceCount] = &voices[i];
      ++oldestVoiceCount;
    }
  }
  // If there's only one oldest voice, reuse that voice
  if (oldestVoiceCount == 1) {
    *oldestVoices[0] = activateVoice(note);
    return;
  }

  // Otherwise, find the voice with the lowest note number
  unsigned int lowestOldestNote = UINT_MAX;
  struct voice *lowestOldestVoice;
  for (int i = 0; i < oldestVoiceCount; i++) {
    if (oldestVoices[i]->note.noteNumber < lowestOldestNote) {
      lowestOldestNote = oldestVoices[i]->note.noteNumber;
      lowestOldestVoice = oldestVoices[i];
    }
  }
  // And finally use that voice
  *lowestOldestVoice = activateVoice(note);
}

struct voice activateVoice(struct note note) {
  struct voice voice;
  voice.active = 1;
  voice.note = note;
  voice.beganAt = millis();
  voice.releasing = 0;
  return voice;
}

// Note off puts the voice into its releasing phase, rather than deactivating it right away
void sendNoteOff(struct note note) {
  for (int i = 0; i < POLYPHONY; i++) {
    if (voices[i].note.noteNumber == note.noteNumber) {
      voices[i].releasing = 1;
      voices[i].releasedAt = millis();
      return;
    }
  }
}

// The 'all off' command immediately stops all notes playing
void sendAllOff() {
  for (int i = 0; i < POLYPHONY; i++) {
    voices[i].active = 0;
    voices[i].releasing = 0;
  }
}

void playAudio() {
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
