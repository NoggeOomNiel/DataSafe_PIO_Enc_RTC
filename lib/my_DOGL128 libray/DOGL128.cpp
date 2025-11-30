/* mbed library DOGL128-6 128*64 pixel LCD
 * Copyright (c) 2012 Peter Drescher - DC2PD,
 * Copyright (c) 2016 Łukasz Godziejewski
 *
 * MIT License
 */

// optional defines :
// #define debug_lcd  1

#include "DOGL128.h"
#include <Arduino.h>
#include "stdio.h"




#define BPP 1 // Bits per pixel

DOGL128::DOGL128(uint16_t mosi_pin, uint16_t clk_pin, uint16_t cs_pin, uint16_t a0_pin, uint16_t reset_pin, const char *name)
    //: GraphicsDisplay(name), _mosi_pin(mosi_pin), _clk_pin(clk_pin), _cs_pin(cs_pin), _a0_pin(a0_pin), _reset_pin(reset_pin), _spi_settings(20000000, MSBFIRST, SPI_MODE3)
    : GraphicsDisplay(name),
      _mosi_pin(mosi_pin),
      _clk_pin(clk_pin),
      _cs_pin(cs_pin),
      _a0_pin(a0_pin),
      _reset_pin(reset_pin),
      _spi_settings(20000000, MSBFIRST, SPI_MODE0) // 20MHz, MSB first, SPI Mode 0 (ST7565 typical)
{
    // Initialize members that don't depend on hardware being ready
    orientation = 0;
    contrast = 16;
    draw_mode = NORMAL;
    char_x = 0;
    char_y = 0;
    auto_up = 1;
    m_useMonospaceSpacing = false; // Default to proportional spacing
    // Hardware initialization is deferred to the begin() method
}

void DOGL128::begin()
{
    // Setup control pins
    pinMode(_cs_pin, OUTPUT);
    digitalWrite(_cs_pin, HIGH); // Deselect display initially
    pinMode(_a0_pin, OUTPUT);    // A0 is also known as DC (Data/Command)
    pinMode(_reset_pin, OUTPUT);

    // Perform hardware reset and SPI/display initialization
    // This will also call SPI.begin() after setting up pins for RP2040 if applicable
    lcd_reset();
    setRotation(orientation); // Apply initial/default rotation hardware commands
}

void DOGL128::setRotation(uint8_t r)
{
    orientation = r % 4; // Store the rotation
    switch (orientation)
    {
    case 0:                               // Landscape 0 degrees
        wr_cmd(kADCModeNormal);           // 0xA0: Column 0 to 127
        wr_cmd(kCommonOutputModeReverse); // 0xC8: Row scan COM63 to COM0 (pages 7 to 0)
        break;
    case 1: // Portrait 90 degrees
        // Hardware scan direction remains as for landscape; software handles XY swap.
        wr_cmd(kADCModeNormal);
        wr_cmd(kCommonOutputModeReverse);
        break;
    case 2:                              // Landscape 180 degrees
        wr_cmd(kADCModeReverse);         // 0xA1: Column 127 to 0
        wr_cmd(kCommonOutputModeNormal); // 0xC0: Row scan COM0 to COM63 (pages 0 to 7)
        break;
    case 3: // Portrait 270 degrees
        // Hardware scan direction remains as for landscape 180; software handles XY swap.
        wr_cmd(kADCModeReverse);
        wr_cmd(kCommonOutputModeNormal);
        break;
    }
}

int DOGL128::width()
{
    //  Returns logical width based on rotation
    //  Rotations 0 & 2 are landscape (128 wide)
    //  Rotations 1 & 3 are portrait (64 wide)
    if (orientation == 1 || orientation == 3) // Portrait
        return 64;
    else
        return 128;
}

int DOGL128::height()
{
    // Returns logical height based on rotation
    // Rotations 0 & 2 are landscape (64 high)
    // Rotations 1 & 3 are portrait (128 high)
    if (orientation == 1 || orientation == 3) // Portrait
        return 128;
    else
        return 64;
}

/*void DOGL128::set_orientation(unsigned int o)
{
    orientation = o;
    switch (o) {
        case (0):
            wr_cmd(0xA0);
            wr_cmd(0xC0);
            break;
        case (1):
            wr_cmd(0xA0);
            wr_cmd(0xC8);
            break;
        case (2):
            wr_cmd(0xA1);
            wr_cmd(0xC8);
            break;
        case (3):
            wr_cmd(0xA1);
            wr_cmd(0xC0);
            break;
    }
}

*/

void DOGL128::invert(unsigned int o)
{
    if (o == 0)
        wr_cmd(kDisplayModeNormal);
    else
        wr_cmd(kDisplayModeReverse);
}

void DOGL128::set_contrast(unsigned int o)
{
    contrast = o;
    wr_cmd(kSetContrast);
    // only bits 0 - 5.
    wr_cmd(o & 0x3F);
}

unsigned int DOGL128::get_contrast(void)
{
    return (contrast);
}

// write command to lcd controller

void DOGL128::wr_cmd(unsigned char cmd)
{
    digitalWrite(_a0_pin, LOW);
    digitalWrite(_cs_pin, LOW);
#ifdef SPI_HAS_TRANSACTION
    SPI.beginTransaction(_spi_settings);
#endif
    SPI.transfer(cmd);
#ifdef SPI_HAS_TRANSACTION
    SPI.endTransaction();
#endif
    digitalWrite(_cs_pin, HIGH);
}

// write data to lcd controller

void DOGL128::wr_dat(unsigned char dat)
{
    digitalWrite(_a0_pin, HIGH);
    digitalWrite(_cs_pin, LOW);
#ifdef SPI_HAS_TRANSACTION
    SPI.beginTransaction(_spi_settings);
#endif
    SPI.transfer(dat);
#ifdef SPI_HAS_TRANSACTION
    SPI.endTransaction();
#endif
    digitalWrite(_cs_pin, HIGH);
}

// reset and init the lcd controller

void DOGL128::configure()
{
    // init taken from official DOGL Arduino library, values:
    // 0x40, 0xA1, 0xC0, 0xA6, 0xA2, 0x2F, 0xF8, 0x00, 0x27, 0x81, 0x10, 0xAC, 0x00, 0xAF

    wr_cmd(0x40); // start line = 0
    wr_cmd(kADCModeReverse);
    wr_cmd(kCommonOutputModeNormal);
    wr_cmd(kDisplayModeNormal);
    wr_cmd(kLCDBiasOneNinth);

    wr_cmd(0x2F); //  power on
    wr_cmd(0xF8); // set booster ratio
    wr_cmd(0x00); // to 2x, 3x, 4x

    wr_cmd(0x27); // resistor ratio set to 6.5

    wr_cmd(kSetContrast);
    wr_cmd(0x10); // set optimal contrast

    wr_cmd(kStaticIndicatorOff);
    wr_cmd(0x00); // static indicator register (?)

    wr_cmd(kDisplayOn);
}

void DOGL128::lcd_reset()
{
    // Hardware reset pulse
    digitalWrite(_reset_pin, LOW);
    delay(10);
    digitalWrite(_reset_pin, HIGH);
    delay(5); // Wait for display to settle after reset

    // Configure SPI pins if this board allows/requires it (e.g., RP2040)

#if defined(ARDUINO_ARCH_RP2040) // Or a more general check if applicable, e.g. SPI_HAS_PIN_ASSIGNMENT

    if (_mosi_pin != (uint16_t)-1)
        SPI.setMOSI(_mosi_pin);
    if (_clk_pin != (uint16_t)-1)
        SPI.setSCK(_clk_pin);
#endif

    // Initialize SPI. It's generally safe to call SPI.begin() multiple times on most Arduino cores,
    // or it initializes the first time. If pins were remapped (RP2040), this call will use them.
    SPI.begin();

    wr_cmd(kReset);

    delay(5);

    /* Start Initial Sequence ----------------------------------------------------*/

    configure();
    // clear and update LCD
    clear_buffer();
    copy_to_lcd();
    // auto_up is already set to 1 by constructor.
    locate(0, 0);
    set_font((unsigned char *)Small_7); // standart font
}

// set one pixel in buffer

void DOGL128::pixel(int x, int y, int color)
{
    // //first check parameter
    // if (x > 128 || y > 64 || x < 0 || y < 0)
    //     return;
    // x, y are logical coordinates

    // Bounds check against logical dimensions
    if (x < 0 || y < 0)
        return;
    // width() and height() return logical dimensions
    if (x >= width() || y >= height())
        return;

    int16_t phy_x = x;
    int16_t phy_y = y;

    // Transform logical (x,y) to physical (phy_x, phy_y) for the 128x64 buffer
    // Physical buffer is 128 wide, 64 high. (0,0) is top-left of buffer.
    switch (orientation)
    {
    case 1: // Portrait 90 deg. Logical W=64, H=128.
        phy_x = y;
        phy_y = 64 - 1 - x;
        break;
    case 2: // Landscape 180 deg. Logical W=128, H=64.
        phy_x = 128 - 1 - x;
        phy_y = 64 - 1 - y;
        break;
    case 3: // Portrait 270 deg. Logical W=64, H=128.
        phy_x = 128 - 1 - y;
        phy_y = x;
        break;
    case 0: // Landscape 0 deg. Logical W=128, H=64.
    default:
        // phy_x = x, phy_y = y (no change)
        break;
    }

    // Sanity check physical coordinates (should be within 0-127 and 0-63)
    // This check should ideally not be needed if logical checks and transforms are correct.
    if (phy_x < 0 || phy_x >= 128 || phy_y < 0 || phy_y >= 64)
    {
        // This indicates an error in the transformation or logical bounds checking.
        // For safety, we can return, or clip.
        return;
    }

    if (draw_mode == NORMAL)
    {
        if (color == 0)
            buffer[phy_x + ((phy_y / 8) * 128)] &= ~(1 << (phy_y % 8)); // erase pixel
        else
            buffer[phy_x + ((phy_y / 8) * 128)] |= (1 << (phy_y % 8)); // set pixel
    }
    else
    { // XOR mode
        if (color == 1)
            buffer[phy_x + ((phy_y / 8) * 128)] ^= (1 << (phy_y % 8)); // xor pixel
    }
}

void DOGL128::write_to_page(uint8_t page_number)
{
    wr_cmd(0x00);               // set column low nibble 0
    wr_cmd(0x10);               // set column hi  nibble 0
    wr_cmd(0xB0 | page_number); // set page address

    for (int i = 128 * page_number; i < 128 * (page_number + 1); ++i)
    {
        wr_dat(buffer[i]);
    }
}

void DOGL128::clear_buffer()
{
    memset(buffer, 0, kPageSize * kPageCount);
}

// update lcd
void DOGL128::copy_to_lcd(void)
{
    for (uint8_t i = 0; i < kPageCount; ++i)
    {
        write_to_page(i);
    }
}

void DOGL128::cls(void)
{
    clear_buffer();
    copy_to_lcd();
}

void DOGL128::line(int x0, int y0, int x1, int y1, int color)
{
    int dx = 0, dy = 0;
    int dx_sym = 0, dy_sym = 0;
    int dx_x2 = 0, dy_x2 = 0;
    int di = 0;

    dx = x1 - x0;
    dy = y1 - y0;

    //  if (dx == 0) {        /* vertical line */
    //      if (y1 > y0) vline(x0,y0,y1,color);
    //      else vline(x0,y1,y0,color);
    //      return;
    //  }

    if (dx > 0)
    {
        dx_sym = 1;
    }
    else
    {
        dx_sym = -1;
    }
    //  if (dy == 0) {        /* horizontal line */
    //      if (x1 > x0) hline(x0,x1,y0,color);
    //      else  hline(x1,x0,y0,color);
    //      return;
    //  }

    if (dy > 0)
    {
        dy_sym = 1;
    }
    else
    {
        dy_sym = -1;
    }

    dx = dx_sym * dx;
    dy = dy_sym * dy;

    dx_x2 = dx * 2;
    dy_x2 = dy * 2;

    if (dx >= dy)
    {
        di = dy_x2 - dx;
        while (x0 != x1)
        {

            pixel(x0, y0, color);
            x0 += dx_sym;
            if (di < 0)
            {
                di += dy_x2;
            }
            else
            {
                di += dy_x2 - dx_x2;
                y0 += dy_sym;
            }
        }
        pixel(x0, y0, color);
    }
    else
    {
        di = dx_x2 - dy;
        while (y0 != y1)
        {
            pixel(x0, y0, color);
            y0 += dy_sym;
            if (di < 0)
            {
                di += dx_x2;
            }
            else
            {
                di += dx_x2 - dy_x2;
                x0 += dx_sym;
            }
        }
        pixel(x0, y0, color);
    }
    if (auto_up)
        copy_to_lcd();
}

void DOGL128::angledLine(int x0, int y0, int angle_deg, int length, int colour)
{
    if (length == 0) {
        pixel(x0, y0, colour); // Draw a single point if length is 0
        if (auto_up) copy_to_lcd();
        return;
    }

    float angle_rad = radians(angle_deg); // Convert degrees to radians

    int x1 = round(x0 + length * cos(angle_rad));
    int y1 = round(y0 - length * sin(angle_rad)); //anticlockwise rotation
    //int y1 = round(y0 + length * sin(angle_rad)); //clockwise rotation

    line(x0, y0, x1, y1, colour); // Call the existing line function
}

int DOGL128::calc_next_xPos(const char *text, int target_char_index, DOGL128 &lcd_instance)
{
  if (target_char_index < 0)
    return 0;

  int current_x = 0;
  unsigned char *current_font = lcd_instance.font; // Get font from LCD instance

  for (int i = 0; i < target_char_index; ++i)
  {
    if (text[i] == '\0')
      return -1; // Target index out of bounds of the string

    unsigned char char_code = text[i];
    // Ensure character is in printable range for the font (typically 32-127 for ASCII based fonts)
    if (char_code < 32 || char_code > 127)
    {
      // This character is outside the typical font range.
      // Its width might be undefined or could default.
      // For PIN entry with '_', '0'-'9', this should not be an issue.
      // If it occurs, we might add a default width or stop.
      // For now, assume valid characters from PIN entry.
      // A robust way would be to check if (char_code - 32) * offset is within font data bounds.
    }

    unsigned int offset = current_font[0]; // bytes / char in font data
    // Point to the specific character's data in the font array
    unsigned char *search_ptr = &current_font[((char_code - 32) * offset) + 4];
    // The first byte of a character's data in this font format is its actual pixel width
    unsigned char w = search_ptr[0];

    current_x += w;
  }
  return current_x;
}

void DOGL128::rect(int x0, int y0, int x1, int y1, int color)
{

    if (x1 > x0)
        line(x0, y0, x1, y0, color);
    else
        line(x1, y0, x0, y0, color);

    if (y1 > y0)
        line(x0, y0, x0, y1, color);
    else
        line(x0, y1, x0, y0, color);

    if (x1 > x0)
        line(x0, y1, x1, y1, color);
    else
        line(x1, y1, x0, y1, color);

    if (y1 > y0)
        line(x1, y0, x1, y1, color);
    else
        line(x1, y1, x1, y0, color);

    if (auto_up)
        copy_to_lcd();
}

void DOGL128::fillrect(int x0, int y0, int x1, int y1, int color)
{
    int l, c, i;
    if (x0 > x1)
    {
        i = x0;
        x0 = x1;
        x1 = i;
    }

    if (y0 > y1)
    {
        i = y0;
        y0 = y1;
        y1 = i;
    }

    for (l = x0; l <= x1; l++)
    {
        for (c = y0; c <= y1; c++)
        {
            pixel(l, c, color);
        }
    }
    if (auto_up)
        copy_to_lcd();
}

void DOGL128::circle(int x0, int y0, int r, int color)
{

    int draw_x0, draw_y0;
    int draw_x1, draw_y1;
    int draw_x2, draw_y2;
    int draw_x3, draw_y3;
    int draw_x4, draw_y4;
    int draw_x5, draw_y5;
    int draw_x6, draw_y6;
    int draw_x7, draw_y7;
    int xx, yy;
    int di;
    // WindowMax();
    if (r == 0)
    { /* no radius */
        return;
    }

    draw_x0 = draw_x1 = x0;
    draw_y0 = draw_y1 = y0 + r;
    if (draw_y0 < height())
    {
        pixel(draw_x0, draw_y0, color); /* 90 degree */
    }

    draw_x2 = draw_x3 = x0;
    draw_y2 = draw_y3 = y0 - r;
    if (draw_y2 >= 0)
    {
        pixel(draw_x2, draw_y2, color); /* 270 degree */
    }

    draw_x4 = draw_x6 = x0 + r;
    draw_y4 = draw_y6 = y0;
    if (draw_x4 < width())
    {
        pixel(draw_x4, draw_y4, color); /* 0 degree */
    }

    draw_x5 = draw_x7 = x0 - r;
    draw_y5 = draw_y7 = y0;
    if (draw_x5 >= 0)
    {
        pixel(draw_x5, draw_y5, color); /* 180 degree */
    }

    if (r == 1)
    {
        return;
    }

    di = 3 - 2 * r;
    xx = 0;
    yy = r;
    while (xx < yy)
    {

        if (di < 0)
        {
            di += 4 * xx + 6;
        }
        else
        {
            di += 4 * (xx - yy) + 10;
            yy--;
            draw_y0--;
            draw_y1--;
            draw_y2++;
            draw_y3++;
            draw_x4--;
            draw_x5++;
            draw_x6--;
            draw_x7++;
        }
        xx++;
        draw_x0++;
        draw_x1--;
        draw_x2++;
        draw_x3--;
        draw_y4++;
        draw_y5++;
        draw_y6--;
        draw_y7--;

        if ((draw_x0 <= width()) && (draw_y0 >= 0))
        {
            pixel(draw_x0, draw_y0, color);
        }

        if ((draw_x1 >= 0) && (draw_y1 >= 0))
        {
            pixel(draw_x1, draw_y1, color);
        }

        if ((draw_x2 <= width()) && (draw_y2 <= height()))
        {
            pixel(draw_x2, draw_y2, color);
        }

        if ((draw_x3 >= 0) && (draw_y3 <= height()))
        {
            pixel(draw_x3, draw_y3, color);
        }

        if ((draw_x4 <= width()) && (draw_y4 >= 0))
        {
            pixel(draw_x4, draw_y4, color);
        }

        if ((draw_x5 >= 0) && (draw_y5 >= 0))
        {
            pixel(draw_x5, draw_y5, color);
        }
        if ((draw_x6 <= width()) && (draw_y6 <= height()))
        {
            pixel(draw_x6, draw_y6, color);
        }
        if ((draw_x7 >= 0) && (draw_y7 <= height()))
        {
            pixel(draw_x7, draw_y7, color);
        }
    }
    if (auto_up)
        copy_to_lcd();
}

void DOGL128::fillcircle(int x, int y, int r, int color)
{
    int i, up;
    up = auto_up;
    auto_up = 0; // off
    for (i = 0; i <= r; i++)
        circle(x, y, i, color);
    auto_up = up;
    if (auto_up)
        copy_to_lcd();
}

void DOGL128::setmode(int mode)
{
    draw_mode = mode;
}

void DOGL128::locate(int x, int y)
{
    char_x = x;
    char_y = y;
}

int DOGL128::columns()
{
    return width() / font[1];
}

int DOGL128::rows()
{
    return height() / font[2];
}

int DOGL128::_putc(int value)
{
    if (value == '\n')
    { // new line
        char_x = 0;
        char_y = char_y + font[2];
        if (char_y >= height() - font[2])
        {
            char_y = 0;
        }
    }
    else
    {
        character(char_x, char_y, value);
        if (auto_up)
            copy_to_lcd();
    }
    return value;
}

void DOGL128::character(int x, int y, int c)
{
    unsigned int hor, vert, offset, bpl, j, i, b;
    unsigned char *zeichen;
    unsigned char z, w;

    if ((c < 31) || (c > 127))
        return; // test char range

    // read font parameter from start of array
    offset = font[0]; // bytes / char
    hor = font[1];    // get hor size of font
    vert = font[2];   // get vert size of font
    bpl = font[3];    // bytes per line

    if (char_x + hor > width())
    {
        char_x = 0;
        char_y = char_y + vert;
        if (char_y >= height() - font[2])
        {
            char_y = 0;
        }
    }

    zeichen = &font[((c - 32) * offset) + 4]; // start of char bitmap
    w = zeichen[0];                           // width of actual char
    // construct the char into the buffer
    for (j = 0; j < vert; j++)
    { //  vert line
        for (i = 0; i < hor; i++)
        { //  horz line
            z = zeichen[bpl * i + ((j & 0xF8) >> 3) + 1];
            b = 1 << (j & 0x07);
            if ((z & b) == 0x00)
            {
                pixel(x + i, y + j, 0);
            }
            else
            {
                pixel(x + i, y + j, 1);
            }
        }
    }

    if (m_useMonospaceSpacing) {
        // Use font's defined cell width for monospace
        char_x += hor; // hor is font[1], the cell width
    } else {
        // Use actual character width for proportional spacing
        char_x += w;   // w is zeichen[0], the actual width
    }
}

void DOGL128::set_font(unsigned char *f)
{
    font = f;
}

void DOGL128::set_auto_up(unsigned int up)
{
    if (up)
        auto_up = 1;
    else
        auto_up = 0;
}

unsigned int DOGL128::get_auto_up(void)
{
    return (auto_up);
}

void DOGL128::setMonospace(bool enabled)
{
    m_useMonospaceSpacing = enabled;
}

bool DOGL128::isMonospaceEnabled() const
{
    return m_useMonospaceSpacing;
}

void DOGL128::print_bm(Bitmap bm, int x, int y)
{
    int h, v, b;
    char d;

    for (v = 0; v < bm.ySize; v++)
    { // lines
        for (h = 0; h < bm.xSize; h++)
        { // pixel
            if (h + x > 127)
                break;
            if (v + y > 63)
                break;
            d = bm.data[bm.Byte_in_Line * v + ((h & 0xF8) >> 3)];
            b = 0x80 >> (h & 0x07);
            if ((d & b) == 0)
            {
                pixel(x + h, y + v, 0);
            }
            else
            {
                pixel(x + h, y + v, 1);
            }
        }
    }
}

void DOGL128::print_logo(const unsigned char *data)
{
    memcpy(buffer, data, 1024);
}

unsigned int DOGL128::getStringPxLen(const char *text) const
{
    if (text == nullptr || font == nullptr) {
        return 0;
    }

    unsigned int total_width = 0;
    unsigned char *current_font = font;

    // Read font parameters
    const unsigned int offset = current_font[0]; // bytes per char
    const unsigned int hor = current_font[1];    // cell width for monospace
    //unsigned int vert = current_font[2];   // cell height (not needed here)
    //unsigned int bpl = current_font[3];    // bytes per line (not needed here)

    // Process each character in the string
    for (int i = 0; text[i] != '\0'; i++) {
        const unsigned char char_code = text[i];

        // Handle newline character (width = 0 for calculation purposes)
        if (char_code == '\n') {
            continue;
        }

        // Only process printable characters (typically ASCII 32-127)
        if (char_code >= 32 && char_code <= 127) {
            if (m_useMonospaceSpacing) {
                // Use fixed cell width for monospace
                total_width += hor;
            } else {
                // Use actual character width for proportional spacing
                const unsigned char *char_data = &current_font[((char_code - 32) * offset) + 4];
                const unsigned char actual_width = char_data[0]; // first byte is actual width
                total_width += actual_width;
            }
        }
        // Note: Non-printable characters outside 32-127 range are skipped
        // You could add a default width here if needed
    }

    return total_width;
}
