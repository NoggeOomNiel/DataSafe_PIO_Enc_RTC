#pragma once

#include <Arduino.h>

//  Defines
#define GMG12864_06D_DOGL128
#define MaxSites 250 // Max number of sites
// Number of fields stored per site in siteData array:
// SiteName, WebAddress, Detail1 (Login), Detail2 (User), Detail3 (Pass), Detail4 (Return), UsageCount
#define SITE_DATA_FIELDS_COUNT 7
// Number of selectable items on the details screen (WebAddress, Login, User, Pass, Return)
#define NUM_DETAIL_ITEMS_ON_SCREEN 5

// --- PIN Entry Definitions ---
#define DEFAULT_LOGIN_PIN "0000" // Default PIN if not found in NVM
#define PIN_DIGITS 4
#define MAX_PIN_ATTEMPTS 3
#define PIN_FILENAME "pin.dat" // File on SD card to store the encrypted PIN

// --- Encryption Constants ---
#define AES_KEY_SIZE 32   // 256 bits
#define AES_BLOCK_SIZE 16 // 128 bits
#define AES_IV_SIZE 16    // 128 bits
#define SALT_SIZE 16      // 128 bits
#define HMAC_SIZE 32      // SHA-256 HMAC
#define PBKDF2_ITERATIONS 10000
#define FILE_HEADER_MAGIC "DSv2"
#define FILE_HEADER_SIZE 4

#define DELETE_UNENCRYPTED_FILE true
#define BORDER_WIDTH 2 // Width of the screen border in pixels

// LCD pins
#ifdef GMG12864_06D_DOGL128
#define TFT_CS 5
#define TFT_RST 6
#define TFT_DC 1
#define TFT_BL 0
#define TFT_MOSI 3
#define TFT_CLK 2
// Index for usage count in siteData array (0-indexed, last column)
#define SITE_NAME_INDEX 0 // Index for site name in siteData array (0-indexed, first column)
#define USAGE_COUNT_COLUMN_INDEX (SITE_DATA_FIELDS_COUNT - 1)
#endif

// Button Pins
#define ENTERBUTTON_PIN D16 // push button on encoder
#define PRINTBUTTON_PIN D20 // Auxiliary button (Yellow)

// EC11 rot encoder pins
#define ROTARY_PIN_A D17 // enc pin a
#define ROTARY_PIN_B D18 // enc pin b

// Buzzer pin
#define Buzzer_PWM_PIN D19

// RGB LED Pins (GPIO7, GPIO8, GPIO9) - High is ON
#define LED_R_PIN 7
#define LED_G_PIN 8
#define LED_B_PIN 9

// SD Card Pins
#define SD_CS_PIN 13                // SD Card Chip Select pin
#define SD_FILENAME "passwords.csv" // File on SD card to read string from

// default for SPI1 on Pico
#define SD_SPI1_SCK_PIN 10  // SPI1 SCK - Connect to SD Card SCK (CLK)
#define SD_SPI1_MOSI_PIN 11 // SPI1 TX - Connect to SD Card MOSI (DI)
#define SD_SPI1_MISO_PIN 12 // SPI1 RX - Connect to SD Card MISO (DO)

// --- LED Color Definitions ---
// RGB Values (PWM levels)
#define RGB_OFF 0, 0, 0
#define RGB_RED 100, 0, 0
#define RGB_GREEN 0, 100, 0
#define RGB_BLUE 0, 0, 100
#define RGB_YELLOW 100, 100, 0
#define RGB_ORANGE 100, 53, 0
#define RGB_CYAN 0, 100, 100
#define RGB_MAGENTA 100, 0, 100
#define RGB_WHITE 100, 100, 100

// LED States (Enum values for switch cases)
enum LedState
{
  LED_STATE_OFF,
  LED_STATE_RED,
  LED_STATE_GREEN,
  LED_STATE_BLUE,
  LED_STATE_YELLOW,
  LED_STATE_ORANGE,
  LED_STATE_CYAN,
  LED_STATE_MAGENTA,
  LED_STATE_WHITE
};

#define INACTIVITY_TIMEOUT_MS (15 * 60 * 1000UL) // 15 mins // 30000 // 30 seconds
// #define INACTIVITY_TIMEOUT_MS (1 * 60 * 1000UL) // 1 min

// Enums
enum ScreenState
{
  SCREEN_HOME,
  SCREEN_DETAILS,
  SCREEN_CLOCK
};
