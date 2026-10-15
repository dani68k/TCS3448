/*!
 * @file Adafruit_TCS3448.cpp
 *
 * Arduino driver additions for the TCS3448 14-channel multi-spectral sensor.
 *
 * MIT license; see LICENSE for more information.
 */

#include "Adafruit_TCS3448.h"

/**
 * @brief Initialize the TCS3448 at its fixed I2C address.
 *
 * @param wire I2C interface to use.
 * @return True when identification and initialization succeed.
 */
bool Adafruit_TCS3448::begin(TwoWire* wire) {
  // GPIO is sampled 600 us after power-up to select the sensor I2C voltage.
  delay(1);
  return Adafruit_AS7343::begin(TCS3448_I2CADDR_DEFAULT, wire);
}

/**
 * @brief Force a TCS3448 power-on reset.
 *
 * @return True when the reset command succeeds and the sensor responds again.
 */
bool Adafruit_TCS3448::reset() {
  if (!i2c_dev || !setBank(false)) {
    return false;
  }

  Adafruit_BusIO_Register control =
      Adafruit_BusIO_Register(i2c_dev, TCS3448_CONTROL);
  Adafruit_BusIO_RegisterBits softwareReset =
      Adafruit_BusIO_RegisterBits(&control, 1, TCS3448_CONTROL_RESET_BIT);
  if (!softwareReset.write(1)) {
    return false;
  }

  // The sensor temporarily NAKs I2C while the power-on reset completes.
  delay(1);
  for (uint16_t elapsedMs = 0; elapsedMs < 250; elapsedMs++) {
    if (i2c_dev->detected()) {
      return true;
    }
    delay(1);
  }
  return false;
}

/**
 * @brief Query whether one complete spectral result is ready.
 *
 * @param ready Set true when STATUS2.AVALID is asserted.
 * @return True when the status register was read successfully.
 */
bool Adafruit_TCS3448::getDataReady(bool* ready) {
  if (!i2c_dev || !ready || !setBank(false)) {
    return false;
  }

  uint8_t status = 0;
  Adafruit_BusIO_Register status2 =
      Adafruit_BusIO_Register(i2c_dev, TCS3448_STATUS2);
  if (!status2.read(&status, 1)) {
    return false;
  }

  *ready = bitRead(status, TCS3448_STATUS2_AVALID_BIT);
  return true;
}

/**
 * @brief Read one coherent 6/12/18-result frame without waiting.
 *
 * @param data Destination for the concurrent gain, saturation, and counts.
 * @param channelCount Valid results in the selected auto-SMUX mode.
 * @return True when the complete register frame was read successfully.
 */
bool Adafruit_TCS3448::readData(tcs3448_data_t* data, uint8_t channelCount) {
  if (!i2c_dev || !data || (channelCount != 6 && channelCount != 12 && channelCount != 18) || !setBank(false)) {
    return false;
  }

  // Reading ASTATUS first latches the following channel bytes. BusIO may split
  // the consecutive read to fit Wire's buffer, but uses repeated starts with
  // no intervening stop. This keeps status and every channel concurrent as
  // required by the TCS3448 datasheet section 10.2.7.
  tcs3448_wire_frame_buffer_t raw = {};
  Adafruit_BusIO_Register resultFrame =
      Adafruit_BusIO_Register(i2c_dev, TCS3448_ASTATUS);
  if (!resultFrame.read(raw.bytes, 1 + channelCount * 2)) {
    return false;
  }

  data->gain = (tcs3448_gain_t)(raw.frame.status & TCS3448_ASTATUS_GAIN_MASK);
  data->saturated = bitRead(raw.frame.status, TCS3448_ASTATUS_SATURATION_BIT);
  for (uint8_t channel = 0; channel < TCS3448_CHANNEL_COUNT; channel++) {
    data->channels[channel] = (uint16_t)raw.frame.channels[channel].low |
                              ((uint16_t)raw.frame.channels[channel].high << 8);
  }
  return true;
}

bool Adafruit_TCS3448::readConfiguration(tcs3448_settings_t* settings) {
  if (!i2c_dev || !settings || !setBank(false)) return false;
  uint8_t gain, atime, astep[2], smux, az, led;
  if (!Adafruit_BusIO_Register(i2c_dev, AS7343_CFG1).read(&gain, 1) ||
      !Adafruit_BusIO_Register(i2c_dev, AS7343_ATIME).read(&atime, 1) ||
      !Adafruit_BusIO_Register(i2c_dev, AS7343_ASTEP_L).read(astep, 2) ||
      !Adafruit_BusIO_Register(i2c_dev, AS7343_CFG20).read(&smux, 1) ||
      !Adafruit_BusIO_Register(i2c_dev, AS7343_AZ_CONFIG).read(&az, 1) ||
      !Adafruit_BusIO_Register(i2c_dev, AS7343_LED).read(&led, 1)) return false;
  *settings = {static_cast<uint8_t>(gain & 0x1f), atime,
      static_cast<uint16_t>(astep[0] | (astep[1] << 8)),
      static_cast<uint8_t>((smux >> 5) & 3), az,
      static_cast<uint16_t>(4 + (led & 0x7f) * 2), (led & 0x80) != 0};
  return true;
}

bool Adafruit_TCS3448::writeConfiguration(const tcs3448_settings_t& settings) {
  if (!i2c_dev || !setBank(false)) return false;
  // The manager stops ALS before changing timing/CFG20; LED is enabled last.
  return enableLED(false) && setGain(static_cast<tcs3448_gain_t>(settings.gain)) &&
      // Avoid an intermediate ATIME=ASTEP=0 even while the engine is stopped.
      (settings.astep == 0 ? (setATIME(settings.atime) && setASTEP(settings.astep)) :
                             (setASTEP(settings.astep) && setATIME(settings.atime))) &&
      setSMUXMode(static_cast<tcs3448_smux_mode_t>(settings.smux)) &&
      setAutoZeroFrequency(settings.autozero) && setLEDCurrent(settings.ledCurrent) &&
      enableLED(settings.ledEnabled);
}

/**
 * @brief Read the sampled GPIO input with I2C error reporting.
 *
 * @param value Set to the sampled GPIO logic level.
 * @return True when the read and bank restore both succeed.
 */
bool Adafruit_TCS3448::getGPIOValue(bool* value) {
  if (!i2c_dev || !value || !setBank(true)) {
    return false;
  }

  uint8_t gpio = 0;
  Adafruit_BusIO_Register gpioRegister =
      Adafruit_BusIO_Register(i2c_dev, TCS3448_GPIO);
  bool readSucceeded = gpioRegister.read(&gpio, 1);
  bool restoreSucceeded = setBank(false);
  if (!readSucceeded || !restoreSucceeded) {
    return false;
  }

  *value = bitRead(gpio, TCS3448_GPIO_INPUT_BIT);
  return true;
}

/**
 * @brief Read the raw flicker status with I2C error reporting.
 *
 * @param status Set to the raw FD_STATUS register value.
 * @return True when the status register was read successfully.
 */
bool Adafruit_TCS3448::getFlickerStatus(uint8_t* status) {
  if (!i2c_dev || !status || !setBank(false)) {
    return false;
  }

  Adafruit_BusIO_Register flickerStatus =
      Adafruit_BusIO_Register(i2c_dev, TCS3448_FD_STATUS);
  return flickerStatus.read(status, 1);
}
