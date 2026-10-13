/*!
 * @file basic_readings.ino
 *
 * Basic example for the Adafruit TCS3448 14-Channel Multi-Spectral Sensor.
 *
 * Reads all 18 results from the three automatic SMUX cycles and prints the
 * 12 spectral channels plus one broadband visible channel.
 */

#include <Adafruit_TCS3448.h>
#include <Wire.h>

Adafruit_TCS3448 tcs3448;

void setup() {
  Serial.begin(115200);
  // Wait for the Serial Monitor on native USB boards. Remove this loop to let
  // the sketch run immediately from battery power.
  while (!Serial) {
    delay(10);
  }
  delay(250);

  Serial.println(F("Adafruit TCS3448 basic readings"));
  Wire.setPins(5, 4);
  Wire.setClock(400000);
  if (!tcs3448.begin()) {
    Serial.println(F("Could not find a TCS3448 sensor!"));
    while (true) {
      delay(10);
    }
  }

  if (!tcs3448.setGain(TCS3448_GAIN_64X) || !tcs3448.setATIME(29) ||
      !tcs3448.setASTEP(599) ||
      !tcs3448.setSMUXMode(TCS3448_SMUX_18CH)) {
    Serial.println(F("Could not configure the sensor!"));
    while (true) {
      delay(10);
    }
  }

  tcs3448.setLEDCurrent(20);
  if (tcs3448.enableLED(true)) {
    Serial.println(F("Could not set the LED current!"));
  }

  Serial.print(F("Integration time: "));
  Serial.print(tcs3448.getIntegrationTime());
  Serial.println(F(" ms"));
  Serial.println();
  Serial.println(F("F1\tF2\tFZ\tF3\tF4\tF5\tFY\tFXL\tF6\tF7\tF8\tNIR\tVIS"));
}

void loop() {
  uint16_t readings[TCS3448_CHANNEL_COUNT];

  if (tcs3448.readAllChannels(readings)) {
    Serial.print(readings[TCS3448_CHANNEL_F1]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_F2]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_FZ]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_F3]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_F4]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_F5]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_FY]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_FXL]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_F6]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_F7]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_F8]);
    Serial.print('\t');
    Serial.print(readings[TCS3448_CHANNEL_NIR]);
    Serial.print('\t');
    Serial.println(readings[TCS3448_CHANNEL_VIS_TL_0]);
  }

  delay(500);
}
