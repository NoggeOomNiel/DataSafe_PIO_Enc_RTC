#pragma once

#include <Arduino.h>
#include "DOGL128.h"
#include <SPI.h> // Required for SD card
#include <SD.h>  // Required for SD card
#include <Keyboard.h>
#include <atomic>
#include "quadrature_encoder.pio.h" //encoder lib using PIO block state machine
// #include "quadrature_encoder_substep.pio.h" //encoder lib using PIO block state machine
#include "hardware/rtc.h"           // For Pico RTC functions
#include "pico/util/datetime.h"     // For datetime_t structure
//#include <time.h>                   // For time_t, tm, gmtime_r
#include "ctime"
//#include <algorithm> // For std::swap
//#include <stdio.h>   // For sprintf
#include "cstdio"
#include "fonts/font_Arial_12x20_32-127_DOGL.h"
#include <vector>
#include <Wire.h> // I2C library
#include "ds3231.h" // For DS3231 RTC module

//  Defines
#define GMG12864_06D_DOGL128
#define MaxSites 250       // Max number of sites
// Number of fields stored per site in siteData array:
// SiteName, WebAddress, Detail1 (Login), Detail2 (User), Detail3 (Pass), Detail4 (Return), UsageCount
#define SITE_DATA_FIELDS_COUNT 7
// Number of selectable items on the details screen (WebAddress, Login, User, Pass, Return)
#define NUM_DETAIL_ITEMS_ON_SCREEN 5

// --- PIN Entry Definitions ---
//#define LOGIN_PIN "1111" // Change this to your desired 4-digit PIN
//#define DEFAULT_LOGIN_PIN "1111" // Default PIN if not found in NVM
#define DEFAULT_LOGIN_PIN "0000" // Default PIN if not found in NVM
#define PIN_DIGITS 4
#define MAX_PIN_ATTEMPTS 3
#define PIN_FILENAME "pin.dat"   // File on SD card to store the encrypted PIN
// --- End PIN Entry Definitions ---

#define ENCRYPTION_KEY 0xA5 // Simple XOR encryption key
#define ROTATION_COUNT 3    // Bit rotations
#define DELETE_UNENCRYPTED_FILE true
#define BORDER_WIDTH 2      // Width of the screen border in pixels

#ifdef GMG12864_06D_DOGL128 // LCD pins************************** */
#define TFT_CS 5
#define TFT_RST 6
#define TFT_DC 1
#define TFT_BL 0
#define TFT_MOSI 3
#define TFT_CLK 2
// Index for usage count in siteData array (0-indexed, last column)
#define SITE_NAME_INDEX 0 // Index for site name in siteData array (0-indexed, first column)
#define USAGE_COUNT_COLUMN_INDEX (SITE_DATA_FIELDS_COUNT - 1)
#endif //GMG12864_06D_DOGL128

// Button Pins
#define ENTERBUTTON_PIN D16 // push button on encoder
#define PRINTBUTTON_PIN D20 // type out decrypted text on keyboard

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

#ifdef GMG12864_06D_DOGL128 //******************************** */
// Use MicroElectronika Font Creator app to generate fonts
// #include "fonts\Small_7.h"
// #include "fonts\DOGL_CONSOLAS_6x11.h"
// #include "fonts\DOGL_CONSOLAS_7x13.h"
//#include "fonts\DOGL_font5x7.h"
inline DOGL128 LCD(TFT_MOSI, TFT_CLK, TFT_CS, TFT_DC, TFT_RST, "LCD");
#endif
//************************************************************ */

// Enums
enum ScreenState
{
    SCREEN_HOME,
    SCREEN_DETAILS,
    SCREEN_CLOCK
};



// Variables

// --- Inactivity Timeout Feature ---
inline unsigned long lastActivityTime = 0;
// Set to 30 minutes (30 * 60 * 1000 ms). Use a shorter duration for testing, e.g., 15000UL for 15s.
#define INACTIVITY_TIMEOUT_MS (30 * 60 * 1000UL) //30 mins
//#define INACTIVITY_TIMEOUT_MS (1 * 60 * 1000UL) //1 min
//#define INACTIVITY_TIMEOUT_MS (15000UL) //15 sec

//extern String currentLoginPin; // Holds the current PIN, loaded from NVM
inline String currentLoginPin = DEFAULT_LOGIN_PIN;      // Initialize with default, will be overwritten by NVM load
inline std::atomic<bool> blueLedPersistentState(false); // True if blue LED should be on by default
inline bool backlightOffDueToTimeout = false;           // True if backlight was turned off by inactivity timeout


// Define the magic string that triggers the special auto-login sequence.
// This should match the content of siteData[positionHold][1] for sites where this mode is desired.
const String AUTO_LOGIN_TRIGGER_STRING = F("Login...");
const String RETURN_STRING = F("Return...");

//String array for days of the week
inline const char*  daysOfWeek[7] = {"Sunday","Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
//Char array for months of the year
inline const char* monthsOfYear[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", " Oct", "Nov", "Dec"};

// PIO encoder
constexpr uint sm = 0;
inline auto pio = pio0;
// std::atomic<int> newEncPos(0);
inline std::atomic<int> newPos(0);
inline std::atomic<boolean> resetPos(false);
inline std::atomic<boolean> blipTone(false);


// SD card Data array
inline String siteData[MaxSites][SITE_DATA_FIELDS_COUNT];
inline int positionHold = 0;

inline boolean timePosFound = false;
inline int header_x_right_time;
inline unsigned long lastHeaderUpdateTime = 0;
inline ScreenState currentScreen = SCREEN_HOME;
inline std::atomic<int> detailSelectedItemIndex(0);    // 0-3 for the 4 data items on details screen
inline std::atomic<bool> detailViewNeedsRedraw(false); // Flag to signal redraw for detail view

//  Button states
constexpr unsigned long debounceDelay = 250;
inline std::atomic<bool> printButtonPressed(false);
inline std::atomic<bool> enterButtonPressed(false);

// A flag to ensure usage is incremented only once per "used" action
inline bool itemWasUsedAction = false;

// Time screen refresh interval (ms)
static constexpr unsigned long TIME_SCREEN_REFRESH_MS = 500;
static unsigned long s_last_time_update = 0;
static String s_last_drawn_time = "";

inline datetime_t dt;


// Functions
void resetInactivityTimer();
void drawScreenBorder();
void doUsageIncrement();
void sendBlipTone(uint32_t freq, int ratio, uint32_t duration);
void drawBootScreen();
void drawHomeScreen(int startIndex);
void drawDetailsScreen();
void ButtonPressISR();
bool SDFile_to_Array(const char *baseFilePath); // Returns true if data was loaded/initialized
void sendStringAsKeystrokes(const String &textToSend);
void doCore1Tasks();
uint8_t rotateLeft(uint8_t value, int shift);
uint8_t rotateRight(uint8_t value, int shift);
bool fileEncrypt(const char *sourceFilename, const char *destEncryptedFilename, bool deleteSource);
bool saveSiteDataToEncryptedFile(const char *baseFilePath);
void sortSiteData();
void streamOut();
void streamIn();
void typeOut();
void reset_pio_y_register(PIO pio_instance, uint sm_instance);
bool syncRTCWithDS3231();
bool syncRTCWithHost();
bool unix_to_datetime(time_t ts, datetime_t *_dt);
bool handlePinEntry(); // Function to manage PIN input
bool askToUpdatePin(); // Asks user if they want to update the PIN
bool performPinUpdateProcess(); // Handles the new PIN entry and saving
bool getPinFromUser(const char* promptMessage, String& outPin); // Helper to get 4 digits
bool savePinToNVM(const String& pinToSave); // Saves PIN to SD card
bool loadPinFromNVM(); // Loads PIN from SD card
void updateHomeScreenHeaderTime();
//int calculate_string_pixel_width(const char* text, DOGL128 &lcd_instance);
String truncateStringToFit(const String& text, int maxPixelWidth, const DOGL128& lcd, const String& suffix = "...");

// LED Control Functions
void setLedColor(int r, int g, int b);
void blinkLed(int r_blink, int g_blink, int b_blink, unsigned long duration);
//void updatePersistentLedState(); // Sets LED based on current screen and sync status
void drawTimeScreen();
static int centerXForText(const char* text);
void doClockDisplay();

//extern std::atomic<bool> blueLedPersistentState; // True if blue LED should be on by default

// --- LED Color Definitions (HIGH is ON) ---
// Note: For simple digital control (HIGH/LOW), Orange will appear as Yellow.
#define LED_STATE_OFF   0,0,0         // PWM level 0 for off
#define LED_STATE_RED   100,0,0       // PWM level 100 for Red
#define LED_STATE_GREEN 0,100,0       // PWM level 100 for Green
#define LED_STATE_BLUE  0,0,100       // PWM level 100 for Blue
#define LED_STATE_YELLOW 100,100,0    // Red + Green at PWM 100
#define LED_STATE_ORANGE 100,53,0    // Red + Green at PWM 100 (same as Yellow)
#define LED_STATE_WHITE 100,100,100   // All channels at PWM 100

// --- End LED Color Definitions ---

//  Serial Command Handling
void handleSerialCommands();
//void doCSVitemAdd();
