#include <Wire.h>
#include <SdFat.h>    // https://github.com/greiman/SdFat
#include <RTClib.h>   // https://github.com/adafruit/RTClib
#include <LowPower.h> // https://github.com/rocketscream/Low-Power

// chipSelect = 10 according to "Item description" section of
// https://www.play-zone.ch/de/dk-data-logging-shield-v1-0.html
#define SD_CHIP_SELECT 10

RTC_DS1307 rtc;

void setup() {
  // The clock won't work with this (thanks https://arduino.stackexchange.com/a/44305!)
  Wire.begin();

  bool rtcWasAlreadyConfigured;
  if (rtc.isrunning()) {
    rtcWasAlreadyConfigured = true;
  }
  else {
    // Note: I set my computer's time zone to UTC before running this for the first time so that
    // the time values would always be UTC and I could leave worrying about time zones for a
    // post-processing phase
    rtc.adjust(DateTime(__DATE__, __TIME__));
    rtcWasAlreadyConfigured = false;
  }

  Serial.begin(9600);

  if (rtcWasAlreadyConfigured) {
    Serial.println("setup: RTC is already running");
  }
  else {
    Serial.println("setup: RTC was not running, so it was set to the time of compilation");
  }
}

void loop() {
  DateTime now = rtc.now();
  if ((now.hour() >= 22) || (now.hour() < 8)) {
    // For my purposes, I don't care about light levels between 10pm and 8am and so the board can
    // go to sleep during that period to conserve the battery - even when DST pushes the hour forward
    // in the UK, I still don't care about 23:00 and 09:00 because that isn't a timeframe that I'll use
    // the garden for.
    // - I originally only checked if the hour was 22, which works if the sketch is left running without
    //   stopping or resetting but if it's plugged into the USB for an update at midnight then that
    //   would restart things and it would log readings
    // You might want to tweak or even remove this entirely if you live in a time zone that is not as
    // close to UTC as I am in the UK (either +0h or +1h from UTC at all times)
    sleepUntil8am();
  }
  
  // Character arrays need to be long enough to store the number of "real" characters plus the
  // null terminator
  char filename[13]; // yyyyMMdd.txt = 12 chars + 1 null
  char timestamp[9]; // 00:00:00     =  8 chars + 1 null
  snprintf(filename, sizeof(filename), "%04u%02u%02u.txt", now.year(), now.month(), now.day());
  snprintf(timestamp, sizeof(timestamp), "%02u:%02u:%02u", now.hour(), now.minute(), now.second());

  int sensorValue = analogRead(0);

  Serial.print(filename);
  Serial.print(" ");
  Serial.print(timestamp);
  Serial.print(" ");
  Serial.println(sensorValue);

  SdFat sd;
  if (!sd.begin(SD_CHIP_SELECT, SPI_HALF_SPEED)) {
    Serial.println("ERROR: sd.begin() failed");
  }
  else {
    SdFile file;
    if (!file.open(filename, O_WRITE | O_APPEND | O_CREAT)) {
      Serial.println("ERROR: file.open() failed - unable to write");
    }
    else {
      file.print(timestamp);
      file.print(" Sensor value: ");
      file.println(sensorValue);
      file.close();
    }
  }

  Serial.flush(); // Ensure we finish sending serial messages before going to sleep

  // 4x 8s is close enough to a reading every 30s, which gives me plenty of data
  // - Using this instead of "delay" should mean that the battery will power the device for longer
  for (int i = 0; i < 3; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}

void sleepUntil8am() {
  Serial.println("It's late at night, going to sleep until 8am..");
  Serial.flush(); // Ensure we finish sending serial messages before going to sleep
  
  while (true) {
    DateTime now = rtc.now();
    if (now.hour() == 8) {
      break;
    }

    // To save as much power as possible, leave longer breaks before checking the current time to
    // see whether it's time to return - if it's not even 7am yet then we can safely wait an hour
    // or so before checking with the RTC again. If it's after 7 but before 07:30 then a half hour
    // wait will be safe - after that, we'll check approximately every minute to try to get close
    // to realising that it's 8am without using too much power in doing so.
    int loopCount;
    if (now.hour() == 7) {
      if (now.minute() > 30) {
        loopCount = 8; // 8 x 8s = 64s ~= 1m
      }
      else {
        loopCount = 225; // 225 x 8s = 1800s = 0.5h
      }
    }
    else {
      loopCount = 450; // 450 x 8s = 3600s = 1h
    }
    for (int i = 0; i < loopCount; i++) {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    }
  }
}
