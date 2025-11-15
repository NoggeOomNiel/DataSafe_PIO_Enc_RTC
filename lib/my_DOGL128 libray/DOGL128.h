/* mbed library DOGL128-6 128*64 pixel LCD
 * Copyright (c) 2012 Peter Drescher - DC2PD,
 * Copyright (c) 2016 Łukasz Godziejewski
 *
 * MIT License
 */

#ifndef DOGL128_H
#define DOGL128_H

#include <Arduino.h>
#include "GraphicsDisplay.h"
#include <SPI.h>
#include "fonts\Small_7.h"
#include "fonts\DOGL_CONSOLAS_6x11.h"
#include "fonts\DOGL_CONSOLAS_7x13.h"

/** optional Defines :
 * #define debug_lcd  1  enable infos to PC_USB
 */

/** Draw mode
 * NORMAl
 * XOR set pixel by xor the screen
 */
enum
{
  NORMAL,
  XOR
};

/** Bitmap
 */
struct Bitmap
{
  int xSize;
  int ySize;
  int Byte_in_Line;
  char *data;
};

/** DOGL128-6 (ST7565) handling library.
Initializes on spi defined in constructor. Works on an internal buffer and writes buffer
content to driver ram.
*/
class DOGL128 : public GraphicsDisplay
{
public:
  /** Create a DOGL128 object connected to SPI0
   *
   */
  DOGL128(uint16_t mosi_pin, uint16_t clk_pin, uint16_t cs_pin, uint16_t a0_pin, uint16_t reset_pin, const char *name = "LCD");

  /** Initialize the display.
   * Call this method from your sketch's setup() function.
   */
  void begin();

  /** Set the screen rotation.
   * @param r Rotation value (0-3). 0: Landscape, 1: Portrait, 2: Landscape 180, 3: Portrait 270.
   * Default is 0.
   */
  void setRotation(uint8_t r);

  /** Get the width of the screen in pixel
   *
   * @param
   * @returns width of screen in pixel
   *
   */
  virtual int width();

  /** Get the height of the screen in pixel
   *
   * @returns height of screen in pixel
   *
   */
  virtual int height();

  /** Draw a pixel at x,y black or white
   *
   * @param x horizontal position
   * @param y vertical position
   * @param colour ,1 set pixel ,0 erase pixel
   */
  virtual void pixel(int x, int y, int colour);

  /** draw a circle
   *
   * @param x0,y0 center
   * @param r radius
   * @param colour ,1 set pixel ,0 erase pixel
   *
   */
  void circle(int x, int y, int r, int colour);

  /** draw a filled circle
   *
   * @param x0,y0 center
   * @param r radius
   * @param color ,1 set pixel ,0 erase pixel
   *
   * use circle with different radius,
   * can miss some pixel
   */
  void fillcircle(int x, int y, int r, int colour);

  /** draw a 1 pixel line
   *
   * @param x0,y0 start point
   * @param x1,y1 stop point
   * @param color ,1 set pixel ,0 erase pixel
   *
   */
  void line(int x0, int y0, int x1, int y1, int colour);

  /** draw an angled line
   *
   * @param x0,y0 start point
   * @param angle_deg angle in degrees (0 is along positive X-axis)
   * @param length length of the line in pixels
   * @param colour 1 set pixel, 0 erase pixel
   *
   */
  void angledLine(int x0, int y0, int angle_deg, int length, int colour);

  /** calculate the starting x-pixel for a character in a string
   *  when printed using the DOGL128 library's current font.
   * @param text Current text string
   * @param target_char_index String index of next character to print
   * @param lcd_instance DOGL128 instance )(eg. LCD)
   *
   */
  int calc_next_xPos(const char *text, int target_char_index, DOGL128 &lcd_instance);

  /** draw a rect
   *
   * @param x0,y0 top left corner
   * @param x1,y1 down right corner
   * @param color 1 set pixel ,0 erase pixel
   *                                                   *
   */
  void rect(int x0, int y0, int x1, int y1, int colour);

  /** draw a filled rect
   *
   * @param x0,y0 top left corner
   * @param x1,y1 down right corner
   * @param color 1 set pixel ,0 erase pixel
   *
   */
  void fillrect(int x0, int y0, int x1, int y1, int colour);

  /** copy display buffer to lcd
   *
   */
  void copy_to_lcd(void);

  /** set the orienation of the screen
   *
   */

  // void set_orientation(unsigned int o);

  /** set the contrast of the screen
   *
   * @param o contrast 0-63
   */
  void set_contrast(unsigned int o);

  /** read the contrast level
   *
   */
  unsigned int get_contrast(void);

  /** invert the screen
   *
   * @param o = 0 normal, 1 invert
   */
  void invert(unsigned int o);

  /** clear the screen
   *
   */
  virtual void cls(void);

  /** set the drawing mode
   *
   * @param mode NORMAl or XOR
   */
  void setmode(int mode);

  virtual int columns(void);

  /** calculate the max number of columns
   *
   * @returns max column
   * depends on actual font size
   *
   */
  virtual int rows(void);

  /** put a char on the screen
   *
   * @param value char to print
   * @returns printed char
   *
   */
  virtual int _putc(int value);

  /** draw a character on given position out of the active font to the LCD
   *
   * @param x x-position of char (top left)
   * @param y y-position
   * @param c char to print
   *
   */
  virtual void character(int x, int y, int c);

  /** setup cursor position
   *
   * @param x x-position (top left)
   * @param y y-position
   */
  virtual void locate(int x, int y);

  /** setup auto update of screen
   *
   * @param up 1 = on , 0 = off
   * if switched off the program has to call copy_to_lcd()
   * to update screen from framebuffer
   */
  void set_auto_up(unsigned int up);

  /** get status of the auto update function
   *
   *  @returns if auto update is on
   */
  unsigned int get_auto_up(void);

  /** Set the character spacing mode.
   *
   * @param enabled true for monospace spacing, false for proportional spacing (default).
   */
  void setMonospace(bool enabled);

  /** Get the current character spacing mode.
   * @return true if monospace spacing is enabled, false otherwise.
   */
  bool isMonospaceEnabled() const;

  /** Vars     */
  unsigned char *font;
  unsigned int draw_mode;
  uint16_t _mosi_pin;
  uint16_t _clk_pin;
  uint16_t _cs_pin;
  uint16_t _a0_pin;
  uint16_t _reset_pin;
  SPISettings _spi_settings;

  /** select the font to use
   *
   * @param f pointer to font array
   *
   *   font array can created with GLCD Font Creator from http://www.mikroe.com
   *   you have to add 4 parameter at the beginning of the font array to use:
   *   - the number of byte / char
   *   - the vertial size in pixel
   *   - the horizontal size in pixel
   *   - the number of byte per vertical line
   *   you also have to change the array to char[]
   *
   */
  void set_font(unsigned char *f);

  /** print bitmap to buffer
   *
   * @param bm Bitmap in flash
   * @param x  x start
   * @param y  y start
   *
   */
  void print_bm(Bitmap bm, int x, int y);

  /// Fill buffer with preset array
  void print_logo(const unsigned char *data);

  unsigned int getStringPxLen(const char *text) const;

protected:
  /** draw a horizontal line
   *
   * @param x0 horizontal start
   * @param x1 horizontal stop
   * @param y vertical position
   * @param ,1 set pixel ,0 erase pixel
   *
   */
  void hline(int x0, int x1, int y, int colour);

  /** draw a vertical line
   *
   * @param x horizontal position
   * @param y0 vertical start
   * @param y1 vertical stop
   * @param ,1 set pixel ,0 erase pixel
   */
  void vline(int y0, int y1, int x, int colour);

  /** Configure LCD registers.

  */
  void configure();

  /** Init the DOGL128 LCD controller
   *
   */
  void lcd_reset();

  /** Write data to the LCD controller
   *
   * @param dat data written to LCD controller
   *
   */
  void wr_dat(unsigned char value);

  /** Write a command the LCD controller
   *
   * @param cmd: command to be written
   *
   */
  void wr_cmd(unsigned char value);

  /** Write control byte (placeholder, not defined in .cpp).
   *
   * @param cmd: command to be written
   *
   */
  void wr_cnt(unsigned char cmd);

  /** Write buffer content to provided page.

      @param page_number Page to write to.
  */
  void write_to_page(uint8_t page_number);

  /// clear the display buffer
  void clear_buffer();

  // commands values
  enum Display
  {
    kDisplayOn = 0xAF,
    kDisplayOff = 0xAE
  };
  enum ADCMode
  {
    kADCModeNormal = 0xA0,
    kADCModeReverse = 0xA1
  };
  enum DisplayMode
  {
    kDisplayModeNormal = 0xA6,
    kDisplayModeReverse = 0xA7
  };
  enum DisplayAllPoints
  {
    kDisplayAllPointsOff = 0xA4,
    kDisplayAllPointsOn = 0xA5
  };
  enum LCDBias
  {
    kLCDBiasOneNinth = 0xA2,
    kLCDBiasOneSeventh = 0xA3
  };
  enum CommonOutputMode
  {
    kCommonOutputModeNormal = 0xC0,
    kCommonOutputModeReverse = 0xC8
  };
  enum StaticIndicator
  {
    kStaticIndicatorOff = 0xAC,
    kStaticIndicatorOn = 0xAD
  };

  static const uint8_t kPageCount = 8;
  static const uint8_t kPageSize = 128;
  static const uint8_t kReset = 0xE2;
  static const uint8_t kSetContrast = 0x81;

  unsigned int orientation;
  uint16_t char_x;
  uint16_t char_y;
  unsigned char buffer[kPageSize * kPageCount];
  unsigned int contrast;
  unsigned int auto_up;
  bool m_useMonospaceSpacing; // Added for monospace/proportional toggle
};

#endif /* DOGL128_H */
