/*!
 * @file Adafruit_TCS3448.h
 *
 * @mainpage Adafruit TCS3448 14-Channel Multi-Spectral Sensor
 *
 * @section intro_sec Introduction
 *
 * Arduino driver for the TCS3448 14-channel multi-spectral sensor.
 *
 * The TCS3448 shares its register map and automatic SMUX channel order with
 * the AS7343. This class subclasses Adafruit_AS7343 and selects the TCS3448
 * I2C address.
 *
 * @section dependencies Dependencies
 *
 * This library depends on the Adafruit AS7343 library.
 *
 * @section author Author
 *
 * Limor "Ladyada" Fried with assistance from OpenAI Codex
 *
 * @section license License
 *
 * MIT license; see LICENSE for more information.
 */

#ifndef ADAFRUIT_TCS3448_H
#define ADAFRUIT_TCS3448_H

#include <Adafruit_AS7343.h>

/** Default 7-bit I2C address for the TCS3448. */
#define TCS3448_I2CADDR_DEFAULT 0x59

/** Expected value of the TCS3448 part-identification register. */
#define TCS3448_CHIP_ID 0x81

/** Number of result values returned by 18-channel automatic SMUX mode. */
#define TCS3448_CHANNEL_COUNT 18

/** STATUS2 register containing the data-ready flag. */
#define TCS3448_STATUS2 AS7343_STATUS2

/** ASTATUS register at the start of a coherent result frame. */
#define TCS3448_ASTATUS AS7343_ASTATUS

/** CONTROL register containing the software-reset bit. */
#define TCS3448_CONTROL AS7343_CONTROL

/** GPIO register containing the sampled GPIO input. */
#define TCS3448_GPIO AS7343_GPIO

/** Flicker-detection status register. */
#define TCS3448_FD_STATUS AS7343_FD_STATUS

/** Data-ready bit number in STATUS2. */
#define TCS3448_STATUS2_AVALID_BIT 6

/** Saturation bit number in ASTATUS. */
#define TCS3448_ASTATUS_SATURATION_BIT 7

/** Mask for the gain code reported in ASTATUS. */
#define TCS3448_ASTATUS_GAIN_MASK 0x0F

/** Software-reset bit number in CONTROL. */
#define TCS3448_CONTROL_RESET_BIT 3

/** Sampled input bit number in GPIO. */
#define TCS3448_GPIO_INPUT_BIT 0

/** TCS3448 gain setting. */
typedef as7343_gain_t tcs3448_gain_t;

/** TCS3448 automatic SMUX mode. */
typedef as7343_smux_mode_t tcs3448_smux_mode_t;

/** TCS3448 result-channel index. */
typedef as7343_channel_t tcs3448_channel_t;

/** TCS3448 flicker-detection result. */
typedef as7343_flicker_t tcs3448_flicker_t;

/** 0.5x spectral gain. */
#define TCS3448_GAIN_0_5X AS7343_GAIN_0_5X
/** 1x spectral gain. */
#define TCS3448_GAIN_1X AS7343_GAIN_1X
/** 2x spectral gain. */
#define TCS3448_GAIN_2X AS7343_GAIN_2X
/** 4x spectral gain. */
#define TCS3448_GAIN_4X AS7343_GAIN_4X
/** 8x spectral gain. */
#define TCS3448_GAIN_8X AS7343_GAIN_8X
/** 16x spectral gain. */
#define TCS3448_GAIN_16X AS7343_GAIN_16X
/** 32x spectral gain. */
#define TCS3448_GAIN_32X AS7343_GAIN_32X
/** 64x spectral gain. */
#define TCS3448_GAIN_64X AS7343_GAIN_64X
/** 128x spectral gain. */
#define TCS3448_GAIN_128X AS7343_GAIN_128X
/** 256x spectral gain. */
#define TCS3448_GAIN_256X AS7343_GAIN_256X
/** 512x spectral gain. */
#define TCS3448_GAIN_512X AS7343_GAIN_512X
/** 1024x spectral gain. */
#define TCS3448_GAIN_1024X AS7343_GAIN_1024X
/** 2048x spectral gain. */
#define TCS3448_GAIN_2048X AS7343_GAIN_2048X

/** Six-result automatic SMUX mode. */
#define TCS3448_SMUX_6CH AS7343_SMUX_6CH
/** Twelve-result automatic SMUX mode. */
#define TCS3448_SMUX_12CH AS7343_SMUX_12CH
/** Eighteen-result automatic SMUX mode. */
#define TCS3448_SMUX_18CH AS7343_SMUX_18CH

/** FZ channel result index. */
#define TCS3448_CHANNEL_FZ AS7343_CHANNEL_FZ
/** FY channel result index. */
#define TCS3448_CHANNEL_FY AS7343_CHANNEL_FY
/** FXL channel result index. */
#define TCS3448_CHANNEL_FXL AS7343_CHANNEL_FXL
/** Near-infrared channel result index. */
#define TCS3448_CHANNEL_NIR AS7343_CHANNEL_NIR
/** First-cycle top-left clear photodiode result index. */
#define TCS3448_CHANNEL_VIS_TL_0 AS7343_CHANNEL_VIS_TL_0
/** First-cycle both-right clear photodiode result index. */
#define TCS3448_CHANNEL_VIS_BR_0 AS7343_CHANNEL_VIS_BR_0
/** F2 channel result index. */
#define TCS3448_CHANNEL_F2 AS7343_CHANNEL_F2
/** F3 channel result index. */
#define TCS3448_CHANNEL_F3 AS7343_CHANNEL_F3
/** F4 channel result index. */
#define TCS3448_CHANNEL_F4 AS7343_CHANNEL_F4
/** F6 channel result index. */
#define TCS3448_CHANNEL_F6 AS7343_CHANNEL_F6
/** Second-cycle top-left clear photodiode result index. */
#define TCS3448_CHANNEL_VIS_TL_1 AS7343_CHANNEL_VIS_TL_1
/** Second-cycle both-right clear photodiode result index. */
#define TCS3448_CHANNEL_VIS_BR_1 AS7343_CHANNEL_VIS_BR_1
/** F1 channel result index. */
#define TCS3448_CHANNEL_F1 AS7343_CHANNEL_F1
/** F7 channel result index. */
#define TCS3448_CHANNEL_F7 AS7343_CHANNEL_F7
/** F8 channel result index. */
#define TCS3448_CHANNEL_F8 AS7343_CHANNEL_F8
/** F5 channel result index. */
#define TCS3448_CHANNEL_F5 AS7343_CHANNEL_F5
/** Third-cycle top-left clear photodiode result index. */
#define TCS3448_CHANNEL_VIS_TL_2 AS7343_CHANNEL_VIS_TL_2
/** Third-cycle both-right clear photodiode result index. */
#define TCS3448_CHANNEL_VIS_BR_2 AS7343_CHANNEL_VIS_BR_2

/** No 100 Hz or 120 Hz flicker detected. */
#define TCS3448_FLICKER_NONE AS7343_FLICKER_NONE
/** 100 Hz flicker detected. */
#define TCS3448_FLICKER_100HZ AS7343_FLICKER_100HZ
/** 120 Hz flicker detected. */
#define TCS3448_FLICKER_120HZ AS7343_FLICKER_120HZ

/**
 * @brief One coherent TCS3448 18-result measurement.
 *
 * The channel values are raw ADC counts in automatic SMUX storage order. Use
 * the TCS3448_CHANNEL_* constants as indices. They are not lux, irradiance, or
 * gain-normalized values.
 */
typedef struct {
  uint16_t channels[TCS3448_CHANNEL_COUNT]; ///< Raw 16-bit channel counts.
  tcs3448_gain_t gain; ///< Gain reported for this exact result frame.
  bool saturated;      ///< True when this result frame was saturated.
} tcs3448_data_t;

/** One little-endian channel value in the sensor's result register layout. */
#pragma pack(push, 1)
typedef struct {
  uint8_t low;  ///< Least-significant data byte.
  uint8_t high; ///< Most-significant data byte.
} tcs3448_wire_channel_t;

/** Complete register layout read consecutively from ASTATUS through DATA_H. */
typedef struct {
  uint8_t status; ///< Concurrent saturation and gain status.
  tcs3448_wire_channel_t channels[TCS3448_CHANNEL_COUNT]; ///< Channel bytes.
} tcs3448_wire_frame_t;
#pragma pack(pop)

/** Byte and field views of one coherent result-register transaction. */
typedef union {
  tcs3448_wire_frame_t frame; ///< Named status and channel fields.
  uint8_t bytes[1 + (TCS3448_CHANNEL_COUNT * 2)]; ///< Raw I2C transfer buffer.
} tcs3448_wire_frame_buffer_t;

static_assert(sizeof(tcs3448_wire_frame_t) == 37,
              "TCS3448 result frame must be 37 bytes");

/**
 * @brief Class for the TCS3448 14-channel multi-spectral sensor.
 *
 * The inherited Adafruit_AS7343 configuration methods apply because the two
 * devices share the relevant register addresses and bit layouts. TCS3448-named
 * type and value aliases are provided above for sketches that should not need
 * AS7343-prefixed constants.
 */
class Adafruit_TCS3448 : public Adafruit_AS7343 {
 public:
  Adafruit_TCS3448() = default;

  using Adafruit_AS7343::getFlickerStatus;
  using Adafruit_AS7343::getGPIOValue;

  /**
   * @brief Prevent copying an object that owns an I2C device.
   * @param other Instance that must not be copied.
   */
  Adafruit_TCS3448(const Adafruit_TCS3448& other) = delete;

  /**
   * @brief Prevent assigning an object that owns an I2C device.
   * @param other Instance that must not be assigned.
   * @return Not callable; this operator is deleted.
   */
  Adafruit_TCS3448& operator=(const Adafruit_TCS3448& other) = delete;

  /**
   * @brief Initialize the TCS3448 at its fixed I2C address.
   *
   * @param wire I2C interface to use.
   * @return True when the sensor responds with the expected part ID and the
   * initialization sequence succeeds.
   */
  bool begin(TwoWire* wire = &Wire);

  /**
   * @brief Force a TCS3448 power-on reset through the CONTROL register.
   *
   * Configuration returns to reset defaults. Call begin() afterward to restore
   * the library defaults.
   *
   * @return True when the reset command was written successfully.
   */
  bool reset();

  /**
   * @brief Query whether a complete spectral result is ready.
   *
   * This method performs one register read and does not wait.
   *
   * @param ready Set true when a complete result is ready.
   * @return True when the status register was read successfully.
   */
  bool getDataReady(bool* ready);

  /**
   * @brief Read one coherent 18-result frame without waiting.
   *
   * Call getDataReady() first. This method addresses ASTATUS once and reads all
   * 36 following result bytes consecutively, so the gain, saturation flag, and
   * ADC counts describe the same measurement. On Wire implementations with a
   * buffer smaller than 37 bytes, Adafruit BusIO continues the read in chunks
   * with repeated starts and no intervening stop. The sensor must be in
   * TCS3448_SMUX_18CH mode.
   *
   * @param data Destination for the raw result frame.
   * @return True when the complete frame was read successfully.
   */
  bool readData(tcs3448_data_t* data);

  /**
   * @brief Read the sampled GPIO input with I2C error reporting.
   *
   * @param value Set to the sampled GPIO logic level.
   * @return True when the GPIO register and bank restore both succeed.
   */
  bool getGPIOValue(bool* value);

  /**
   * @brief Read the raw flicker status with I2C error reporting.
   *
   * @param status Set to the raw FD_STATUS register value.
   * @return True when the status register was read successfully.
   */
  bool getFlickerStatus(uint8_t* status);
};

#endif
