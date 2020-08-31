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
  // Character arrays need to be long enough to store the number of "real" characters plus the
  // null terminator
  char filename[13]; // yyyyMMdd.txt = 12 chars + 1 null
  char timestamp[9]; // 00:00:00     =  8 chars + 1 null
  DateTime now = rtc.now();
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
