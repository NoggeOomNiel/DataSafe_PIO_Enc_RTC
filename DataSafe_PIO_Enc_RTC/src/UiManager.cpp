#include "UiManager.h"
#include <string>
#include "fonts/font_Arial_12x20_32-127_DOGL.h"
#include <cstdio>

extern const unsigned char Small_7[];
extern const unsigned char DOGL_Consolas7x13[];
// extern const unsigned char DOGL_Arial12x20[];

/**
 * @brief Constructor - initializes DOGL128 LCD with pin configuration
 */
UiManager::UiManager() : lcd(TFT_MOSI, TFT_CLK, TFT_CS, TFT_DC, TFT_RST, "LCD")
{
}

/**
 * @brief Initializes the LCD display with default settings
 */
void UiManager::begin()
{
  lcd.begin();
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  lcd.cls();
  lcd.set_auto_up(0);
  lcd.invert(0);
  lcd.set_contrast(20);
}

/**
 * @brief Clears the screen buffer
 */
void UiManager::clear()
{
  lcd.cls();
}

/**
 * @brief Draws a 2-pixel wide border around the screen
 */
void UiManager::drawScreenBorder()
{
  const int screen_height = lcd.height();
  lcd.rect(0, 0, lcd.width() - 1, screen_height - 1, 1);
  lcd.rect(1, 1, lcd.width() - 2, screen_height - 2, 1);
}

/**
 * @brief Draws the initial boot screen with "Password Safe" title
 */
void UiManager::drawBootScreen()
{
  lcd.cls();
  drawScreenBorder();
  constexpr int content_padding = 2;
  constexpr int text_x = BORDER_WIDTH + content_padding;
  constexpr int text_y = BORDER_WIDTH + content_padding;
  lcd.locate(text_x, text_y);
  lcd.print("     Password Safe");
  lcd.line(BORDER_WIDTH, text_y + 7 + 2, lcd.width() - 1 - BORDER_WIDTH, text_y + 7 + 2, 1);
  lcd.copy_to_lcd();
}

/**
 * @brief Draws the home screen showing a scrollable list of password entries
 *
 * Displays up to 4 entries starting from startIndex. Selected entry is marked with ">".
 *
 * @param startIndex Index of first entry to display
 * @param siteData 2D array containing all password entries
 */
void UiManager::drawHomeScreen(int startIndex, const std::string siteData[][SITE_DATA_FIELDS_COUNT])
{
  lcd.cls();
  drawScreenBorder();
  constexpr int content_padding = 2;
  constexpr int header_x = BORDER_WIDTH + content_padding;
  constexpr int header_y = BORDER_WIDTH + content_padding;
  constexpr int item_start_y = header_y + 7 + 3;
  constexpr int item_line_height = 7 + 3;

  lcd.locate(header_x, header_y);
  lcd.print(F("Select..."));

  // Time is drawn by updateHomeScreenHeaderTime or caller

  for (int i = 0; i < 4; i++)
  {
    lcd.locate(header_x, item_start_y + (i * item_line_height));
    int dataIndex = startIndex + i;
    if (dataIndex >= 0 && dataIndex < MaxSites)
    {
      std::string siteName = siteData[dataIndex][0];
      if (siteName.length() > 0)
      {
        if (i == 0)
          lcd.print("> ");

        if (siteName.length() > 24)
        {
          siteName = siteName.substr(0, 22) + "...";
        }
        lcd.print(siteName.c_str());
      }
    }
    else
    {
      lcd.print("                ");
    }
  }
  lcd.copy_to_lcd();
}

/**
 * @brief Draws the details screen showing fields of a selected password entry
 *
 * Displays site name as header, then 5 selectable fields (URL, Login, User, Pass, Return).
 * Selected field is marked with ">".
 *
 * @param positionHold Index of the password entry to display
 * @param selectedItemIndex Index of the currently selected field (0-4)
 * @param siteData 2D array containing all password entries
 */
void UiManager::drawDetailsScreen(int positionHold, int selectedItemIndex, const std::string siteData[][SITE_DATA_FIELDS_COUNT])
{
  lcd.cls();
  drawScreenBorder();
  constexpr int content_padding = 2;
  constexpr int text_x = BORDER_WIDTH + content_padding;
  constexpr int site_name_y = BORDER_WIDTH + content_padding;
  constexpr int item_start_y = site_name_y + 7 + 2;
  constexpr int item_line_height = 7 + 2;
  const int site_name_x = centerXForText(siteData[positionHold][0].c_str());

  lcd.locate(site_name_x, site_name_y);
  lcd.print(siteData[positionHold][0].c_str());

  for (int i = 0; i < NUM_DETAIL_ITEMS_ON_SCREEN; i++)
  {
    lcd.locate(text_x, item_start_y + (i * item_line_height));
    const char *prefix_str = (i == selectedItemIndex) ? "> " : "  ";
    lcd.print(prefix_str);

    std::string itemText = siteData[positionHold][i + 1];

    if (i == 0)
    { // WebAddress
      std::string displayableItemText = itemText;
      if (displayableItemText.find("https://www.") == 0)
        displayableItemText = displayableItemText.substr(12);
      else if (displayableItemText.find("http://www.") == 0)
        displayableItemText = displayableItemText.substr(11);
      else if (displayableItemText.find("https://") == 0)
        displayableItemText = displayableItemText.substr(8);
      else if (displayableItemText.find("http://") == 0)
        displayableItemText = displayableItemText.substr(7);
      else if (displayableItemText.find("www.") == 0)
        displayableItemText = displayableItemText.substr(4);

      int prefix_width = static_cast<int>(lcd.getStringPxLen(prefix_str));
      int available_width = lcd.width() - BORDER_WIDTH - text_x - prefix_width - (BORDER_WIDTH + content_padding);
      lcd.print(truncateStringToFit(displayableItemText, available_width).c_str());
    }
    else
    {
      lcd.print(itemText.c_str());
    }
  }
  lcd.copy_to_lcd();
}

/**
 * @brief Draws a large clock display showing current time and date
 *
 * Uses large Arial font for time (centered) and smaller Consolas font for date.
 *
 * @param dt datetime_t structure containing current date/time
 */
void UiManager::drawTimeScreen(const datetime_t &dt)
{
  lcd.cls();

  // Store current font to restore later if needed, though we set it explicitly below
  originalFont = lcd.font;

  // Use a large font for time
  lcd.set_font((unsigned char *)DOGL_Arial12x20);
  lcd.setMonospace(true);
  lcd.cls();
  drawScreenBorder();

  char buf[20];
  sprintf(buf, "%02u:%02u:%02u", dt.hour, dt.min, dt.sec);

  int x = centerXForText(buf);
  int y = (lcd.height() - lcd.font[2]) / 2 - 5; // Center vertically roughly

  lcd.locate(x, y);
  lcd.print(buf);
  lcd.setMonospace(false);

  // Restore font
  if (originalFont)
    lcd.set_font(originalFont);

  // Draw date below
  char buf_date[20];
  sprintf(buf_date, "%02u.%02u.%04u", dt.day, dt.month, dt.year);
  x = centerXForText(buf_date);
  lcd.locate(x, lcd.height() - 5 - lcd.font[2]);
  lcd.print(buf_date);

  // Draw day of the week
  sprintf(buf_date, "%s", weekdays[dt.dotw]);
  x = centerXForText(buf_date);
  lcd.locate(x, lcd.height() - 5 - (lcd.font[2] * 2));
  lcd.print(buf_date);

  lcd.copy_to_lcd();
}

/**
 * @brief Updates only the time portion in the home screen header (efficient partial update)
 *
 * @param dt datetime_t structure containing current time
 */
void UiManager::updateHomeScreenHeaderTime(const datetime_t &dt)
{
  constexpr int content_padding = 2;
  constexpr int header_x = BORDER_WIDTH + content_padding;
  constexpr int header_y = BORDER_WIDTH + content_padding;

  char time_buf[10];
  sprintf(time_buf, "%02d:%02d:%02d", dt.hour, dt.min, dt.sec);

  lcd.locate(header_x, header_y);
  lcd.print(F("Select..."));

  lcd.setMonospace(true);
  if (!timePosFound)
  {
    int time_str_width = static_cast<int>(lcd.getStringPxLen(time_buf));
    header_x_right_time = lcd.width() - BORDER_WIDTH - content_padding - time_str_width;
    timePosFound = true;
  }
  lcd.locate(header_x_right_time, header_y);
  lcd.print(time_buf);
  lcd.copy_to_lcd();
  lcd.setMonospace(false);
}

/**
 * @brief Displays a message dialog with up to 3 centered text lines
 *
 * @param line1 First line of text
 * @param line2 Second line of text (optional)
 * @param line3 Third line of text (optional)
 */
void UiManager::showMessage(const std::string &line1, const std::string &line2, const std::string &line3)
{
  lcd.cls();
  drawScreenBorder();
  int y = BORDER_WIDTH + 5;

  if (line1.length() > 0)
  {
    lcd.locate(centerXForText(line1.c_str()), y);
    lcd.print(line1.c_str());
  }
  if (line2.length() > 0)
  {
    lcd.locate(centerXForText(line2.c_str()), y + 10);
    lcd.print(line2.c_str());
  }
  if (line3.length() > 0)
  {
    lcd.locate(centerXForText(line3.c_str()), y + 20);
    lcd.print(line3.c_str());
  }
  lcd.copy_to_lcd();
}

/**
 * @brief Truncates a string to fit within a pixel width using current font
 *
 * Adds suffix (e.g., "...") if truncation is needed.
 *
 * @param text Text to truncate
 * @param maxPixelWidth Maximum width in pixels
 * @param suffix Suffix to append if truncated (default "...")
 * @return Truncated string with suffix if needed
 */
std::string UiManager::truncateStringToFit(const std::string &text, int maxPixelWidth, const std::string &suffix)
{
  const unsigned char *current_font = lcd.font;
  if (!current_font)
    return text;

  if (static_cast<int>(lcd.getStringPxLen(text.c_str())) <= maxPixelWidth)
  {
    return text;
  }

  int suffixWidth = static_cast<int>(lcd.getStringPxLen(suffix.c_str()));
  if (suffixWidth >= maxPixelWidth)
    return suffix;

  std::string resultString = "";
  int currentResultWidth = 0;
  for (unsigned int i = 0; i < text.length(); ++i)
  {
    char singleCharStr[2] = {text[i], '\0'};
    int char_width = static_cast<int>(lcd.getStringPxLen(singleCharStr));
    if (currentResultWidth + char_width + suffixWidth <= maxPixelWidth)
    {
      resultString += text[i];
      currentResultWidth += char_width;
    }
    else
    {
      break;
    }
  }
  return resultString + suffix;
}

/**
 * @brief Calculates X coordinate to horizontally center text on screen
 *
 * @param text Text to center
 * @return X coordinate for centered text
 */
int UiManager::centerXForText(const char *text)
{
  unsigned int w = lcd.getStringPxLen(text);
  return (lcd.width() - static_cast<int>(w)) / 2;
}

/**
 * @brief Calculates Y coordinate to vertically center text on screen
 *
 * @param text Text to center
 * @return Y coordinate for centered text
 */
int UiManager::centerYForText(const char *text)
{
  int h = getFontHeight();
  return (lcd.height() - h) / 2;
}

/**
 * @brief Calculates X position after a specific character index in a string
 *
 * @param text Text string
 * @param index Character index
 * @return X pixel position after the indexed character
 */
int UiManager::calcNextXPos(const char *text, int index)
{
  return lcd.calc_next_xPos(text, index, lcd);
}

/**
 * @brief Returns the pixel width of a text string using current font
 *
 * @param text Text to measure
 * @return Width in pixels
 */
int UiManager::getStringPxLen(const char *text)
{
  return static_cast<int>(lcd.getStringPxLen(text));
}

/**
 * @brief Returns the height of the current font
 *
 * @return Font height in pixels
 */
int UiManager::getFontHeight()
{
  return lcd.font ? lcd.font[2] : 0;
}

/**
 * @brief Returns the LCD screen width
 *
 * @return Screen width in pixels
 */
int UiManager::getWidth()
{
  return lcd.width();
}

/**
 * @brief Returns the LCD screen height
 *
 * @return Screen height in pixels
 */
int UiManager::getHeight()
{
  return lcd.height();
}

/**
 * @brief Sets the text cursor position for next print operation
 *
 * @param x X coordinate in pixels
 * @param y Y coordinate in pixels
 */
void UiManager::locate(int x, int y)
{
  lcd.locate(x, y);
}

/**
 * @brief Prints text at current cursor position
 *
 * @param text Text to print
 */

void UiManager::print(const std::string &text)
{
  lcd.print(text.c_str());
}

/**
 * @brief Copies the screen buffer to the physical LCD display
 */
void UiManager::copyToLcd()
{
  lcd.copy_to_lcd();
}

/**
 * @brief Sets RGB LED color using PWM values
 *
 * @param r Red intensity (0-255)
 * @param g Green intensity (0-255)
 * @param b Blue intensity (0-255)
 */
void UiManager::setLedColor(int r, int g, int b)
{
  analogWrite(LED_R_PIN, r);
  analogWrite(LED_G_PIN, g);
  analogWrite(LED_B_PIN, b);
}

/**
 * @brief Sets RGB LED color using predefined color state
 *
 * @param colorState One of LED_STATE_* enum values
 */
void UiManager::setLedColor(int colorState)
{
  switch (colorState)
  {
  case LED_STATE_OFF:
    setLedColor(RGB_OFF);
    break;
  case LED_STATE_RED:
    setLedColor(RGB_RED);
    break;
  case LED_STATE_GREEN:
    setLedColor(RGB_GREEN);
    break;
  case LED_STATE_BLUE:
    setLedColor(RGB_BLUE);
    break;
  case LED_STATE_YELLOW:
    setLedColor(RGB_YELLOW);
    break;
  case LED_STATE_CYAN:
    setLedColor(RGB_CYAN);
    break;
  case LED_STATE_MAGENTA:
    setLedColor(RGB_MAGENTA);
    break;
  case LED_STATE_WHITE:
    setLedColor(RGB_WHITE);
    break;
  default:
    setLedColor(RGB_OFF);
    break;
  }
}

/**
 * @brief Blinks the LED with specified color and duration
 *
 * @param colorState Color to blink (LED_STATE_* enum)
 * @param duration Duration in milliseconds
 */
void UiManager::blinkLed(int colorState, unsigned long duration)
{
  setLedColor(LED_STATE_OFF);
  delayMicroseconds(100);
  setLedColor(colorState);
  delay(duration);
  setLedColor(LED_STATE_BLUE);
}

/**
 * @brief Sets font to Small_7 (default small font)
 */
void UiManager::setFontNormal()
{
  lcd.set_font((unsigned char *)Small_7);
}

/**
 * @brief Sets font to Consolas 7x13 (monospace font for PIN entry)
 */
void UiManager::setFontConsolas()
{
  lcd.set_font((unsigned char *)DOGL_Consolas7x13);
}

/**
 * @brief Returns pointer to current font array
 *
 * @return Pointer to current font data
 */
unsigned char *UiManager::getCurrentFont()
{
  return lcd.font;
}

/**
 * @brief Restores a previously saved font
 *
 * @param font Pointer to font data to restore
 */
void UiManager::restoreFont(unsigned char *font)
{
  if (font)
    lcd.set_font(font);
}

/**
 * @brief Draws a filled rectangle on the screen
 *
 * @param x1 Top-left X coordinate
 * @param y1 Top-left Y coordinate
 * @param x2 Bottom-right X coordinate
 * @param y2 Bottom-right Y coordinate
 * @param pattern Fill pattern (0=clear, 1=fill)
 */
void UiManager::fillrect(int x1, int y1, int x2, int y2, int pattern)
{
  lcd.fillrect(x1, y1, x2, y2, pattern);
}

/**
 * @brief Enables or disables monospace rendering mode
 *
 * @param enable true to enable monospace, false for proportional
 */
void UiManager::setMonospace(bool enable)
{
  lcd.setMonospace(enable);
}
