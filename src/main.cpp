
// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <Adafruit_LEDBackpack.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <SD.h>
#include <Servo.h>

void printDirectory(File dir, int numTabs);
void checkRemote();

Servo servoR;  // create servo object to control a servo
Servo servoL;  // create servo object to control a servo

// open and closed door values
int servoCloseL = 100;
int servoCloseR = 11;
int servoOpenL = 11;
int servoOpenR = 100;
int doorDelay = 30; // ms delay before right door starts to close

// pins for single LEDs
byte ledRed = 15;
byte ledYellow = 16;

// activation button pin
byte activationSwitch = 5;

// smoke machine
byte smokePin = 5;
boolean endingSmoke = false; // smoke effect after trap close?

Adafruit_24bargraph bar = Adafruit_24bargraph();

// These are the pins used for the breakout board
#define BREAKOUT_RESET  9      // VS1053 reset pin (output)
#define BREAKOUT_CS     10     // VS1053 chip select pin (output)
#define BREAKOUT_DCS    8      // VS1053 Data/command select pin (output)
// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 2       // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer =
  // create breakout-example object!
  //Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);
  // create shield-example object!
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

int trapState = 0;
boolean redLEDState = 1;
boolean remotePressed = false;
boolean smokeActive = false;
unsigned long debounceBuffer = 0;
unsigned long redFlashTime = 0;
unsigned long whiteFlashTime = 0;
unsigned long smokeToggleTime = 0;
byte activeFlasher = 0;

////

void setup() {
  Serial.begin(57600);
  Serial.println("Who ya gonna call?");

  //  musicPlayer.GPIO_pinMode(1, OUTPUT);
  //  musicPlayer.GPIO_pinMode(2, OUTPUT);
  //  musicPlayer.GPIO_pinMode(3, OUTPUT);
  //  musicPlayer.GPIO_digitalWrite(1, LOW);
  //  musicPlayer.GPIO_digitalWrite(2, LOW);
  //  musicPlayer.GPIO_digitalWrite(3, LOW);

  pinMode (ledYellow, OUTPUT);
  pinMode (ledRed, OUTPUT);

  servoR.attach(3);  // attaches the servo on pin 3 to the servo object
  servoL.attach(5);  // attaches the servo on pin 5 to the servo object
  servoR.write(servoCloseR);


  // init the bargraph LEDs
  bar.begin(0x70);  // pass in the address

  for (uint8_t b = 12; b < 24; b++ ) {
    if ((b % 3) == 0)  bar.setBar(b, LED_RED);
    if ((b % 3) == 1)  bar.setBar(b, LED_YELLOW);
    if ((b % 3) == 2)  bar.setBar(b, LED_GREEN);
  }
  bar.writeDisplay();
  delay(100);
  for (uint8_t b = 0; b < 12; b++) {
    bar.setBar(23 - b, LED_OFF);
    bar.writeDisplay();
  }

  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1);
  }
  Serial.println(F("VS1053 found"));

  musicPlayer.GPIO_pinMode(activationSwitch, INPUT);

  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
  delay(1000);


  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");

  // list files
  printDirectory(SD.open("/"), 0);

  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(0, 0);

  /***** Two interrupt options! *******/
  // This option uses timer0, this means timer1 & t2 are not required
  // (so you can use 'em for Servos, etc) BUT millis() can lose time
  // since we're hitchhiking on top of the millis() tracker
  //musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT);

  // This option uses a pin interrupt. No timers required! But DREQ
  // must be on an interrupt pin. For Uno/Duemilanove/Diecimilla
  // that's Digital #2 or #3
  // See http://arduino.cc/en/Reference/attachInterrupt for other pins
  // *** This method is preferred
  if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT))
    Serial.println(F("DREQ pin is not an interrupt pin"));

  //    // Start playing a file, then we can do stuff while waiting for it to finish
  //    if (! musicPlayer.startPlayingFile("GB_inst.mp3")) {
  //      Serial.println("Could not open file GB_inst.mp3");
  //      while (1);
  //    }
  //  Serial.println(F("Started playing in background."));

  // Alternately, we can just play an entire file at once
  // This doesn't happen in the background, instead, the entire
  // file is played and the program will continue when it's done!
  // musicPlayer.playFullFile("track001.ogg");

  musicPlayer.GPIO_digitalWrite(6, LOW);
  musicPlayer.GPIO_pinMode(6, INPUT);
}

void loop()
{
  checkRemote();

  // open the trap!
  if (remotePressed && trapState == 0)
  {
    Serial.println("Activate Trap!");

    // open doors
    servoR.attach(3);  // attaches the servo on pin 3 to the servo object
    servoL.attach(5);  // attaches the servo on pin 5 to the servo object
    servoR.write(servoOpenR);
    servoL.write(servoOpenL);

   
    // smoke on
    if (millis() > smokeToggleTime && smokeActive == false)
    {
      smokeActive = true;
      musicPlayer.GPIO_pinMode(6, OUTPUT);
      musicPlayer.GPIO_digitalWrite(6, HIGH);

      // smoke for 4 seconds
      smokeToggleTime = millis() + 4000;
    }
    
    // turn on middle flasher
    activeFlasher = 3;
    musicPlayer.GPIO_pinMode(activeFlasher, OUTPUT);
    musicPlayer.GPIO_digitalWrite(activeFlasher, HIGH);

    // playSFX
    musicPlayer.setVolume(0, 0);
    musicPlayer.playFullFile("turn_on.mp3");

    // trap is open
    trapState = 1;
    musicPlayer.startPlayingFile("sprkloop.mp3");
  }

  // close the trap!
  else if (remotePressed && trapState == 1)
  {
    Serial.println("Deactivate Trap.");
    trapState = 2;

    // turn off flashers
    for (uint8_t i = 2; i < 5; i++) {
      musicPlayer.GPIO_digitalWrite(i, LOW);
      musicPlayer.GPIO_pinMode(i, INPUT);
    }

    // stop smoke
    if (!endingSmoke)
    {
      smokeActive = false;
      smokeToggleTime = 0;
      musicPlayer.GPIO_pinMode(6, INPUT);
      musicPlayer.GPIO_digitalWrite(6, LOW);
    }

    // close doors
     servoR.attach(3);  // attaches the servo on pin 3 to the servo object
    servoL.attach(5);  // attaches the servo on pin 5 to the servo object
    servoL.write(servoCloseL);
    delay(doorDelay);
    servoR.write(servoCloseR);

    
    // playSFX
    musicPlayer.stopPlaying();
    musicPlayer.playFullFile("capture.mp3");

    // Turn off servos (avoids twitching)
     servoL.detach();
     servoR.detach();
    
    // fill bargraph
    for (uint8_t b = 0; b < 12; b++) {
      bar.setBar(23 - b, LED_YELLOW);
      bar.writeDisplay();
      delay(50);
    }

    // yellow LED on
    digitalWrite(ledYellow, HIGH);   // turn the LED on (HIGH is the voltage level)

    // start "full" sound loop
    musicPlayer.setVolume(30, 30);
    musicPlayer.startPlayingFile("fullloop.mp3");
  


  }
  
  // reset
  else if (remotePressed && trapState == 2)
  {
    Serial.println("RESET!");
    musicPlayer.stopPlaying();
    trapState = 0;
    redLEDState = 1;

    // turn off LEDs
    digitalWrite(ledRed, LOW);
    digitalWrite(ledYellow, LOW);
    for (uint8_t b = 0; b < 12; b++) {
      bar.setBar(23 - b, LED_OFF);
      bar.writeDisplay();
    }

    // make sure smoke is off
    smokeActive = false;
    smokeToggleTime = 0;
    musicPlayer.GPIO_pinMode(6, INPUT);
    musicPlayer.GPIO_digitalWrite(6, LOW);
  }

  // looping states
  // trap open! sparks!
  else if (trapState == 1)
  {
    // smoke on
    if (millis() > smokeToggleTime && smokeActive == false)
    {
      smokeActive = true;
      musicPlayer.GPIO_pinMode(6, OUTPUT);
      musicPlayer.GPIO_digitalWrite(6, HIGH);

      // smoke for 4 seconds
      smokeToggleTime = millis() + 4000;
    }

    // smoke off
    if (millis() > smokeToggleTime && smokeActive == true)
    {
      smokeActive = false;
      musicPlayer.GPIO_digitalWrite(6, LOW);
      musicPlayer.GPIO_pinMode(6, INPUT);
      // wait 8 seconds
      smokeToggleTime = millis() + 8000;
    }

    // flash bright LEDs
    if (millis() > whiteFlashTime)
    {
      whiteFlashTime = millis() + 50;
      musicPlayer.GPIO_digitalWrite(activeFlasher, LOW);
      musicPlayer.GPIO_pinMode(activeFlasher, INPUT);
      activeFlasher = random(2, 5);
      musicPlayer.GPIO_pinMode(activeFlasher, OUTPUT);
      musicPlayer.GPIO_digitalWrite(activeFlasher, HIGH);
    }
  }

  // full trap! blink red led.
  else if (trapState == 2)
  {
    if (millis() > redFlashTime)
    {
      redFlashTime = millis() + 150;
      redLEDState = !redLEDState;
      if (redLEDState) digitalWrite(ledRed, HIGH);
      else digitalWrite(ledRed, LOW);
    }

    // smoke on
    if (endingSmoke)
    {
      if (millis() > smokeToggleTime && smokeActive == false)
      {
        smokeActive = true;
        musicPlayer.GPIO_pinMode(6, OUTPUT);
        musicPlayer.GPIO_digitalWrite(6, HIGH);

        // smoke for 4 seconds
        smokeToggleTime = millis() + 4000;
      }

      // smoke off
      if (millis() > smokeToggleTime && smokeActive == true)
      {
        smokeActive = false;
        musicPlayer.GPIO_digitalWrite(6, LOW);
        musicPlayer.GPIO_pinMode(6, INPUT);
        // wait 8 seconds
        smokeToggleTime = millis() + 8000;
      }
    }
  }
}

// MP3 FILE NAMES:
// turn_on.mp3
// sparks.mp3
// sprkloop.mp3
// capture.mp3
// full.mp3
// fullloop.mp3
// GB_inst.mp3

void checkRemote()
{
  //  Serial.print("trapState: ");
  //  Serial.println(trapState);
  //  delay(500);

  if (musicPlayer.GPIO_digitalRead(activationSwitch) == HIGH && millis() > debounceBuffer)
  {
    Serial.println("Remote press detected.");
    Serial.print("Trap state: ");
    Serial.println(trapState);
    debounceBuffer = millis() + 1000;
    remotePressed = true;
  }
  else remotePressed = false;
}

void servoTest()
{
  // Servo Test
  servoR.write(servoOpenR);
  servoL.write(servoOpenL);
  delay(2000);
  servoR.write(servoCloseR);
  servoL.write(servoCloseL);
  delay(2000);
}

void bargraphTest()
{
  // Bargraph Test
  for (uint8_t b = 0; b < 12; b++) {
    bar.setBar(23 - b, LED_YELLOW);
    bar.writeDisplay();
    delay(50);
    bar.setBar(23 - b, LED_OFF);
    bar.writeDisplay();
  }
}

void remoteTest()
{
  // remote test
  Serial.print("Remote Pin: ");
  Serial.println(digitalRead(activationSwitch));
  if (digitalRead(activationSwitch) == LOW) digitalWrite(ledRed, HIGH);
  else digitalWrite(ledRed, LOW);
  delay(50);
}

void flasherTest()
{
  // Super bright LED test
  for (uint8_t i = 2; i < 5; i++) {
    musicPlayer.GPIO_pinMode(i, OUTPUT);
    musicPlayer.GPIO_digitalWrite(i, HIGH);
    delay(100);
    musicPlayer.GPIO_digitalWrite(i, LOW);
    musicPlayer.GPIO_pinMode(i, INPUT);
  }
}

void flasherTest2()
{
  // Super bright LED test
  for (uint8_t i = 2; i < 7; i++) {
    Serial.print("Output/High: ");
    Serial.println(i);
    musicPlayer.GPIO_pinMode(i, OUTPUT);
    musicPlayer.GPIO_digitalWrite(i, HIGH);
    delay(1000);
    musicPlayer.GPIO_digitalWrite(i, LOW);
    musicPlayer.GPIO_pinMode(i, INPUT);
  }
}

void ledTest()
{
  // led TEST
  digitalWrite(ledYellow, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(ledRed, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);              // wait for a second
  digitalWrite(ledYellow, LOW);    // turn the LED off by making the voltage LOW
  digitalWrite(ledRed, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);              // wait for a second
}

/// File listing helper
void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      //Serial.println("**nomorefiles**");
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

