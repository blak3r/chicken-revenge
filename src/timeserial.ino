/**
 * REVENGE CHICKEN PRANK!
 * 
 * @author Blake Robertson
 */

#include "Arduino.h"
#include <TimeLib.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "SoftwareSerial.h"
#include "mpu6050.h"
#include "RTClib.h"



extern "C" {
  #include <user_interface.h>
}

// ------------ POWER OPTIONS ------------------
//#define MPU_ENABLED 1
//#define LCD_ENABLED 1
//#define LCD_BACKLIGHT_ENABLED 1
#define RTC_ENABLED 1
//#define RTC_FORCE_ADJUST 1
#define NO_SPEAKING_AT_NIGHT_MODE 1


#if LCD_ENABLED || MPU_ENABLED
  #define BOOT_DELAY 50
#else
  #define BOOT_DELAY 5
#endif

//------- SPEAK STYLE ----------
#define USE_MP3_RELAY 1
//#define USE_SPEAK_PIN 1

#ifdef RTC_ENABLED
  RTC_DS1307 rtc;
  char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
#endif

#ifdef MPU_ENABLED
  Adafruit_MPU6050 mpu;
s ensors_event_t a, g, temp;
#endif

#ifdef LCD_ENABLED
  LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

#define EXTRA_OFFSET_TIME 27000

///--------- IO PINS -------------------- //
#define LED_PIN D8
//define SPEAK_PIN D6
#define MP3_RELAY D7
#define LIGHT_SENSOR A0
#define RXPIN D6
#define TXPIN D5
#define ACCEL_INT_PIN 10 // AKA S3
#define DEBUG_PIN 9 // Aka S2
#define LOG Serial

SoftwareSerial mp3Serial(RXPIN, TXPIN); // RX, TX

#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

#define COMPILE_TIME __DATE__
#define RTCMEMORYSTART 65

#define TRACK_BUHGOK 6
#define TRACK_PUMPLEAD 3
#define TRACK_PUTMEDOWN 1
#define TRACK_WARNINGYOU 4
#define TRACK_LOWLEVELCLUCKS 2
#define TRACK_COUNTDOWN 7

#define TRACK_BUHGOK_LENGTH 4000
#define TRACK_PUTMEDOWN_LENGTH 7000
#define TRACK_COUNTDOWN_LENGTH 20000

// ------------ GLOBALS ------------------//
typedef struct {
  int timeInitialized;
  int count;
  unsigned long millisOffset;
  time_t compileTime;
  int prevLightSensorADC;
  bool lockedOut;
  bool ledState;
  time_t lockedOutUntil;
  time_t lastMotionTrackPlayedAt;
  time_t lastLightSensorTrackPlayedAt;
} rtcStore;

rtcStore rtcMem;

static char timeBuf[64];
static char altBuf[64];

//------------SETUP ---------------------//

void setupFreshBoot() {
  delay(1000);
  LOG.println("===== FRESH BOOT! ======");
  digitalWrite(DEBUG_PIN, 1);

  #ifdef LCD_ENABLED
    lcd.init();
    #ifdef LCD_BACKLIGHT_ENABLED
      lcd.backlight();
    #endif
    lcd.setCursor(0, 0);
    lcd.print(__DATE__);
    lcd.setCursor(0, 1);
    lcd.print(__TIME__);
    delay(1000);
  #endif

  #ifdef RTC_ENABLED
    if (! rtc.isrunning()) {
      LOG.println("RTC is NOT running, let's set the time!");
      // When time needs to be set on a new device, or after a power loss, the
      // following line sets the RTC to the date & time this sketch was compiled
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      // This line sets the RTC with an explicit date & time, for example to set
      // January 21, 2014 at 3am you would call:
      // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }
    #ifdef RTC_FORCE_ADJUST
      LOG.println("FORCE RTC Time Set En");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    #endif
  #endif


  mpu6050_initForInterrupts();
  mpu6050_clearInterruptRegisters();
  digitalWrite(DEBUG_PIN, 0);
}

void setupWakeupBoot() {
  #ifdef LCD_ENABLED
    lcd._displayfunction =  LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;
    lcd._numlines = 2;
    lcd._cols = 16;
    lcd._rows = 2;

    // lcd.init();
    // #ifdef LCD_BACKLIGHT_ENABLED
    //   lcd.backlight();
    // #endif
  #endif

  #ifdef MPU_ENABLED

    initMpuForInterrupts();
    // Try to initialize!
    if (!mpu.begin()) {
      LOG.println("Failed to find MPU6050 chip");
      while (1) {
        delay(10);
      }
    }
    LOG.println("MPU6050 Found!");
  #endif

}


void setup()  {
  time_t compileTime;
  time_t compileTimePlusRTCOffsets;
  byte rtcStore[2];
  bool doCompileTimesMatch;
  rst_info *resetInfo = ESP.getResetInfoPtr();

    
  WiFi.mode( WIFI_OFF );
  LOG.begin(115200);

  //pinMode(SPEAK_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(MP3_RELAY, INPUT);
  pinMode(DEBUG_PIN, OUTPUT);
  
  pinMode( ACCEL_INT_PIN, INPUT);
  //pinMode( ACCEL_INT_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN, 1);
  digitalWrite(DEBUG_PIN, 0);

  delay(BOOT_DELAY);

  if( !digitalRead(ACCEL_INT_PIN) ) {
    LOG.println("Accel Interrupt Det");
  }


  #if (LCD_ENABLED || MPU_ENABLED || RTC_ENABLED)
    Wire.begin();
  #endif

  mp3Serial.begin(9600);
 // delay(1000);

  #ifdef RTC_ENABLED 
    if (! rtc.begin()) {
      LOG.println("Couldn't find RTC");
      //LOG.flush();
      //abort();
    }
  #endif

  // SET THE TIME BASED ON COMPILE TIME!
  compileTime = getCompileTimeEpoch();
  readFromRTCMemory(); // Initializes the rtcMem global.
  compileTimePlusRTCOffsets = ((uint64_t) compileTime + (offsetMillis() / 1000));
  setTime( compileTimePlusRTCOffsets );

  doCompileTimesMatch = (uint64_t) rtcMem.compileTime == (uint64_t) compileTime;
  // if( !doCompileTimesMatch ) {
  //   LOG.printf("\n\n********* compileTimes don't match %llu %llu %d\n", (uint64_t) rtcMem.compileTime, (uint64_t) compileTime, doCompileTimesMatch);
  // } else {
  //   LOG.printf("compileTimes Match %llu %llu %d\n", (uint64_t) rtcMem.compileTime, (uint64_t) compileTime, doCompileTimesMatch);
  // }

  LOG.printf("RST REASON: %d, Do Compile Times Match?: %d\n", resetInfo->reason, doCompileTimesMatch );
  if( resetInfo->reason != 5 || !doCompileTimesMatch ) { // TODO also save the compile time string and compare it for easier testing.
    digitalWrite(DEBUG_PIN, 1);
    LOG.printf("CLEARING RTC MEMORY AND DOING FULL INIT.\n");
    rtcMem.timeInitialized = 99;
    rtcMem.count = 0;
    rtcMem.millisOffset = EXTRA_OFFSET_TIME;
    rtcMem.compileTime = compileTime;
    rtcMem.prevLightSensorADC = 0;
    rtcMem.lockedOut = false;
    rtcMem.lockedOutUntil = 0;
    rtcMem.ledState = 1;
    rtcMem.lastMotionTrackPlayedAt = 0;
    rtcMem.lastLightSensorTrackPlayedAt = 0;
    writeToRTCMemory();
    setupFreshBoot();
  }
  else {
    setupWakeupBoot();
  }

  printUptime();

  mp3_writeCmd( mp3_getResetCmd() );

  LOG.println("== SETUP ENDED ==");
}

bool ledState = false;
int noSpeakHours[] = {18,19,20,21,22,23,24,0,1,2,3,4,5,6,7,8,9,10};
int speakHours[] = {11, 12, 13, 14, 15, 16, 17};
int speakMins[] = {0, 30};
int lightSensorADC = 0;

size_t noSpeakHoursSize = sizeof( noSpeakHours ) / sizeof(int);
size_t speakHoursSize = sizeof( speakHours ) / sizeof(int);
size_t speakMinsSize = sizeof( speakMins ) / sizeof(int);

#define LOCK_OUT_TIME 6

int h;
int m;
int s;


void loop() {
  h = hour();
  m = minute();
  s = second();

  #ifdef RTC_ENABLED
    DateTime rtcNow = rtc.now();
    h = rtcNow.hour();
    m = rtcNow.minute();
    s = rtcNow.second();

    setTime( rtcNow.unixtime() );

    Serial.print("RTC: ");
    Serial.print(rtcNow.year(), DEC);
    Serial.print('/');
    Serial.print(rtcNow.month(), DEC);
    Serial.print('/');
    Serial.print(rtcNow.day(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[rtcNow.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(rtcNow.hour(), DEC);
    Serial.print(':');
    Serial.print(rtcNow.minute(), DEC);
    Serial.print(':');
    Serial.print(rtcNow.second(), DEC);
    Serial.print(" ");
    Serial.print(rtcNow.unixtime(), DEC);
    Serial.println();
#endif    

  // -------- RELEASE SPEAK LOCKS ------------
  if ( rtcMem.lockedOut ) {
    if( now() > rtcMem.lockedOutUntil ) {
      LOG.println("LOCK_OUT Released");
      rtcMem.lockedOut = false;
      rtcMem.lockedOutUntil = 0;
    }
  }


  // Check if it's in the valid hour, then minute.
  if ( indexOfInteger(speakHours, speakHoursSize, h) ) {
    if ( indexOfInteger(speakMins, speakMinsSize, m) ) {
      if ( s < 8) {
        LOG.printf("The hour (%d) and minute (%d) matched and s=%d... it's buhGok Time (if not locked out)\n", h, m, s);
        triggerBuhgok();
      }
    }
  } 

  //rtcMem.prevLightSensorADC = lightSensorADC;
  lightSensorADC = analogRead(LIGHT_SENSOR);
  if ( (lightSensorADC - rtcMem.prevLightSensorADC) > 100 && rtcMem.prevLightSensorADC != 0 ) {
    LOG.printf("Light Sensor Detected more light! - %d - %d = %d", lightSensorADC, rtcMem.prevLightSensorADC, (lightSensorADC - rtcMem.prevLightSensorADC) );
    triggerLightDetected();
  }
  rtcMem.prevLightSensorADC = lightSensorADC;

#ifdef MPU_ENABLED
  mpu.getEvent(&a, &g, &temp);
#endif

  // PRINT TIME LOG
  digitalClockDisplay();

  bool intStatus = digitalRead(ACCEL_INT_PIN);
  #define STATE_WHEN_MOTION_ACTIVE 0
  LOG.printf("Interrupt pin: %d, (Active=%d)\n", intStatus, STATE_WHEN_MOTION_ACTIVE);
  if( intStatus == STATE_WHEN_MOTION_ACTIVE ) {
    LOG.printf("MOTION TRIGGERING!\n");
    triggerMotion();
    
    mpu6050_clearInterruptRegisters();    
    mpu6050_initForInterrupts();
    mpu6050_clearInterruptRegisters();  
    delay(10);
    intStatus = digitalRead(ACCEL_INT_PIN);
    LOG.printf("Post Clearing Motion Interrupt pin: %d, (Active=%d)\n", intStatus, STATE_WHEN_MOTION_ACTIVE);
    if( intStatus == STATE_WHEN_MOTION_ACTIVE ) {
      LOG.println("ERROR Clearing MPU Interrupt");
      Wire.begin();
      mpu6050_clearInterruptRegisters();    
      mpu6050_initForInterrupts();
      mpu6050_clearInterruptRegisters();  
      intStatus = digitalRead(ACCEL_INT_PIN);
      LOG.println("AFTER 2nd Clear attempt");
    }
    delay(500);

  }

  if ( mp3Serial.available() ) {
    delay(100); // ensure we have all chars of the message 
    LOG.print("MP3 <-- ");
    while (mp3Serial.available()) {
      byte temp =  mp3Serial.read();
      LOG.printf( "%02X ", temp );
    }
    LOG.println();
  }

  if (Serial.available()) {
    processSyncMessage();
  }


  LOG.println("^^^^^^^^ SLEEPING ^^^^^^^^");
  mySleep(5000); // Writes to RTC Memory 

} // ------------ END OF LOOP ------------

void readFromRTCMemory() {
  system_rtc_mem_read(RTCMEMORYSTART, &rtcMem, sizeof(rtcMem));

  //Serial.print("count = ");
  //Serial.println(rtcMem.count);
  //yield();
}

void writeToRTCMemory() {
  rtcMem.count++;
  system_rtc_mem_write(RTCMEMORYSTART, &rtcMem, sizeof(rtcMem));

  //Serial.print("count = ");
  //Serial.println(rtcMem.count);
  //yield();
}

void playTrack(int trackNum, int lengthInMs) {
  LOG.printf("Attempting to play track %d, for %d\nms", trackNum, lengthInMs);
  //mp3_setVolume(10);
  
  // Check if in NO SPEAK TIMES
  #if NO_SPEAKING_AT_NIGHT_MODE
    if ( indexOfInteger(noSpeakHours, noSpeakHoursSize, h) ) {
      LOG.printf("NOT SPEAKING - IN NO SPEAK LOCKOUT, hour=%d\n", h);
      return;
    }
  #endif

  if ( !rtcMem.lockedOut ) {
    LOG.println("playing track");

    rtcMem.lockedOut = true;
    rtcMem.lockedOutUntil = now() + (lengthInMs/1000)*2;
    LOG.printf("TIME NOW: %lu will be locked out until %lu\n", (unsigned long) now(), (unsigned long) rtcMem.lockedOutUntil);

#ifdef USE_MP3_RELAY
    pinMode(MP3_RELAY, OUTPUT);
    digitalWrite(MP3_RELAY, 0);
    delay(2000);
#endif

    // USING SIMPLE ONE
#ifdef USE_SPEAK_PIN
    digitalWrite(SPEAK_PIN, 1);
    delay(500);
    digitalWrite(SPEAK_PIN, 0);
#endif

    mp3_writeCmd( mp3_getPlayTrackCmd(trackNum) );

#ifdef USE_MP3_RELAY
    delay(lengthInMs);
    pinMode(MP3_RELAY, INPUT);
    digitalWrite(MP3_RELAY, 0);
#endif
  } else {
    LOG.println("NOT PLAYING -- LOCKED OUT");
  }
}

void triggerBuhgok() {
  #ifdef LCD_ENABLED
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TIME TRIG");
  #endif
  playTrack(6, TRACK_BUHGOK_LENGTH);
}

void triggerLightDetected() {
  #ifdef LCD_ENABLED
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LIGHT DET");
  #endif
  rtcMem.lastLightSensorTrackPlayedAt = now();
  playTrack(TRACK_COUNTDOWN, TRACK_COUNTDOWN_LENGTH);
}

void triggerMotion() {
  #ifdef LCD_ENABLED
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MOTION DET");
  #endif
  rtcMem.lastMotionTrackPlayedAt = now();
  playTrack(TRACK_PUTMEDOWN, TRACK_PUTMEDOWN_LENGTH);
}

void digitalClockDisplay() {
  char accelBuf[64];
  char lcdTimeBuf[30];


  // digital clock display of the time
  sprintf(timeBuf, "%02d:%02d:%02d %d/%d/%d", hour(), minute(), second(), month(), day(), year());
  sprintf(lcdTimeBuf, "%02d:%02d:%02d %d %5d ", hour(), minute(), second(), day(), rtcMem.count);
  sprintf(altBuf, "LS: %d delta %d", lightSensorADC, lightSensorADC - rtcMem.prevLightSensorADC);

#ifdef MPU_ENABLED
  sprintf(accelBuf, "%f %f %f", a.acceleration.x, a.acceleration.y, a.acceleration.z);
#endif

  LOG.print(timeBuf);
  LOG.print("   ");
  LOG.print( rtcMem.count );
  LOG.print(altBuf);
  LOG.print("   ");
#ifdef MPU_ENABLED
  LOG.print(accelBuf);
#endif
  LOG.println();

#ifdef LCD_ENABLED
  if ( !rtcMem.lockedOut ) {
    //lcd.init();
    #ifdef LCD_BACKLIGHT_ENABLED
      lcd.backlight();
    #endif 
    lcd.setCursor(0, 0);
    lcd.print(lcdTimeBuf);
    lcd.setCursor(0, 1);
    lcd.print("LS: ");
    lcd.print(lightSensorADC);
  }
#endif

  LOG.print( "Millis: " );
  LOG.print( millis() );
  LOG.println();


}

void printDigits(int digits) {
  // utility function for digital clock display: prints preceding colon and leading 0
  LOG.print(":");
  if (digits < 10)
    LOG.print('0');
  LOG.print(digits);
}


void processSyncMessage() {
  unsigned long pctime;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

  if (Serial.find(TIME_HEADER)) {
    pctime = LOG.parseInt();
    if ( pctime >= DEFAULT_TIME) { // check the integer is a valid time (greater than Jan 1 2013)
      setTime(pctime); // Sync Arduino clock to the time received on the serial port
    }
  }
}

time_t requestSync()
{
  LOG.write(TIME_REQUEST);
  return 0; // the time will be sent later in response to serial mesg
}


char *tolowercase(char *letstr) {
  int l;
  for (l = 0; l <= strlen(letstr); l++) {
    if (letstr[l] >= 65 && letstr[l] <= 92) {
      letstr[l] = letstr[l] + 32;
    }
  }
  return letstr;
}

int getMonFromAbbr(char *abbr) {
  if (strlen(abbr) > 0)
    tolowercase(abbr);
  if ( strcmp(abbr, "jan") == 0 )
    return 0;
  if ( strcmp(abbr, "feb") == 0 )
    return 1;
  if ( strcmp(abbr, "mar") == 0 )
    return 2;
  if ( strcmp(abbr, "apr") == 0 )
    return 3;
  if ( strcmp(abbr, "may") == 0 )
    return 4;
  if ( strcmp(abbr, "jun") == 0 )
    return 5;
  if ( strcmp(abbr, "jul") == 0 )
    return 6;
  if ( strcmp(abbr, "aug") == 0 )
    return 7;
  if ( strcmp(abbr, "sep") == 0 )
    return 8;
  if ( strcmp(abbr, "oct") == 0 )
    return 9;
  if ( strcmp(abbr, "nov") == 0 )
    return 10;
  if ( strcmp(abbr, "dec") == 0 )
    return 11;
  return (-1);
}

// Convert from __DATE__ macro
uint64_t getCompileTimeEpoch() {
  char date_macro[20] = "";
  char time_macro[20] = "";
  strcpy(date_macro, __DATE__);
  strcpy(time_macro, __TIME__);
  char *token;
  char *timeToken;
  int hour = 0;
  int min = 0;
  int sec = 0;
  int yea = 0;
  int mon = 0;
  int day = 0;
  token = strtok(date_macro, " ");
  if (token != NULL) {
    mon = getMonFromAbbr(token);
    token = strtok(NULL, " ");
    if (token != NULL) {
      day = atoi(token);
      token = strtok(NULL, " ");
      if (token != NULL) {
        yea = atoi(token);
        struct tm t;
        time_t epoch_t;
        t.tm_year = yea - 1900;  // Year - 1900
        t.tm_mon = mon;          // Month, where 0 = jan
        t.tm_mday = day;         // Day of the month

        timeToken = strtok(time_macro, ":");
        hour = atoi(timeToken);
        timeToken = strtok(NULL, ":");
        min = atoi(timeToken);
        timeToken = strtok(NULL, ":");
        sec = atoi(timeToken);


        LOG.print("THE COMPILE TIME IS");
        LOG.print(__TIME__);
        LOG.print(" hour");
        LOG.print(hour);
        LOG.print(" min " );
        LOG.print(min);
        LOG.print( " sec " );
        LOG.print(sec);

        t.tm_hour = hour;
        t.tm_min = min;
        t.tm_sec = sec;
        t.tm_isdst = 1;         // Is DST on? 1 = yes, 0 = no, -1 = unknown
        epoch_t = mktime(&t);
        return epoch_t;
      }
    }
  }
  return (-1);
}


bool indexOfInteger(int* arr, size_t arrSize, int val) {
  int i;
  //Serial.println(val);
  for (i = 0; i < arrSize; i++) {
    //Serial.printf("%d == %d", arr[i], val);

    if ( arr[i] == val ) {
      if (arrSize <= 6 ) {
        LOG.printf("%d == %d, arrSize = %d\n", arr[i], val, arrSize );
      }
      return true;
    }
  }
  return false;
}

//--------- MP3 Mini Clone Functions ---------------------//
// SoftwareSerial mp3Serial(RXPIN, TXPIN); <--- Define a "Stream" for serial communications and call it mp3Serial.

// MP3 Player
byte resetCmd[]  = { 0x7E, 0xFF, 0x06, 0x0C, 0x01, 0x00, 0x00, 0xFE, 0xEE, 0xEF };
byte track1Cmd[] = { 0x7e, 0xff, 0x06, 0x03, 0x01, 0x00, 0x01, 0xfe, 0xf7, 0xef};
byte track6Cmd[] = { 0x7e, 0xff, 0x06, 0x03, 0x01, 0x00, 0x06, 0xef};
#define TRACK_CMD_SIZE 8
byte _sharedMp3CmdBuf[ TRACK_CMD_SIZE ];
#define MP3CMD_PLAYTRACK 0x03
#define MP3CMD_RESET 0x0C
#define MP3CMD_SLEEP 0x0A
#define MP3CMD_VOLUME 0x43
#define MP3CMD_NORMAL

// assumes default root folder
byte* mp3_getCmd( byte cmdOpCode, byte msb, byte lsb, bool wantFeedback = 1 ) {
  _sharedMp3CmdBuf[0] = 0x7E;
  _sharedMp3CmdBuf[1] = 0xFF;
  _sharedMp3CmdBuf[2] = 0x06;
  _sharedMp3CmdBuf[3] = cmdOpCode;
  _sharedMp3CmdBuf[4] = wantFeedback ? 0x01 : 0x00;
  _sharedMp3CmdBuf[5] = msb;
  _sharedMp3CmdBuf[6] = lsb;
  _sharedMp3CmdBuf[7] = 0xEF;

  return _sharedMp3CmdBuf;
}

void mp3_setVolume(int level) {
  return mp3_writeCmd( mp3_getCmd( MP3CMD_VOLUME, 0, level, 0) );
}

byte* mp3_getPlayTrackCmd(byte trackNum) {
  return mp3_getCmd( MP3CMD_PLAYTRACK, 0, trackNum, 1 );
}

byte* mp3_getResetCmd()  {
  return mp3_getCmd( MP3CMD_RESET, 0, 0, 1);
}

byte* mp3_getSleepCmd()  {
  return mp3_getCmd( 0x09, 0, 0x05, 0);
}

void mp3_writeCmd( byte* cmdBuf) {
  LOG.print("MP3 --> ");
  for (int i = 0; i < TRACK_CMD_SIZE; i++) {
    LOG.printf("%02X ", (byte) cmdBuf[i]);
  }
  LOG.print("\n");
  mp3Serial.write( cmdBuf, TRACK_CMD_SIZE);
}

// ----------------- MY TIME KEEPING --------------------------------
unsigned long offsetMillis()
{
  LOG.printf("READ:  rtcMem.millisOffset: %lu cnt: %d\n", rtcMem.millisOffset, rtcMem.count);
  return millis() + rtcMem.millisOffset;
}

void mySleep(unsigned long sleepMillis)
{
  unsigned long _offsetMillis;
  unsigned long computedSleepMillis;
  _offsetMillis = offsetMillis();
  computedSleepMillis = sleepMillis;

  if( (rtcMem.count % 57) == 0 ) {
    computedSleepMillis += 5000;
  }
    // if( (rtcMem.count % 67) == 0 ) {
    //   computedSleepMillis += 4000;
    // }

  //LOG.printf("offsetMillis() = %lu computedSleepMillis=%lu\n", _offsetMillis, computedSleepMillis);
  
  rtcMem.millisOffset = _offsetMillis + computedSleepMillis;
  writeToRTCMemory();
  LOG.printf("WROTE: rtcMem.millisOffset: %lu\n", rtcMem.millisOffset);
  
  ESP.deepSleep( sleepMillis * 1000);
  delay(100); // supposed to follow deepsleep
}

void printUptime()
{
  unsigned long seconds, sec, min, hrs;
  seconds = offsetMillis() / 1000;

  sec = seconds % 60;
  seconds /= 60;
  min = seconds % 60;
  seconds /= 60;
  hrs = seconds % 24;

  LOG.printf("UPTIME: %02d:%02d:%02d\n", hrs, min, sec);
}
//----------------------------------------------------------------------
