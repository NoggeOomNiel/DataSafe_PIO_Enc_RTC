#pragma once

#include <Arduino.h>
#include "DOGL128.h"
#include "Constants.h"
#include "pico/util/datetime.h"
#include <atomic>
#include <string>

class UiManager
{
public:
  /** @brief Constructor - initializes DOGL128 LCD with pin configuration */
  UiManager();

  /** @brief Initializes the LCD display with default settings */
  void begin();

  // Screen drawing methods
  /** @brief Draws the initial boot screen with title */
  void drawBootScreen();

  /** @brief Draws the home screen showing scrollable password list */
  void drawHomeScreen(int startIndex, const std::string siteData[][SITE_DATA_FIELDS_COUNT]);

  /** @brief Draws the details screen showing fields of a password entry */
  void drawDetailsScreen(int positionHold, int selectedItemIndex, const std::string siteData[][SITE_DATA_FIELDS_COUNT]);

  /** @brief Draws a large clock display */
  void drawTimeScreen(const datetime_t &dt);

  /** @brief Draws a 2-pixel wide border around the screen */
  void drawScreenBorder();

  // Helper methods
  /** @brief Clears the screen buffer */
  void clear();

  /** @brief Displays a message dialog with up to 3 centered text lines */
  void showMessage(const std::string &line1, const std::string &line2 = "", const std::string &line3 = "");

  /** @brief Shows PIN entry UI (used internally by SecurityManager) */
  void showPinEntry(const char *prompt, const char *enteredPin, int currentDigitIndex, int cursorXOffset);

  /** @brief Updates only the time portion in the home screen header */
  void updateHomeScreenHeaderTime(const datetime_t &dt);

  // Accessors
  /** @brief Returns reference to the LCD object */
  DOGL128 &getLCD() { return lcd; }

  // Utilities
  /** @brief Truncates a string to fit within a pixel width */
  std::string truncateStringToFit(const std::string &text, int maxPixelWidth, const std::string &suffix = "...");

  /** @brief Calculates X coordinate to horizontally center text */
  int centerXForText(const char *text);

  /** @brief Calculates Y coordinate to vertically center text */
  int centerYForText(const char *text);

  /** @brief Calculates X position after a specific character index */
  int calcNextXPos(const char *text, int index);

  /** @brief Returns the pixel width of a text string */
  int getStringPxLen(const char *text);

  /** @brief Returns the height of the current font */
  int getFontHeight();

  /** @brief Returns the LCD screen width */
  int getWidth();

  /** @brief Returns the LCD screen height */
  int getHeight();

  // LED Control
  /** @brief Sets RGB LED color using PWM values */
  void setLedColor(int r, int g, int b);

  /** @brief Sets RGB LED color using predefined color state */
  void setLedColor(int colorState); // Helper for predefined states

  /** @brief Blinks the LED with specified color and duration */
  void blinkLed(int colorState, unsigned long duration);

  // Font Control
  /** @brief Sets font to Small_7 (default small font) */
  void setFontNormal();

  /** @brief Sets font to Consolas 7x13 (monospace font for PIN entry) */
  void setFontConsolas();

  /** @brief Returns pointer to current font array */
  unsigned char *getCurrentFont();

  /** @brief Restores a previously saved font */
  void restoreFont(unsigned char *font);

  /** @brief Enables or disables monospace rendering mode */
  void setMonospace(bool enable);

  // Drawing Primitives
  /** @brief Sets the text cursor position for next print operation */
  void locate(int x, int y);

  /** @brief Prints text at current cursor position */
  void print(const std::string &text);

  /** @brief Draws a filled rectangle on the screen */
  void fillrect(int x1, int y1, int x2, int y2, int pattern);

  /** @brief Copies the screen buffer to the physical LCD display */
  void copyToLcd();

private:
  DOGL128 lcd;
  bool timePosFound = false;
  int header_x_right_time = 0;

  // For drawTimeScreen
  unsigned char *originalFont = nullptr;
  char weekdays[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
};
