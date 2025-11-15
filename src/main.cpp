
/*
 This application:

    Author: Niel Malan
    Date: November 2025

    * Embedded password manager for a Raspberry Pi Pico with DOGL128 LCD and rotary encoder input.
    * Stores site records on an SD card as encrypted .enc files (XOR + rotate), can import/export CSV via Serial, and stream/type data to a host using USB Keyboard or Serial.
    * PIN-protected access with encrypted PIN saved on SD, supports PIN update and retry limits.
    * RTC synchronization (DS3231 or host), usage-based sorting, usage counters, buzzer and RGB LED feedback, and background encoder handling on core1.
    * Communicates with host PC via USB Serial for importing/exporting data and debugging.
    * Host PC application for easier data management is available separately.

    This code was written using https://github.com/maxgerhardt/platform-raspberrypi.git platform
    and the Arduino framework on Platfomio for the Raspberry Pi Pico
    (board_build.core = earlephilhower)

    This code was written using various AI tools to assist with code generation and debugging.

    NB: This program is provided as-is, without any warranties or liabilities.

*/

// GNU GPLv3 license:
//
//   This program is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program.  If not, see <http://www.gnu.org/licenses/>.

/*
    This program use the following libraries obtained from 3rd parties:

    ds3231: a library for the DS3231 RTC module
    Author: Petre Rodan <petre.rodan@simplex.ro>
    Available from: https: //github.com/rodan/ds3231

    my_DOGL128: a library for the DOGL128 display module.
    Author: mbed library DOGL128-6 128*64 pixel LCD
            * Copyright (c) 2012 Peter Drescher - DC2PD,
            * Copyright (c) 2016 Łukasz Godziejewski
            * MIT License
    I have modified to add some functions.
*/

#include "main.h"

/*! \brief Main setup function, initializes hardware and software components.
 *  Sets up PIO, LCD, SPI, buttons, buzzer, RTC, SD card, and USB Keyboard.
 */

void setup()
{
    // set_sys_clock_khz(300000000L / 1000, true); // set clockspeed to 300MHz - this works

    // Set up PIO state machine for handling encoder counting
    pio_add_program(pio, &quadrature_encoder_program);
    quadrature_encoder_program_init(pio, sm, ROTARY_PIN_A, 0);

    LCD.begin(); // Initialize the DOGL128 display library (includes SPI, pin setup, and display reset)
    pinMode(TFT_BL, OUTPUT); // Backlight control remains in sketch
    digitalWrite(TFT_BL, HIGH); // Turn backlight ON
    LCD.cls();
    LCD.set_auto_up(0); // need to use .copy_to_lcd() to send to screen
    LCD.invert(0);
    LCD.set_contrast(20); // good

    // Initialize SPI1 for the SD Card
    SPI1.setRX(SD_SPI1_MISO_PIN);
    SPI1.setTX(SD_SPI1_MOSI_PIN);
    SPI1.setSCK(SD_SPI1_SCK_PIN);
    SPI1.begin();

    // Initialize I2C for RTC
    Wire1.setSCL(D15);
    Wire1.setSDA(D14);
    Wire1.begin();
    DS3231_init(DS3231_CONTROL_INTCN);
    //memset(recv, 0, BUFF_MAX);

    // Initialise button pins
    pinMode(ENTERBUTTON_PIN, INPUT_PULLUP);
    pinMode(PRINTBUTTON_PIN, INPUT_PULLUP);
    digitalWrite(ENTERBUTTON_PIN, HIGH);
    digitalWrite(PRINTBUTTON_PIN, HIGH);
    attachInterrupt(ENTERBUTTON_PIN, ButtonPressISR, FALLING);
    attachInterrupt(PRINTBUTTON_PIN, ButtonPressISR, FALLING);

    // Initialise buzzer pin
    pinMode(Buzzer_PWM_PIN, OUTPUT);

    // Initialise RGB LED pins
    pinMode(LED_R_PIN, OUTPUT);
    pinMode(LED_G_PIN, OUTPUT);
    pinMode(LED_B_PIN, OUTPUT);
    analogWrite(LED_R_PIN, 0); // Off using PWM
    analogWrite(LED_G_PIN, 0); // Off using PWM
    analogWrite(LED_B_PIN, 0); // Off using PWM


    // Start Core 1
    multicore_launch_core1(doCore1Tasks);
    sleep_ms(200);

    // --- SD Card and PIN Initialization ---
    if (!SD.begin(SD_CS_PIN,SPI_DIV6_SPEED, SPI1)) // Use slower speed for compatibility 8MHz
    {
        LCD.cls();
        drawScreenBorder();
        LCD.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 10); // Adjusted for clarity
        LCD.print(F("SD Card Error!"));
        LCD.copy_to_lcd();
        while (true)
        {
            delay(500);
        }
    }
    // Load current PIN or set default and save it
    loadPinFromNVM();

    // --- Handle PIN Entry ---
    if (!handlePinEntry())
    {
        // LCD messages are handled by handlePinEntry on failure
        while (true)
        {
            setLedColor(LED_STATE_RED); // Indicate locked state
            delay(500);
        }
    }
    // PIN Entry OK
    if (askToUpdatePin())
    {
        performPinUpdateProcess(); // Handles the update process
    }
    LCD.cls();
    drawScreenBorder();

    Serial.begin(115200); //This is the USB Serial for talking to host pc
    delay(500);

    // Define content starting positions and line heights
    constexpr int content_padding = 2;
    constexpr int text_start_x = BORDER_WIDTH + content_padding;
    constexpr int text_start_y = BORDER_WIDTH + content_padding;
    constexpr int small_font_line_height = 7 + 3; // Font height 7 + spacing 3

    // RTC Time Synchronization
    if (!syncRTCWithDS3231())
    {
        //Pico not time synced - do normal host sync
        const bool rtc_synced = syncRTCWithHost();
        // Display sync status
        LCD.cls();
        drawScreenBorder();
        LCD.locate(text_start_x, text_start_y + small_font_line_height); // Position message
        if (rtc_synced)
        {
            LCD.print(F("RTC Sync OK"));
            setLedColor(LED_STATE_GREEN);
            blueLedPersistentState.store(true);
        }
        else
        {
            LCD.print(F("RTC Sync Failed!"));
            blueLedPersistentState.store(false);
            setLedColor(LED_STATE_RED);
        }
        LCD.copy_to_lcd();
        sleep_ms(2000);
    }

    // Load data from SD card.
    if (SDFile_to_Array(SD_FILENAME))
    {
        sortSiteData();
    }

    // Update LCD after SD load and core1 start
    LCD.cls();
    drawScreenBorder();
    LCD.locate(text_start_x, text_start_y + small_font_line_height);
    LCD.print(F("Data loaded."));
    LCD.copy_to_lcd();
    sleep_ms(2000);

    // drawTimeScreen();
    // sleep_ms(5000);

    resetInactivityTimer(); // Initialize inactivity timer

    currentScreen = SCREEN_CLOCK;
}


/*! \brief Main loop function, handles screen updates and user interactions.
 *  Manages screen transitions, encoder input, button presses, and periodic updates.
 */
void loop()
{
    // --- Inactivity Timeout Check ---
    if (millis() - lastActivityTime > INACTIVITY_TIMEOUT_MS)
    {
        unsigned char* originalFont = LCD.font; // Save current font

        doClockDisplay();

        // Reset any pending button presses that might have been true right before timeout
        enterButtonPressed.store(false, std::memory_order_release);
        printButtonPressed.store(false, std::memory_order_release);

        // if (!handlePinEntry())
        // {
        //     while (true)
        //     {
        //         // Stay in locked state
        //         delay(500);
        //     }
        // }

        // PIN entry successful after timeout
        LCD.set_font(originalFont);
        currentScreen = SCREEN_HOME;
        positionHold = 0;
        newPos.store(0);
        resetPos.store(true);
        drawHomeScreen(positionHold);
        setLedColor(LED_STATE_BLUE);
        resetInactivityTimer();
    }

    if (blipTone.load())
    {
        if (currentScreen == SCREEN_DETAILS)
        {
            detailViewNeedsRedraw.store(true);
        }
        sendBlipTone(2000, 50, 20);
        blipTone.store(false, std::memory_order_release);
    }
    if (currentScreen == SCREEN_CLOCK)
    {
        doClockDisplay();
    }
    else if (currentScreen == SCREEN_HOME)
    {
        handleSerialCommands();
        if (const unsigned long currentTime = millis(); currentTime - lastHeaderUpdateTime >= 1000)
        // Update every second
        {
            updateHomeScreenHeaderTime();
            lastHeaderUpdateTime = currentTime;
        }

        if (positionHold != newPos.load())
        {
            if (newPos.load() < 0)
            {
                newPos.store(0);
                resetPos.store(true);
            }
            positionHold = newPos.load();
            drawHomeScreen(positionHold);
            resetInactivityTimer(); // Encoder activity processed
        }
        if (enterButtonPressed.load(std::memory_order_acquire))
        {
            blinkLed(LED_STATE_GREEN, 200); // Green for Enter press
            delay(500);
            currentScreen = SCREEN_DETAILS;
            // Reset selection to the first item
            detailSelectedItemIndex.store(0);
            newPos.store(0);
            resetPos.store(true);
            drawDetailsScreen();
            enterButtonPressed.store(false, std::memory_order_release);
        }
    }
    else if (currentScreen == SCREEN_DETAILS)
    {
        if (detailViewNeedsRedraw.load())
        {
            const int currentRawEncoderPos = newPos.load(); // Logical encoder position from core1
            int newSelection = currentRawEncoderPos;

            //Start Keyboard if not already running
            if (!Keyboard._running)
            {
                Keyboard.begin();
            }

            // Determine the number of selectable items on the details screen.
            // If MaxDetailItems is 5, items are siteData[...][1] to siteData[...][4].
            // These correspond to selection indices 0, 1, 2, 3.
            if (newSelection < 0)
            {
                newSelection = 0;
                // If newPos was out of bounds, reset it and tell core1 to adjust its offset
                if (newPos.load() != 0)
                {
                    newPos.store(0);
                    resetPos.store(true);
                }
            }
            else if (constexpr int numDetailMenuItems = NUM_DETAIL_ITEMS_ON_SCREEN; newSelection >= numDetailMenuItems)
            {
                newSelection = numDetailMenuItems - 1; // Clamp to the last valid index
                // If newPos was out of bounds, reset it and tell core1 to adjust its offset
                if (newPos.load() != (numDetailMenuItems - 1))
                {
                    newPos.store(numDetailMenuItems - 1);
                    resetPos.store(true);
                }
            }
            detailSelectedItemIndex.store(newSelection);
            drawDetailsScreen();
            resetInactivityTimer(); // Encoder activity processed
        }
        if (enterButtonPressed.load(std::memory_order_acquire))
        {
            blinkLed(LED_STATE_GREEN, 200); // Green for Enter press
            delay(500);

            const int selectedDetailIdx = detailSelectedItemIndex.load();
            // The string displayed for the selected item on the details screen:
            // siteData[positionHold][0] is SiteName
            // selectedDetailIdx = 0 -> siteData[positionHold][1] (WebAddress)
            // selectedDetailIdx = 1 -> siteData[positionHold][2] (Detail1 - e.g., "Login...")
            // selectedDetailIdx = 2 -> siteData[positionHold][3] (Detail2 - e.g., Username)
            // selectedDetailIdx = 3 -> siteData[positionHold][4] (Detail3 - e.g., Password)
            // selectedDetailIdx = 4 -> siteData[positionHold][5] (Detail4 - e.g., "Return...")
            const String selectedItemText = siteData[positionHold][selectedDetailIdx + 1];

            bool shouldReturnHome = false;
            itemWasUsedAction = false; // Reset before processing action

            // WebAddress
            if (selectedDetailIdx == 0)
            {
                sendStringAsKeystrokes(selectedItemText);
                Keyboard.write(KEY_RETURN);
                delay(50);
            }
            // Auto-Login Trigger
            else if (selectedDetailIdx == 1 && selectedItemText.equals(AUTO_LOGIN_TRIGGER_STRING))
            {
                sendStringAsKeystrokes(siteData[positionHold][3]); // Username
                delay(500);
                Keyboard.write(KEY_TAB);
                delay(500);
                sendStringAsKeystrokes(siteData[positionHold][4]); // Password
                delay(500);
                Keyboard.write(KEY_RETURN);
                itemWasUsedAction = true; // set ranking increment
            }
            // Return
            else if (selectedDetailIdx == (NUM_DETAIL_ITEMS_ON_SCREEN - 1) && selectedItemText.equals(RETURN_STRING))
            {
                shouldReturnHome = true;
            }
            else // Type out other selected items:
            {
                sendStringAsKeystrokes(selectedItemText);
                // Increment usage count
                if (selectedDetailIdx == 2 || selectedDetailIdx == 3)
                {
                    itemWasUsedAction = true;
                }
            }

            if (shouldReturnHome)
            {
                //Stop keyboard to allow normal Serial comms
                Keyboard.end();
                if (!Serial.available())
                {
                    Serial.begin(115200); // Initialize Serial for communication with host
                    delay(500);
                }

                currentScreen = SCREEN_HOME;
                newPos.store(0); // home screen
                resetPos.store(true);
                positionHold = 0; // top of the list
                drawHomeScreen(positionHold);
            }
            else // Staying on the details screen
            {
                if (itemWasUsedAction)
                {
                    const String siteNameToKeepSelected = siteData[positionHold][SITE_NAME_INDEX];
                    // Store site name before potential sort
                    doUsageIncrement();

                    // After sorting, find the current site again to update positionHold,
                    // so the details screen continues to show the same site.
                    int newIdxOfSite = -1;
                    for (int i = 0; i < MaxSites; ++i)
                    {
                        if (siteData[i][SITE_NAME_INDEX].length() == 0)
                            break; // End of actual data
                        if (siteData[i][SITE_NAME_INDEX].equals(siteNameToKeepSelected))
                        {
                            newIdxOfSite = i;
                            break;
                        }
                    }

                    if (newIdxOfSite != -1)
                    {
                        positionHold = newIdxOfSite;
                    }
                    else
                    {
                        // Site not found after sort
                        currentScreen = SCREEN_HOME;
                        newPos.store(0);
                        resetPos.store(true);
                        positionHold = 0;
                        drawHomeScreen(positionHold);
                        enterButtonPressed = false;
                        return;
                    }
                }
                drawDetailsScreen();
            }
            enterButtonPressed.store(false, std::memory_order_release);
        }
    }

    if (printButtonPressed.load(std::memory_order_acquire))
    {
        blinkLed(LED_STATE_YELLOW, 200); // Yellow for Print press

        // Detect long-press vs short-press: button is active LOW
        const unsigned long pressStart = millis();

        // Wait while button remains pressed; check for >= 5000 ms hold
        while (digitalRead(PRINTBUTTON_PIN) == LOW)
        {
            if (millis() - pressStart >= 5000)
            {
                // Long hold detected -> reboot Pico
                LCD.cls();
                drawScreenBorder();
                LCD.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 20);
                LCD.print(F("Rebooting..."));
                LCD.copy_to_lcd();
                // Trigger system reset
                watchdog_reboot(0, 0, 1000);
            }
            sleep_ms(50);
        }

        // Button released before 5s -> treat as short press
        printButtonPressed.store(false, std::memory_order_release);
        //typeOut(); // Not used -- utility app handles this now
    }
}

// Functions

/*! \brief Resets the inactivity timer. Call this function whenever user activity is detected.
 */
void resetInactivityTimer()
{
    lastActivityTime = millis();
}

/*! \brief Draws a 2-pixel wide border around the screen.
 *  Uses LCD.rect() to draw two concentric rectangles.
 */
void drawScreenBorder()
{
    const int screen_height = LCD.height(); // DOGL128 is 128x64
    LCD.rect(0, 0, LCD.width() - 1, screen_height - 1, 1);
    LCD.rect(1, 1, LCD.width() - 2, screen_height - 2, 1);
}

/*! \brief Increments the usage count for the currently selected site.
 *  The usage count is stored in the last column of the `siteData` array for the
 *  site at `positionHold`. After incrementing, it saves the updated `siteData`
 *  to an encrypted file and then re-sorts the `siteData` array.
 */
void doUsageIncrement()
{
    long currentUsage = siteData[positionHold][USAGE_COUNT_COLUMN_INDEX].toInt();
    currentUsage++;
    siteData[positionHold][USAGE_COUNT_COLUMN_INDEX] = String(currentUsage);
    if (saveSiteDataToEncryptedFile(SD_FILENAME))
    {
        sortSiteData();
    }
    else
    {
        LCD.cls();
        drawScreenBorder();
        constexpr int content_padding = 2;
        LCD.locate(BORDER_WIDTH + content_padding, BORDER_WIDTH + content_padding + (7 + 3));
        // Position error message
        LCD.print(F("Save Error!"));
        LCD.copy_to_lcd();
        delay(2000);
    }
}

/*! \brief Generates a short blip tone on the buzzer.
 *  \param freq The frequency of the tone in Hz.
 *  \param ratio The duty cycle for the PWM signal (0-255, though analogWrite typically uses 0-255 for 8-bit PWM).
 *               Effectively controls the volume/timbre.
 *  \param duration The duration of the tone in milliseconds.
 *  \return void
 */
void sendBlipTone(const uint32_t freq, const int ratio, const uint32_t duration)
{
    analogWriteFreq(freq);
    analogWrite(Buzzer_PWM_PIN, ratio);
    sleep_ms(duration);
    analogWrite(Buzzer_PWM_PIN, 0);
}

/*! \brief Draw the Boot screen
 * \param
 * \return void
 */
void drawBootScreen()
{
    // Frame around screen
    LCD.cls();
    drawScreenBorder();
    constexpr int content_padding = 2; // Local padding for this screen
    constexpr int text_x = BORDER_WIDTH + content_padding;
    constexpr int text_y = BORDER_WIDTH + content_padding;
    LCD.locate(text_x, text_y);
    LCD.print("     Password Safe");
    LCD.line(BORDER_WIDTH, text_y + 7 + 2, LCD.width() - 1 - BORDER_WIDTH, text_y + 7 + 2, 1); // Line below title
    LCD.copy_to_lcd();
}

/*! \brief Draw the Home screen
 * \param startIndex
 * \return void
 */
void drawHomeScreen(const int startIndex)
{
    // Draw heading "Site Names"
    LCD.cls();
    drawScreenBorder();
    constexpr int content_padding = 2;
    constexpr int header_x = BORDER_WIDTH + content_padding;
    constexpr int header_y = BORDER_WIDTH + content_padding;

    constexpr int item_start_y = header_y + 7 + 3; // Header font height 7 + spacing 3
    constexpr int item_line_height = 7 + 3; // Item font height 7 + spacing 3

    LCD.locate(header_x, header_y);
    LCD.print(F("Select..."));

    datetime_t t;
    char time_buf[10]; // "HH:MM:SS\0"
    if (rtc_get_datetime(&t))
    {
        sprintf(time_buf, "%02d:%02d:%02d", t.hour, t.min, t.sec);
    }
    else
    {
        strcpy(time_buf, "--:--:--"); // time fetch failed
    }

    // Calculate position for time on the right
    // Use actual pixel width (mono) to pevent digits jumping arround
    LCD.setMonospace(true);
    if (!timePosFound)
    {
        const int time_str_width = static_cast<int>(LCD.getStringPxLen(time_buf));
        header_x_right_time = LCD.width() - BORDER_WIDTH - content_padding - time_str_width;
        timePosFound = true;
    }
    LCD.locate(header_x_right_time, header_y);
    LCD.print(time_buf);
    LCD.setMonospace(false); // back to proportional spacing

    // list sitenames for 4 entries starting from startIndex
    for (int i = 0; i < 4; i++) // Changed from 5 to 4
    {
        LCD.locate(header_x, item_start_y + (i * item_line_height));
        // Check if the index is within bounds (0 to 19 for siteData[MaxSites][SITE_DATA_FIELDS_COUNT])
        if (const int dataIndex = startIndex + i; dataIndex >= 0 && dataIndex < MaxSites)
        {
            if (i == 0)
            {
                LCD.print("> ");
                String siteName = siteData[dataIndex][0];
                if (siteName.length() > 24)
                {
                    siteName = siteName.substring(0, 22);
                    siteName += "...";
                }
                LCD.print(siteName);
            }
            else
            {
                String siteName = siteData[dataIndex][0];
                if (siteName.length() > 24)
                {
                    siteName = siteName.substring(0, 22);
                    siteName += "...";
                }
                LCD.print(siteName);
            }
        }
        else
        {
            // Print an empty line if index is out of bounds
            LCD.print("                "); // Print enough spaces to clear the line
        }
    }
    LCD.copy_to_lcd();
    lastHeaderUpdateTime = millis(); // Reset timer when full screen is drawn
}

/*! \brief Draws the details screen for the currently selected site.
 *  Displays the site name and the 4 associated data items, with the currently selected item highlighted.
 * \return void
 */
void drawDetailsScreen()
{
    LCD.cls();
    drawScreenBorder();
    constexpr int content_padding = 2;
    constexpr int text_x = BORDER_WIDTH + content_padding;
    constexpr int site_name_y = BORDER_WIDTH + content_padding;
    constexpr int item_start_y = site_name_y + 7 + 3; // Site name font height 7 + spacing 3
    constexpr int item_line_height = 7 + 3; // Item font height 7 + spacing 3

    // Display Site Name
    LCD.locate(text_x, site_name_y);
    LCD.print(siteData[positionHold][0]);

    // Display data items
    for (int i = 0; i < NUM_DETAIL_ITEMS_ON_SCREEN; i++)
    {
        LCD.locate(text_x, item_start_y + (i * item_line_height));
        String prefix_str;
        if (i == detailSelectedItemIndex.load())
        {
            prefix_str = "> ";
            LCD.print(prefix_str);
        }
        else
        {
            prefix_str = "  "; // Two spaces for alignment
            LCD.print(prefix_str);
        }

        String itemText = siteData[positionHold][i + 1];

        if (i == 0)
        {
            // This is the WebAddress field (index 0 of the displayed details, siteData[...][1])
            String displayableItemText = itemText; // Start with the full text

            // Logic to strip prefixes for display
            if (displayableItemText.startsWith(F("https://www.")))
            {
                displayableItemText = displayableItemText.substring(strlen("https://www."));
            }
            else if (displayableItemText.startsWith(F("http://www.")))
            {
                displayableItemText = displayableItemText.substring(strlen("http://www."));
            }
            else if (displayableItemText.startsWith(F("https://")))
            {
                displayableItemText = displayableItemText.substring(strlen("https://"));
            }
            else if (displayableItemText.startsWith(F("http://")))
            {
                displayableItemText = displayableItemText.substring(strlen("http://"));
            }
            else if (displayableItemText.startsWith(F("www.")))
            {
                // This case is less common to appear alone but good to have
                displayableItemText = displayableItemText.substring(strlen("www."));
            }

            const int prefix_width = static_cast<int>(LCD.getStringPxLen(prefix_str.c_str()));
            const int available_width_for_text = LCD.width() - BORDER_WIDTH - text_x - prefix_width - (
                BORDER_WIDTH + content_padding);
            String displayText = truncateStringToFit(displayableItemText, available_width_for_text, LCD);
            LCD.print(displayText);
        }
        else
        {
            LCD.print(itemText); // Print other data items
        }
    }
    LCD.copy_to_lcd();
    detailViewNeedsRedraw = false;
}

/*! \brief Interrupt Service Routine for button presses.
 *  Handles debouncing and sets flags for the enter button and print button.
 *  This ISR is attached to ENTERBUTTON_PIN and PRINTBUTTON_PIN.
 * \return void
 */
void ButtonPressISR()
{
    // This sets the flag on press, main loop consumes and clears it.
    static unsigned long lastEnterPressTime = 0;
    static unsigned long lastPrintPressTime = 0;
    const unsigned long currentTime = millis();
    //enter button
    if (digitalRead(ENTERBUTTON_PIN) == LOW)
    {
        if (currentTime - lastEnterPressTime > debounceDelay)
        {
            enterButtonPressed.store(true, std::memory_order_release);
            resetInactivityTimer();
            lastEnterPressTime = currentTime;
        }
    }
    //print button
    if (digitalRead(PRINTBUTTON_PIN) == LOW)
    {
        if (currentTime - lastPrintPressTime > debounceDelay)
        {
            printButtonPressed.store(true, std::memory_order_release);
            resetInactivityTimer();
            lastPrintPressTime = currentTime;
        }
    }
}

void parseCSVLine(const String& lineContent, const int siteIdx)
{
    String currentLine = lineContent; // Make a mutable copy
    currentLine.trim();

    // Initialize all fields
    for (int k = 0; k < SITE_DATA_FIELDS_COUNT; ++k)
    {
        if (k == USAGE_COUNT_COLUMN_INDEX)
        {
            siteData[siteIdx][k] = "0"; // Default usage count
        }
        else
        {
            siteData[siteIdx][k] = ""; // Default empty for other fields
        }
    }

    // Simple CSV tokenizer that supports quoted fields ("" -> " escape).
    constexpr int MAX_PARTS = 16;
    String parts[MAX_PARTS];
    int partsCount = 0;

    String token = "";
    bool inQuotes = false;
    const int len = static_cast<int>(currentLine.length());

    for (int i = 0; i <= len; ++i)
    {
        const char c = (i < len) ? currentLine[i] : '\0';

        if (inQuotes)
        {
            if (c == '"')
            {
                // Escaped quote "" add single quote and skip next if also quote
                if (i + 1 < len && currentLine[i + 1] == '"')
                {
                    token += '"';
                    ++i; // consume the escaped quote
                }
                else
                {
                    inQuotes = false; // closing quote
                }
            }
            else if (c == '\0')
            {
                // Unterminated quotes - treat as end
                inQuotes = false;
            }
            else
            {
                token += c;
            }
        }
        else
        {
            if (c == '"')
            {
                inQuotes = true;
            }
            else if (c == ',' || c == '\0')
            {
                token.trim();
                if (partsCount < MAX_PARTS) parts[partsCount++] = token;
                token = "";
            }
            else
            {
                token += c;
            }
        }
    }

    // Trim each parsed part (defensive)
    for (int i = 0; i < partsCount; ++i) parts[i].trim();

    /*   0: Name
         1: Site
         2: Login...   (fixed text, no surrounding quotes in stored data)
         3: UserName
         4: Password
         5: Return...  (fixed text, no surrounding quotes in stored data)
    */
    String finalFields[6];
    bool ok = false;

    if (partsCount >= 6)
    {
        // Accept if tokens at positions 2 and 5 match the fixed text (no quotes)
        if (parts[2] == "Login..." && parts[5] == "Return...")
        {
            for (int i = 0; i < 6; ++i) finalFields[i] = parts[i];
            ok = true;
        }
        else
        {
            // If not exact, still allow taking first 6 parts but do not fail hard;
            // however keep behavior consistent by logging/commenting is handled elsewhere.
            for (int i = 0; i < 6; ++i) finalFields[i] = parts[i];
            ok = true;
        }
    }
    else if (partsCount >= 4)
    {
        // Legacy short form: Name,Site,UserName,Password -> insert fixed tokens.
        finalFields[0] = parts[0]; // Name
        finalFields[1] = parts[1]; // Site
        finalFields[2] = String("Login..."); // fixed marker (no quotes stored)
        finalFields[3] = parts[2]; // UserName
        finalFields[4] = parts[3]; // Password
        finalFields[5] = String("Return..."); // fixed marker (no quotes stored)
        ok = true;
    }
    else
    {
        // map whatever parts are present into siteData
        int item = 0;
        const int limit = partsCount < SITE_DATA_FIELDS_COUNT ? partsCount : SITE_DATA_FIELDS_COUNT;
        for (int i = 0; i < limit; ++i)
        {
            siteData[siteIdx][item++] = parts[i];
        }
        ok = false;
    }

    // copy into siteData row
    if (ok)
    {
        constexpr int copyCount = SITE_DATA_FIELDS_COUNT < 6 ? SITE_DATA_FIELDS_COUNT : 6;
        for (int j = 0; j < copyCount; ++j)
        {
            siteData[siteIdx][j] = finalFields[j];
        }
        // Ensure any remaining columns beyond the copied ones are at defaults
        for (int j = copyCount; j < SITE_DATA_FIELDS_COUNT; ++j)
        {
            if (j == USAGE_COUNT_COLUMN_INDEX)
                siteData[siteIdx][j] = "0";
            else
                siteData[siteIdx][j] = "";
        }
    }

    // Final validation for usage count: if it's not a number, set to "0"
    // (Only if site name exists, otherwise the row is considered empty)

    static_assert(SITE_NAME_INDEX >= 0 && USAGE_COUNT_COLUMN_INDEX >= 0 &&
                  SITE_NAME_INDEX < SITE_DATA_FIELDS_COUNT && USAGE_COUNT_COLUMN_INDEX < SITE_DATA_FIELDS_COUNT,
                  "SITE_* index constants out of bounds relative to SITE_DATA_FIELDS_COUNT");

    if (siteData[siteIdx][SITE_NAME_INDEX].length() > 0)
    {
        const String& usageStr = siteData[siteIdx][USAGE_COUNT_COLUMN_INDEX];

        // small helper to check that the string is a non-empty sequence of digits
        auto is_all_digits = [](const String& s) -> bool
        {
            if (s.length() == 0) return false;
            for (unsigned int i = 0; i < s.length(); ++i)
            {
                if (!isDigit(static_cast<uint8_t>(s[i]))) return false;
            }
            return true;
        };

        if (!is_all_digits(usageStr))
        {
            siteData[siteIdx][USAGE_COUNT_COLUMN_INDEX] = "0";
        }
    }

    // if constexpr (SITE_NAME_INDEX >= 0 && USAGE_COUNT_COLUMN_INDEX >= 0 &&
    //     SITE_NAME_INDEX < SITE_DATA_FIELDS_COUNT && USAGE_COUNT_COLUMN_INDEX < SITE_DATA_FIELDS_COUNT)
    // {
    //     if (siteData[siteIdx][SITE_NAME_INDEX].length() > 0)
    //     {
    //         if (siteData[siteIdx][USAGE_COUNT_COLUMN_INDEX].toInt() == 0 &&
    //             siteData[siteIdx][USAGE_COUNT_COLUMN_INDEX] != "0")
    //         {
    //             siteData[siteIdx][USAGE_COUNT_COLUMN_INDEX] = "0";
    //         }
    //     }
    // }
}

/*! \brief Loads site data from the SD card into the siteData array.
 *  Prioritizes loading from the encrypted file (.enc). If not found or fails,
 *  loads from the plain CSV file (.csv), encrypts it, and optionally deletes
 *  the plain file. If neither is found, initializes an empty array.
 * \param baseFilePath The base name of the file (e.g., "passwords.csv").
 * \return bool True if data was loaded or initialized, false on critical SD error.
 */
bool SDFile_to_Array(const char* baseFilePath)
{
    const String encryptedFilePath = String(baseFilePath) + ".enc";
    const auto plainFilePath = String(baseFilePath);
    bool loadedData = false;
    File dataFile;

    // Ensure display is not selected while using SD card
    digitalWrite(TFT_CS, HIGH);

    // Delete encrypted file if both .txt and .enc exists. User may have added an updated passwords.txt file.
    if (SD.exists(plainFilePath.c_str()) && SD.exists(encryptedFilePath.c_str()))
    {
        SD.remove(encryptedFilePath.c_str());
    }

    if (SD.exists(encryptedFilePath.c_str()))
    {
        dataFile = SD.open(encryptedFilePath.c_str(), FILE_READ);
        if (dataFile)
        {
            uint16_t site = 0;
            // uint16_t item = 0; // item is handled by parseCSVLine
            String currentLine = "";

            while (dataFile.available() && site < MaxSites)
            {
                const char encryptedChar = dataFile.read();
                //  Decryption: First XOR, then rotate right to fully restore original character
                const char charAfterXor = encryptedChar ^ ENCRYPTION_KEY;

                if (const char fullyDecryptedChar = rotateRight(charAfterXor, ROTATION_COUNT); fullyDecryptedChar ==
                    '\n')
                {
                    currentLine.trim(); // Remove any \r or other whitespace
                    parseCSVLine(currentLine, site);
                    currentLine = ""; // Reset for next line
                    if (++site >= MaxSites)
                        break; // Max sites reached
                }
                else if (fullyDecryptedChar == '\r')
                {
                    // do nothing
                }
                else
                {
                    currentLine += fullyDecryptedChar;
                }
            }
            // Process any remaining data in currentLine if EOF is reached before \n
            if (currentLine.length() > 0 && site < MaxSites)
            {
                parseCSVLine(currentLine, site);
                site++; // Increment site count as this line is processed
            }
            dataFile.close();
            loadedData = true;
            LCD.locate(4, 30);
            LCD.print(F("Secure load OK    "));
            LCD.copy_to_lcd();
            delay(2000);
            return true;
        }
    }

    if (!loadedData && SD.exists(plainFilePath.c_str()))
    {
        dataFile = SD.open(plainFilePath.c_str(), FILE_READ);
        if (dataFile)
        {
            uint16_t site = 0;
            while (dataFile.available() && site < MaxSites)
            {
                String line = dataFile.readStringUntil('\n');
                parseCSVLine(line, site);

                if (++site >= MaxSites)
                    break;
            }
            dataFile.close();
            loadedData = true;
            LCD.locate(4, 30);
            LCD.print(F(".csv File load OK"));
            LCD.copy_to_lcd();
            LCD.locate(4, 40);
            LCD.print(F("Securing data..."));
            LCD.copy_to_lcd();
            // Encrypt plainFilePath to encryptedFilePath, then delete plainFilePath
            if (fileEncrypt(plainFilePath.c_str(), encryptedFilePath.c_str(), DELETE_UNENCRYPTED_FILE))
            {
                LCD.print(F("OK"));
                LCD.copy_to_lcd();
            }
            else
            {
                LCD.print(F("FAIL"));
                LCD.copy_to_lcd();
            }
            delay(2000);
            return true;
        }
    }

    if (!loadedData)
    {
        LCD.locate(4, 20);
        LCD.print(F("No valid file found."));
        LCD.copy_to_lcd();
        LCD.locate(4, 30);
        LCD.print(F("Initializing empty."));
        LCD.copy_to_lcd();
        // Initialize siteData to empty strings
        //for (int s = 0; s < MaxSites; ++s)
        for (auto& s : siteData)
        {
            for (int i = 0; i < SITE_DATA_FIELDS_COUNT; ++i)
            {
                if (i == USAGE_COUNT_COLUMN_INDEX)
                {
                    s[i] = "0";
                }
                else
                {
                    s[i] = "";
                }
            }
        }
        loadedData = true;
        delay(2000);
        return true;
    }

    digitalWrite(SD_CS_PIN, HIGH);
    return false; // Should not reach here if one of the above blocks executed fully
}

/*! \brief Sends a string as a series of keystrokes via the USB keyboard.
 *  Iterates through the input string and types each character.
 *  Includes small delays between keystrokes for reliability.
 * \param textToSend The string to be typed out.
 * \return void
 */
void sendStringAsKeystrokes(const String& textToSend)
{
    delay(50); // Small delay before typing, helps host computer readiness

    for (unsigned int i = 0; i < textToSend.length(); i++)
    {
        if (const char c = textToSend.charAt(i); Keyboard.write(c) == 0)
        {
            break; // Stop trying to send if there's an error
        }
        delay(50); // Delay between keystrokes (adjust as needed for reliability with the host)
    }
}

/*! \brief Do rotary encoder decoding
 * \return void
 */
void doCore1Tasks()
{
    // Initialize last_raw_pio_for_app_step based on the initial physical PIO count.
    // This makes the current physical position the reference for the first 2-indent step.
    int32_t last_raw_pio_for_app_step = quadrature_encoder_get_count(pio, sm);

    // Initialize to the current newPos value to prevent a spurious blip on the first iteration
    // if newPos starts non-zero and different from a fixed initial old_value.
    int32_t last_newPos_value_for_blip = newPos.load(std::memory_order_acquire);

    // const int32_t RAW_COUNTS_PER_APP_STEP = 8; // 4 PIO counts/indent * 2 indents

    while (true)
    {
        //Do the encoder reading and processing
        int32_t current_raw_pio = quadrature_encoder_get_count(pio, sm);

        // This is the application-level position that Core1 aims to update.
        // It's loaded once per iteration. If Core0 changes it, Core1 will see it in the next iteration,
        // or immediately if a resetPos is also flagged.
        const int32_t current_logical_app_pos = newPos.load(std::memory_order_acquire);

        if (resetPos.load(std::memory_order_acquire)) // Ensure visibility of Core0's writes
        {
            reset_pio_y_register(pio, sm);
            current_raw_pio = quadrature_encoder_get_count(pio, sm); // Get fresh count after PIO reset

            // Align last_raw_pio_for_app_step with the new physical PIO state.
            // The current_logical_app_pos (which was set by Core0) is the new baseline.
            // The next full step of RAW_COUNTS_PER_APP_STEP from current_raw_pio will modify this baseline.
            last_raw_pio_for_app_step = current_raw_pio;

            // current_logical_app_pos already holds the value Core0 wants.
            // Update last_newPos_value_for_blip to this reset position.
            last_newPos_value_for_blip = current_logical_app_pos;

            resetPos.store(false, std::memory_order_release);
        }

        const int32_t delta_raw = current_raw_pio - last_raw_pio_for_app_step;

        // This will be the new value for newPos if a change is warranted.
        int32_t next_logical_app_pos = current_logical_app_pos;
        bool app_pos_updated_this_cycle = false;

        if (constexpr int32_t RAW_COUNTS_PER_APP_STEP = 4; delta_raw >= RAW_COUNTS_PER_APP_STEP)
        {
            const int num_steps = delta_raw / RAW_COUNTS_PER_APP_STEP;
            next_logical_app_pos += num_steps;
            last_raw_pio_for_app_step += num_steps * RAW_COUNTS_PER_APP_STEP;
            app_pos_updated_this_cycle = true;
        }
        else if (delta_raw <= -RAW_COUNTS_PER_APP_STEP)
        {
            const int num_steps = -delta_raw / RAW_COUNTS_PER_APP_STEP; // num_steps is positive
            next_logical_app_pos -= num_steps;
            last_raw_pio_for_app_step -= num_steps * RAW_COUNTS_PER_APP_STEP;
            app_pos_updated_this_cycle = true;
        }

        if (app_pos_updated_this_cycle)
        {
            newPos.store(next_logical_app_pos, std::memory_order_release);
            if (last_newPos_value_for_blip != next_logical_app_pos)
            {
                blipTone.store(true, std::memory_order_release);
                last_newPos_value_for_blip = next_logical_app_pos;
            }
        }
        sleep_ms(1); // Optional: uncomment if yielding or reducing CPU usage is desired.
    }
}

/*! \brief Rotates bits in a byte to the left.
 * \param value The byte to rotate.
 * \param shift The number of positions to rotate.
 * \return The rotated byte.
 */
uint8_t rotateLeft(const uint8_t value, int shift)
{
    shift %= 8; // Ensure shift is within 0-7
    if (shift == 0)
        return value;
    return (value << shift) | (value >> (8 - shift));
}

/*! \brief Rotates bits in a byte to the right.
 * \param value The byte to rotate.
 * \param shift The number of positions to rotate.
 * \return The rotated byte.
 */
uint8_t rotateRight(uint8_t value, int shift)
{
    shift %= 8; // Ensure shift is within 0-7
    if (shift == 0)
        return value;
    return (value >> shift) | (value << (8 - shift));
}

/*! \brief Encrypts a file on the SD card using XOR encryption and optionally deletes the original.
 * \param sourceFilename The path to the file to encrypt.
 * \param destEncryptedFilename The path to write the encrypted file to.
 * \param deleteSource If true, the original file will be deleted after successful encryption.
 * \return True if encryption was successful, false otherwise.
 */
bool fileEncrypt(const char* sourceFilename, const char* destEncryptedFilename, bool deleteSource)
{
    digitalWrite(TFT_CS, HIGH); // Ensure display CS is high to prevent SPI conflict

    File sourceFile = SD.open(sourceFilename, FILE_READ);
    if (!sourceFile)
    {
        digitalWrite(SD_CS_PIN, HIGH); // Deselect SD card
        return false;
    }

    // If the destination encrypted file exists, remove it first.
    // This is crucial if destEncryptedFilename is the same as a previous encrypted file.
    if (SD.exists(destEncryptedFilename))
    {
        if (!SD.remove(destEncryptedFilename))
        {
            sourceFile.close();
            digitalWrite(SD_CS_PIN, HIGH); // Deselect SD card
            return false;
        }
    }

    File destFile = SD.open(destEncryptedFilename, FILE_WRITE);
    if (!destFile)
    {
        sourceFile.close();
        digitalWrite(SD_CS_PIN, HIGH); // Deselect SD card
        return false;
    }

    constexpr size_t bufferSize = 64; // Process file in chunks
    uint8_t buffer[bufferSize];
    int bytesRead;

    while ((bytesRead = sourceFile.read(buffer, bufferSize)) > 0)
    {
        for (int i = 0; i < bytesRead; i++)
        {
            buffer[i] = rotateLeft(buffer[i], ROTATION_COUNT); // Rotate before XOR
            buffer[i] = buffer[i] ^ ENCRYPTION_KEY; // Apply XOR encryption
        }
        if (destFile.write(buffer, bytesRead) != static_cast<size_t>(bytesRead))
        {
            // Log error to LCD
            // LCD.locate(0, 40); LCD.print(F("Write err")); LCD.copy_to_lcd(); delay(2000);
            sourceFile.close();
            destFile.close();
            SD.remove(destEncryptedFilename); // Attempt to clean up partially written encrypted file
            digitalWrite(SD_CS_PIN, HIGH); // Deselect SD card
            return false;
        }
    }

    sourceFile.close();
    destFile.close();

    if (deleteSource)
    {
        if (!SD.remove(sourceFilename))
        {
            LCD.cls();
            LCD.locate(0, 40);
            LCD.print(F("Del orig err!!!"));
            LCD.copy_to_lcd();
            while (true)
            {
            }
        }
    }
    digitalWrite(SD_CS_PIN, HIGH); // Deselect SD card
    return true;
}

/*! \brief Saves the current siteData array to an encrypted file on the SD card.
 *  \param baseFilePath The base name of the file (e.g., "passwords.csv").
 *  Writes to a temporary file, then encrypts it to the final .enc file.
 */
bool saveSiteDataToEncryptedFile(const char* baseFilePath)
{
    digitalWrite(TFT_CS, HIGH); // Ensure display CS is high

    const String plainFilePathTemp = String(baseFilePath) + ".tmp";
    const String finalEncryptedPath = String(baseFilePath) + ".enc";

    File tempFile = SD.open(plainFilePathTemp.c_str(), FILE_WRITE);

    if (!tempFile)
    {
        // LCD.cls(); LCD.locate(0,20); LCD.print(F("Err open .tmp")); LCD.copy_to_lcd(); delay(2000);
        digitalWrite(SD_CS_PIN, HIGH);
        return false;
    }

    //for (int i = 0; i < MaxSites; ++i)
    for (const auto& i : siteData)
    {
        if (i[SITE_NAME_INDEX].length() == 0)
        {
            // Stop if site name is empty (end of actual data)
            break;
        }
        for (int j = 0; j < SITE_DATA_FIELDS_COUNT; ++j)
        {
            tempFile.print(i[j]);
            if (j < SITE_DATA_FIELDS_COUNT - 1)
            {
                tempFile.print(",");
            }
        }
        tempFile.println();
    }
    tempFile.close();

    // Encrypt plainFilePathTemp to finalEncryptedPath, then delete plainFilePathTemp
    const bool success = fileEncrypt(plainFilePathTemp.c_str(), finalEncryptedPath.c_str(), true);
    // true to delete .tmp

    digitalWrite(SD_CS_PIN, HIGH);
    return success;
}

/*! \brief Swaps two rows in the global siteData array.
 *  \param r1 Index of the first row to swap.
 *  \param r2 Index of the second row to swap.
 */
void swapSiteRows(const int r1, const int r2)
{
    if (r1 == r2 || r1 < 0 || r1 >= MaxSites || r2 < 0 || r2 >= MaxSites)
        return;
    for (int j = 0; j < SITE_DATA_FIELDS_COUNT; ++j)
    {
        std::swap(siteData[r1][j], siteData[r2][j]);
    }
}

/*! \brief Sorts the siteData array.
 *  Primary sort key is usage count (descending).
 *  Secondary sort key is site name (alphabetical ascending) for ties in usage count.
 */
void sortSiteData()
{
    int n = 0; // Count actual number of sites with data
    //for (int i = 0; i < MaxSites; ++i)
    for (const auto& i : siteData)
    {
        if (i[SITE_NAME_INDEX].length() == 0)
            break;
        n++;
    }
    if (n <= 1)
        return; // No need to sort

    // Selection Sort:
    for (int i = 0; i < n - 1; i++)
    {
        int best_idx = i;
        for (int j = i + 1; j < n; j++)
        {
            const long usageJ = siteData[j][USAGE_COUNT_COLUMN_INDEX].toInt();

            if (const long usageBest = siteData[best_idx][USAGE_COUNT_COLUMN_INDEX].toInt(); usageJ > usageBest ||
                (usageJ == usageBest && siteData[j][SITE_NAME_INDEX].compareTo(siteData[best_idx][SITE_NAME_INDEX]) <
                    0))
            {
                best_idx = j;
            }

            //if sorting does not work, restore this previous logic:
            // if (const long usageBest = siteData[best_idx][USAGE_COUNT_COLUMN_INDEX].toInt(); usageJ > usageBest)
            //     best_idx = j;
            // else if (usageJ == usageBest && siteData[j][SITE_NAME_INDEX].compareTo(siteData[best_idx][SITE_NAME_INDEX])< 0)
            //     best_idx = j;
        }
        if (best_idx != i)
            swapSiteRows(i, best_idx);
    }
}

/*! \brief Streams the decrypted content of the passwords file via Serial.
 *  Waits for user confirmation before starting, and allows cancellation.
 * \param
 * \return void
 */
void streamOut()
{
    // Reset button flags
    printButtonPressed.store(false, std::memory_order_release);
    enterButtonPressed.store(false, std::memory_order_release);

    // Inform user
    LCD.cls();
    drawScreenBorder();
    LCD.locate(10, 20);
    LCD.print(F("Streaming out data..."));
    // LCD.locate(0, 30);
    // LCD.print(F("Press PRN to cancel"));
    LCD.copy_to_lcd();

    // Ensure LCD not selected during SD access
    digitalWrite(TFT_CS, HIGH);

    String encryptedFilePath = String(SD_FILENAME) + ".enc";
    File dataFile = SD.open(encryptedFilePath.c_str(), FILE_READ);

    if (!dataFile)
    {
        LCD.cls();
        LCD.locate(0, 20);
        LCD.print(F("Error: Encrypted"));
        LCD.locate(0, 30);
        LCD.print(F("file not found!"));
        LCD.copy_to_lcd();
        delay(3000);
        digitalWrite(SD_CS_PIN, HIGH);
        drawHomeScreen(positionHold);
        return;
    }

    // Begin streaming decrypted lines to Serial
    String currentLine = "";

    while (dataFile.available())
    {
        const char encryptedChar = dataFile.read();
        const char afterXor = encryptedChar ^ ENCRYPTION_KEY;

        if (const char dec = static_cast<char>(rotateRight(static_cast<uint8_t>(afterXor), ROTATION_COUNT)); dec ==
            '\n')
        {
            // Trim any stray CR and send the completed line
            currentLine.trim();
            Serial.println(currentLine);
            currentLine = "";

            // Allow host/read cancellation
            if (printButtonPressed)
                break;
        }
        else if (dec == '\r')
        {
            // ignore CR
        }
        else
        {
            currentLine += dec;
        }

        // Small yield to allow button ISR to run and host to process data
        sleep_ms(1);
    }

    // If there is a final line without trailing newline, send it
    //if (currentLine.length() > 0 && !printButtonPressed)
    if (currentLine.length() > 0)
    {
        currentLine.trim();
        Serial.println(currentLine);
    }

    //Serial.println(F("END_PASSWORDS_STREAM")); // Optional footer marker

    dataFile.close();
    digitalWrite(SD_CS_PIN, HIGH);

    // Feedback to user
    LCD.cls();
    LCD.locate(10, 20);
    LCD.print(F("Streaming complete"));
    LCD.copy_to_lcd();
    delay(1500);

    // Restore UI
    LCD.cls();
    drawScreenBorder();
    drawHomeScreen(positionHold);
    resetInactivityTimer();
}

// Language: C++
/*! \brief Receive CSV over Serial, replace encrypted passwords file with backup and load.
 */
void streamIn()
{
    digitalWrite(TFT_CS, HIGH); // Deselect display during SD access

    // UI: show receiving message
    LCD.cls();
    drawScreenBorder();
    LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 12);
    LCD.print(F("Receiving CSV..."));
    // LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 24);
    // LCD.print(F("Send data now"));
    LCD.copy_to_lcd();

    // Prepare file paths
    String basePath = String(SD_FILENAME); // e.g. `passwords.csv`
    String tempPlainPath = basePath + String(".tmp"); // e.g. `passwords.csv.tmp`
    String finalEncryptedPath = basePath + String(".enc"); // e.g. `passwords.csv.enc`

    // Open temp file for incoming plain CSV
    File tempFile = SD.open(tempPlainPath.c_str(), FILE_WRITE);
    if (!tempFile)
    {
        LCD.cls();
        drawScreenBorder();
        LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 18);
        LCD.print(F("Err open tmp"));
        LCD.copy_to_lcd();
        delay(2000);
        digitalWrite(SD_CS_PIN, HIGH);
        return;
    }

    // Read serial into temp file. Use idle timeout after first byte and an overall timeout.
    constexpr unsigned long overallTimeout = 30000; // 30s max
    constexpr unsigned long idleAfterDataTimeout = 2000; // 2s no-data -> finish
    unsigned long startTime = millis();
    unsigned long lastDataTime = 0;
    bool receivedAny = false;

    while (millis() - startTime < overallTimeout)
    {
        while (Serial.available() > 0)
        {
            int v = Serial.read();
            if (v >= 0)
            {
                tempFile.write(static_cast<uint8_t>(v));
                receivedAny = true;
                lastDataTime = millis();
            }
        }

        if (receivedAny && (millis() - lastDataTime) > idleAfterDataTimeout)
        {
            // assume transfer finished
            break;
        }

        sleep_ms(10);
    }

    tempFile.close();

    if (!receivedAny)
    {
        // No data received
        SD.remove(tempPlainPath.c_str());
        LCD.cls();
        drawScreenBorder();
        LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 18);
        LCD.print(F("No data received"));
        LCD.copy_to_lcd();
        delay(1500);
        digitalWrite(SD_CS_PIN, HIGH);
        return;
    }

    // Backup existing encrypted file if present
    auto backupPath = String();
    bool backedUp = false;
    if (SD.exists(finalEncryptedPath.c_str()))
    {
        // Build timestamp
        datetime_t t;
        String ts;
        if (rtc_get_datetime(&t))
        {
            char buf[32];
            sprintf(buf, "%04d%02d%02d_%02d%02d%02d", t.year, t.month, t.day, t.hour, t.min, t.sec);
            ts = String(buf);
        }
        else
        {
            // Fallback to millis timestamp
            ts = String(millis());
        }

        backupPath = finalEncryptedPath + String(".") + ts; // e.g. passwords.csv.enc.20250101_123456

        // Try to rename first
        if (SD.rename(finalEncryptedPath.c_str(), backupPath.c_str()))
        {
            backedUp = true;
        }
        else
        {
            // Fallback: copy file contents then remove original
            if (File src = SD.open(finalEncryptedPath.c_str(), FILE_READ))
            {
                if (File dst = SD.open(backupPath.c_str(), FILE_WRITE))
                {
                    constexpr size_t bufSize = 64;
                    uint8_t buf[bufSize];
                    int r;
                    while ((r = src.read(buf, bufSize)) > 0)
                    {
                        if (dst.write(buf, r) != static_cast<size_t>(r))
                        {
                            break;
                        }
                    }
                    dst.close();
                    // Verify backup exists/has size > 0
                    if (File chk = SD.open(backupPath.c_str(), FILE_READ))
                    {
                        backedUp = (chk.size() > 0);
                        chk.close();
                    }
                }
                src.close();
            }

            if (backedUp)
            {
                SD.remove(finalEncryptedPath.c_str());
            }
            else
            {
                // Backup failed: cleanup incoming tmp and abort
                SD.remove(tempPlainPath.c_str());
                LCD.cls();
                drawScreenBorder();
                LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 18);
                LCD.print(F("Backup failed"));
                LCD.copy_to_lcd();
                delay(2000);
                digitalWrite(SD_CS_PIN, HIGH);
                return;
            }
        }
    }

    // Encrypt received plain CSV into final encrypted file, deleting the plain temp on success
    LCD.cls();
    drawScreenBorder();
    LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 12);
    LCD.print(F("Securing data..."));
    LCD.copy_to_lcd();

    if (bool encryptOk = fileEncrypt(tempPlainPath.c_str(), finalEncryptedPath.c_str(), true); !encryptOk)
    {
        // Attempt to restore backup if we made one
        if (backedUp)
        {
            // restore backup -> rename back
            if (!SD.rename(backupPath.c_str(), finalEncryptedPath.c_str()))
            {
                // try copy back
                if (File src = SD.open(backupPath.c_str(), FILE_READ))
                {
                    if (File dst = SD.open(finalEncryptedPath.c_str(), FILE_WRITE))
                    {
                        constexpr size_t bufSize = 64;
                        uint8_t buf[bufSize];
                        int r;
                        while ((r = src.read(buf, bufSize)) > 0)
                        {
                            dst.write(buf, r);
                        }
                        dst.close();
                    }
                    src.close();
                }
            }
        }

        LCD.cls();
        drawScreenBorder();
        LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 18);
        LCD.print(F("Encrypt FAIL"));
        LCD.copy_to_lcd();
        delay(2000);
        digitalWrite(SD_CS_PIN, HIGH);
        return;
    }

    // Load the new encrypted file into memory (decrypt & parse)
    LCD.cls();
    drawScreenBorder();
    LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 12);
    LCD.print(F("Loading new data..."));
    LCD.copy_to_lcd();

    if (bool loadOk = SDFile_to_Array(basePath.c_str()); !loadOk)
    {
        // Loading failed, attempt to restore backup if available
        if (backedUp)
        {
            SD.remove(finalEncryptedPath.c_str()); // remove the bad encrypted file
            if (!SD.rename(backupPath.c_str(), finalEncryptedPath.c_str()))
            {
                // try copy back
                if (File src = SD.open(backupPath.c_str(), FILE_READ))
                {
                    if (File dst = SD.open(finalEncryptedPath.c_str(), FILE_WRITE))
                    {
                        constexpr size_t bufSize = 64;
                        uint8_t buf[bufSize];
                        int r;
                        while ((r = src.read(buf, bufSize)) > 0)
                            dst.write(buf, r);
                        dst.close();
                    }
                    src.close();
                }
            }

            // load again
            SDFile_to_Array(basePath.c_str());
        }

        LCD.cls();
        drawScreenBorder();
        LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 18);
        LCD.print(F("Load Failed"));
        LCD.copy_to_lcd();
        delay(2000);
        digitalWrite(SD_CS_PIN, HIGH);
        return;
    }
    digitalWrite(SD_CS_PIN, HIGH);
    sortSiteData();

    // Success feedback
    LCD.cls();
    drawScreenBorder();
    LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 12);
    LCD.print(F("Receive complete"));
    LCD.locate(BORDER_WIDTH + 4, BORDER_WIDTH + 24);
    if (backedUp)
        LCD.print(F("Backup saved"));
    else
        LCD.print(F("No prior file"));
    LCD.copy_to_lcd();
    delay(1500);

    // Restore UI to home
    LCD.cls();
    drawScreenBorder();
    drawHomeScreen(positionHold);
    resetInactivityTimer();
}


/*! \brief Types out the entire decrypted content of the passwords file via USB Keyboard.
 * \param
 * \return void
 */
void typeOut()
{
    printButtonPressed.store(false, std::memory_order_release);
    enterButtonPressed.store(false, std::memory_order_release);
    LCD.cls();
    LCD.locate(0, 20);
    LCD.print(F("Data Type out.  .."));
    LCD.copy_to_lcd();
    sleep_ms(500);

    digitalWrite(TFT_CS, HIGH); // Ensure LCD is not selected during SD operations

    const String encryptedFilePath = String(SD_FILENAME) + ".enc";
    File dataFile = SD.open(encryptedFilePath.c_str(), FILE_READ);

    if (!dataFile)
    {
        LCD.cls();
        LCD.locate(0, 20);
        LCD.print(F("Error: Encrypted"));
        LCD.locate(0, 30);
        LCD.print(F("file not found!"));
        LCD.copy_to_lcd();
        delay(3000);
        digitalWrite(SD_CS_PIN, HIGH); // Deselect SD card
        drawHomeScreen(positionHold); // Or current screen
        return;
    }

    LCD.locate(0, 20);
    LCD.print(F("Confirm to continue or,"));
    LCD.locate(0, 30);
    LCD.print(F("Press ENC BTN to stop"));
    LCD.copy_to_lcd();

    while (!printButtonPressed && !enterButtonPressed)
    {
        sleep_ms(100);
    }
    printButtonPressed = false;
    enterButtonPressed = false;

    while (dataFile.available())
    {
        if (enterButtonPressed)
        {
            // Check if encoder button was pressed to abort
            enterButtonPressed = false; // Reset flag
            LCD.locate(0, 40);
            LCD.print(F("Typing aborted!"));
            LCD.copy_to_lcd();
            delay(1500);
            break;
        }
        char encryptedChar = dataFile.read();
        char charAfterXor = encryptedChar ^ ENCRYPTION_KEY;
        char fullyDecryptedChar = rotateRight(charAfterXor, ROTATION_COUNT);

        if (fullyDecryptedChar == '\n')
        {
            Keyboard.write(KEY_RETURN);
        }
        else if (fullyDecryptedChar == '\r')
        {
            // Ignore carriage return, as \n handles the line break
        }
        else
        {
            Keyboard.write(fullyDecryptedChar);
        }
        delay(60); // Delay between keystrokes for reliability
    }
    dataFile.close();
    digitalWrite(SD_CS_PIN, HIGH); // Deselect SD card

    LCD.cls();
    LCD.locate(0, 20);
    if (!dataFile.available() && !enterButtonPressed)
    {
        // Check if loop completed naturally
        LCD.print(F("Done typing."));
    }
    LCD.copy_to_lcd();
    delay(2000);

    // Restore the previous screen (assuming home screen for simplicity here)
    // You might want to store and restore the actual currentScreen state
    currentScreen = SCREEN_HOME; // Or SCREEN_DETAILS if that was active
    drawHomeScreen(positionHold); // Redraw the screen
}

/*! \brief Resets the Y scratch register of a PIO state machine to zero.
 *  This function temporarily halts the specified state machine, executes an
 *  instruction to set its Y register to 0, clears its FIFOs, restarts it
 *  from its initial program counter, and then re-enables it.
 * \param pio_instance The PIO instance (e.g., pio0, pio1).
 * \param sm_instance The state machine number (0-3) on the PIO instance.
 * \return void
 */
void reset_pio_y_register(PIO pio_instance, uint sm_instance)
{
    // 1. Disable the State Machine to halt its execution.
    pio_sm_set_enabled(pio_instance, sm_instance, false);

    // 2. Execute the 'mov y, 0' instruction.
    //    The instruction is 0xa040. This will set the Y scratch register to 0.
    //    This executes on the stalled state machine.
    pio_sm_exec(pio_instance, sm_instance, 0xa040);

    // 3. Clear the RX and TX FIFOs associated with the state machine.
    //    This removes any data that might have been pushed before or during the reset.
    pio_sm_clear_fifos(pio_instance, sm_instance);

    // 4. Restart the State Machine.
    //    This resets the program counter to the state machine's initial configured PC.
    //    For your quadrature_encoder program, it will start from its beginning.
    pio_sm_restart(pio_instance, sm_instance);

    // 5. Re-enable the State Machine.
    pio_sm_set_enabled(pio_instance, sm_instance, true);
}

/*! \brief Converts a Unix timestamp to a datetime_t structure.
 *  \param ts The Unix timestamp (seconds since epoch).
 *  \param dt Pointer to the datetime_t structure to be filled.
 *  \return True if conversion was successful, false otherwise.
 */
bool unix_to_datetime(const time_t ts, datetime_t* dt)
{
    struct tm ti{};
    // Use gmtime_r for thread-safety
    if (gmtime_r(&ts, &ti) == nullptr)
    {
        return false; // Prefer to fail if gmtime_r isn't working as expected
    }

    dt->year = static_cast<int16_t>(ti.tm_year + 1900);
    dt->month = static_cast<int8_t>(ti.tm_mon + 1);
    dt->day = static_cast<int8_t>(ti.tm_mday);
    dt->dotw = static_cast<int8_t>(ti.tm_wday); // 0=Sunday, 1=Monday, ..., 6=Saturday
    dt->hour = static_cast<int8_t>(ti.tm_hour);
    dt->min = static_cast<int8_t>(ti.tm_min);
    dt->sec = static_cast<int8_t>(ti.tm_sec);
    return true;
}

bool syncRTCWithDS3231()
{
    datetime_t dt;
    rtc_get_datetime(&dt); // pico RTC call
    if (dt.year < 2025 || dt.year > 2125) //pico rtc not set up -
    {
        //pico rtc not set up -- check if DS3231 is set up
        ts t{};
        DS3231_get(&t);
        if (t.year < 2025 || t.year > 2125)
        {
            return false; // DS3231 also not set up - return for normal handling
            setLedColor(LED_STATE_RED);
        }
        else
        {
            //load from DS3231 to pico RTC
            dt.day = static_cast<int8_t>(t.mday);
            dt.month = static_cast<int8_t>(t.mon);
            dt.dotw = static_cast<int8_t>(t.wday);
            dt.hour = static_cast<int8_t>(t.hour);
            dt.min = static_cast<int8_t>(t.min);
            dt.sec = static_cast<int8_t>(t.sec);
            dt.year = t.year;

            //set pico rtc
            rtc_init(); // Initialize RTC
            rtc_set_datetime(&dt);
            rtc_get_datetime(&dt); // pico RTC call
            setLedColor(LED_STATE_BLUE);
            return true; //pico rtc set up from DS3231
        }
    }
    return true;
}

/*! \brief Handles RTC synchronization with a host application over serial.
 *  Communicates with a host (e.g., Python script) to get the current Unix time.
 *  Sets the Pico's RTC after adjusting for the local timezone.
 */
bool syncRTCWithHost()
{
    constexpr unsigned long timeoutMillis = 20000; // 20 seconds timeout for sync
    unsigned long startTime = millis();
    bool timeReceived = false;
    String timeStr = "";

    LCD.cls();
    drawScreenBorder();
    constexpr int content_padding = 2;
    constexpr int text_x = BORDER_WIDTH + content_padding;
    constexpr int line_height = 7 + 3; // Small font
    constexpr int y_line0 = BORDER_WIDTH + content_padding;
    constexpr int y_line1 = y_line0 + line_height;
    constexpr int y_line2 = y_line1 + line_height;
    constexpr int y_line3 = y_line2 + line_height;
    constexpr int y_line4 = y_line3 + line_height;

    LCD.locate(text_x, y_line0);
    LCD.print(F("RTC Sync with Host:"));
    LCD.locate(text_x, y_line1);
    LCD.print(F("Waiting for Host..."));
    LCD.copy_to_lcd();

    // Wait for the serial port to be truly ready and for the host to potentially connect
    // This loop also gives time for the user to start the vb PicoTimeSync app if it wasn't already running.
    const unsigned long waitHostStartTime = millis();
    bool hostAckReceived = false;
    constexpr unsigned long hostConnectTimeout = 25000; // 15 seconds for host to acknowledge

    while (millis() - waitHostStartTime < hostConnectTimeout)
    {
        if (!Serial)
        {
            // Check if USB Serial got disconnected or not yet fully initialized
            delay(100); // Wait a bit
            continue; // And retry
        }
        Serial.println(F("PICO_READY")); // Periodically send ready signal
        LCD.locate(text_x, y_line2); // Update line 2
        LCD.print(F("Sent PICO_READY   ")); // Pad with spaces to clear previous
        LCD.copy_to_lcd();

        const unsigned long ackWaitStartTime = millis();
        while (millis() - ackWaitStartTime < 1000)
        {
            // Wait 1 second for HOST_ACK after sending PICO_READY
            if (Serial.available() > 0)
            {
                String ack = Serial.readStringUntil('\n');
                ack.trim();
                if (ack.equals(F("HOST_ACK")))
                {
                    hostAckReceived = true;
                    LCD.locate(text_x, y_line3); // Update line 3
                    LCD.print(F("Host ACK Received! ")); // Pad
                    LCD.copy_to_lcd();
                    delay(500);
                    break;
                }
            }
            delay(10);
        }
        if (hostAckReceived)
            break;
        delay(1000); // Wait a second before resending PICO_READY
    }

    if (!hostAckReceived)
    {
        LCD.locate(text_x, y_line3); // Update line 3
        LCD.print(F("Host Connect Timeout")); // Pad
        LCD.copy_to_lcd();
        return false;
    }

    // Host is connected, now request time
    Serial.println(F("SYNC_TIME_REQUEST")); // Send time sync request to host
    LCD.locate(text_x, y_line2); // Update line 2
    LCD.print(F("Req Time, Wait Resp ")); // Pad
    LCD.copy_to_lcd();

    startTime = millis(); // Reset startTime for this specific timeout
    while (millis() - startTime < timeoutMillis) // Use original timeoutMillis
    {
        if (Serial.available() > 0)
        {
            timeStr = Serial.readStringUntil('\n');
            timeStr.trim();
            if (timeStr.length() > 0)
            {
                timeReceived = true;
                break;
            }
        }
        delay(10); // Small delay to allow host to respond and not busy-wait
    }

    if (timeReceived)
    {
        LCD.locate(text_x, y_line3); // Update line 3
        LCD.print(F("Received: "));
        LCD.print(timeStr.substring(0, 16 - strlen("Received: "))); // Truncate if too long, ensure fits
        LCD.copy_to_lcd();
        delay(1000);
        if (time_t unix_ts = timeStr.toInt(); unix_ts > 0)
        {
            // Basic validation
            unix_ts += 7200L; // adjust for GMT+2 (2 * 60 * 60 = 7200 seconds.)
            datetime_t dt;
            if (unix_to_datetime(unix_ts, &dt))
            {
                rtc_init(); // Initialize RTC
                if (rtc_set_datetime(&dt))
                {
                    //also set DS3231
                    ts t{};
                    t.hour = dt.hour;
                    t.min = dt.min;
                    t.sec = dt.sec;
                    t.mday = dt.day;
                    t.mon = dt.month;
                    t.year = dt.year;
                    t.wday = dt.dotw;
                    DS3231_set(t);

                    LCD.locate(text_x, y_line4); // Update line 4
                    LCD.print(F("RTC/DS3231 Set OK! ")); // Pad
                    LCD.copy_to_lcd();
                    return true;
                }
                else
                {
                    LCD.locate(text_x, y_line4); // Update line 4
                    LCD.print(F("RTC Set Fail!      ")); // Pad
                    LCD.copy_to_lcd();
                }
            }
            else
            {
                LCD.locate(text_x, y_line4); // Update line 4
                LCD.print(F("Time Conv Fail!    ")); // Pad
                LCD.copy_to_lcd();
            }
        }
        else
        {
            LCD.locate(text_x, y_line4); // Update line 4
            LCD.print(F("Invalid TimeVal!   ")); // Pad
            LCD.copy_to_lcd();
        }
    }
    else
    {
        LCD.locate(text_x, y_line3); // Update line 3
        LCD.print(F("Sync Timeout!      ")); // Pad
        LCD.copy_to_lcd();
    }
    return false;
}

/*! \brief Handles the complete PIN entry and validation process.
 *  Manages the user interface for entering a 4-digit PIN, compares it against
 *  the stored PIN, and handles incorrect attempts.
 *  \return True if the correct PIN is entered within the allowed attempts,
 *          false otherwise (e.g., too many incorrect attempts, leading to a locked state).
 *  Displays appropriate messages on the LCD for PIN prompts, success, or failure.
 */
bool handlePinEntry()
{
    char enteredPin[PIN_DIGITS + 1];
    int attempts = 0;
    // const char *actualPin = currentLoginPin.c_str(); // Used directly in strcmp
    // Define positions for PIN display (can be shared with getPinFromUser)
    constexpr int content_padding = 2;
    constexpr int text_x = BORDER_WIDTH + content_padding + 25;
    constexpr int pin_font_height = 13; // DOGL_Consolas7x13
    constexpr int pin_line_height = pin_font_height + 3;

    //constexpr int header_y = BORDER_WIDTH + content_padding;
    //constexpr int pin_display_y = header_y + pin_line_height;

    while (attempts < MAX_PIN_ATTEMPTS)
    {
        String pinAttemptStr;
        // Font for PIN entry is handled by getPinFromUser
        if (!getPinFromUser("Enter PIN:", pinAttemptStr))
        {
            // TODO
            //  This case implies getPinFromUser could be aborted,
            //  for now, assume it always completes and returns true.
            //  If an abort mechanism is added to getPinFromUser, handle it here.
        }
        strncpy(enteredPin, pinAttemptStr.c_str(), PIN_DIGITS);
        enteredPin[PIN_DIGITS] = '\0'; // Ensure null termination

        constexpr int msg_y = BORDER_WIDTH + content_padding + pin_line_height; // Y for status messages

        // All digits entered, compare PIN
        if (strcmp(enteredPin, currentLoginPin.c_str()) == 0)
        {
            LCD.cls();
            blinkLed(LED_STATE_GREEN, 200); // Green blink for correct PIN
            drawScreenBorder();
            // Use a central position for "PIN OK!"
            LCD.locate(text_x, msg_y);
            LCD.print(F("PIN OK!"));
            LCD.copy_to_lcd();
            delay(500);
            // Reset encoder to 0
            newPos.store(0);
            resetPos.store(true); // Signal core1 to reset its offset based on newPos
            resetInactivityTimer(); // Successful PIN entry is activity

            LCD.set_font(const_cast<unsigned char*>(Small_7)); // Reset to default font

            return true; // PIN correct
        }
        else
        {
            attempts++;
            LCD.cls();
            blinkLed(LED_STATE_RED, 200); // Red blink for incorrect PIN
            drawScreenBorder();
            LCD.locate(text_x - 20, msg_y);
            LCD.print(F("Invalid PIN!"));
            if (attempts < MAX_PIN_ATTEMPTS)
            {
                LCD.locate(text_x - 20, msg_y + pin_line_height);
                LCD.print(F("Attempts left: "));
                LCD.print(MAX_PIN_ATTEMPTS - attempts);
            }
            LCD.copy_to_lcd();
            delay(2000);
            if (attempts >= MAX_PIN_ATTEMPTS)
            {
                LCD.cls();
                drawScreenBorder();
                LCD.locate(text_x - 20, msg_y);
                LCD.print(F("Too many attempts!"));
                LCD.locate(text_x - 20, msg_y + pin_line_height);
                LCD.print(F("Device locked."));
                LCD.copy_to_lcd();

                LCD.set_font(const_cast<unsigned char*>(Small_7));

                return false; // Max attempts reached
            }
        }
    }
    LCD.set_font(const_cast<unsigned char*>(Small_7)); // Ensure font is reset on failure too
    // No resetInactivityTimer here on failure, as it's not a "successful" interaction continuation
    return false;
}

/*! \brief Sets the RGB LED to a specific color.
 *  \param r The state for the Red LED (HIGH or LOW).
 *  \param g The state for the Green LED (HIGH or LOW).
 *  \param b The state for the Blue LED (HIGH or LOW).
 */
void setLedColor(int r, int g, int b)
{
    analogWrite(LED_R_PIN, r);
    analogWrite(LED_G_PIN, g);
    analogWrite(LED_B_PIN, b);
}

/*! \brief Blinks the RGB LED with a specific color for a duration.
 *  Manages the persistent blue LED state.
 *  \param r_blink The Red component of the blink color.
 *  \param g_blink The Green component of the blink color.
 *  \param b_blink The Blue component of the blink color.
 *  \param duration The duration of the blink in milliseconds.
 */
void blinkLed(const int r_blink, const int g_blink, const int b_blink, const unsigned long duration)
{
    setLedColor(LED_STATE_OFF); // Turn all off, including persistent blue
    delayMicroseconds(100); // Short delay to ensure LEDs are off (increased slightly for PWM)

    setLedColor(r_blink, g_blink, b_blink); // Set the blink color
    delay(duration);
    // After blink, restore the correct persistent LED state
    setLedColor(LED_STATE_BLUE);
}

// /*! \brief Sets the persistent LED state based on current screen and sync status.
//  *  - Blue if on HomeScreen.
//  *  - Blue if not on HomeScreen and USB/RTC sync was successful.
//  *  - Off otherwise (not on HomeScreen and sync failed).
//  */
// void updatePersistentLedState()
// {
//     if (blueLedPersistentState.load())
//     {
//         // If USB/RTC sync was successful
//         setLedColor(LED_STATE_BLUE);
//     }
//     else
//     {
//         // USB/RTC sync failed
//         setLedColor(LED_STATE_RED);
//     }
// }

/*! \brief Updates the time display in the home screen header.
 *  Called periodically to refresh the time every second.
 *  Displays "Select..." and the current time (HH:MM:SS) aligned to the right.
 */
void updateHomeScreenHeaderTime()
{
    if (currentScreen != SCREEN_HOME)
    {
        return; // Only update if on the home screen
    }

    // Use the same positioning constants as drawHomeScreen
    constexpr int content_padding = 2;
    constexpr int header_x = BORDER_WIDTH + content_padding;
    constexpr int header_y = BORDER_WIDTH + content_padding;

    datetime_t t;
    char time_buf[10]; // "HH:MM:SS\0" -> 9 chars
    if (rtc_get_datetime(&t))
    {
        sprintf(time_buf, "%02d:%02d:%02d", t.hour, t.min, t.sec);
    }
    else
    {
        strcpy(time_buf, "--:--:--");
    }

    // Print "Select..." on the left
    LCD.locate(header_x, header_y);
    LCD.print(F("Select..."));

    // Calculate position for time on the right
    // Use actual pixel width for precise alignment
    LCD.setMonospace(true);
    //int time_str_width = calculate_string_pixel_width(time_buf, LCD);
    const int time_str_width = static_cast<int>(LCD.getStringPxLen(time_buf));
    const int _header_x_right_time = LCD.width() - BORDER_WIDTH - content_padding - time_str_width;

    LCD.locate(_header_x_right_time, header_y);
    LCD.print(time_buf);
    LCD.copy_to_lcd();
    LCD.setMonospace(false);
}

/*! \brief Truncates a string to fit within a maximum pixel width, appending a suffix if truncated.
 *  \param text The original string.
 *  \param maxPixelWidth The maximum allowed pixel width for the text (including suffix if appended).
 *  \param lcd The DOGL128 LCD instance.
 *  \param suffix The suffix to append if truncation occurs (defaults to "...").
 *  \return The original string if it fits, or the truncated string with the suffix.
 */
String truncateStringToFit(const String& text, const int maxPixelWidth, const DOGL128& lcd, const String& suffix)
{
    // Get current font characteristics from the LCD instance
    const unsigned char* current_font = lcd.font;
    if (!current_font)
    {
        return text; // No font, cannot calculate width
    }

    // Check if the original string fits
    //if (calculate_string_pixel_width(text.c_str(), lcd) <= maxPixelWidth)
    if (static_cast<int>(LCD.getStringPxLen(text.c_str())) <= maxPixelWidth)
    {
        return text;
    }

    //int suffixWidth = calculate_string_pixel_width(suffix.c_str(), lcd);
    const int suffixWidth = static_cast<int>(LCD.getStringPxLen(suffix.c_str()));

    // If the suffix alone is wider than maxPixelWidth, or maxPixelWidth is too small
    // for any character plus suffix, try to display as much of the original string as possible without suffix.
    if (suffixWidth >= maxPixelWidth || maxPixelWidth < (current_font[1] + suffixWidth))
    {
        // current_font[1] is char width for mono, a guess for min char width
        String tempString = "";
        int currentTempWidth = 0;
        for (int i = 0; i < text.length(); ++i)
        {
            const char singleCharStr[2] = {text.charAt(i), '\0'};
            //int char_width = calculate_string_pixel_width(singleCharStr, lcd);
            int char_width = static_cast<int>(LCD.getStringPxLen(singleCharStr));
            if (currentTempWidth + char_width <= maxPixelWidth)
            {
                tempString += text.charAt(i);
                currentTempWidth += char_width;
            }
            else
            {
                break;
            }
        }
        return tempString;
    }

    String resultString = "";
    int currentResultWidth = 0;
    for (int i = 0; i < text.length(); ++i)
    {
        const char singleCharStr[2] = {text.charAt(i), '\0'};
        if (const int char_width = static_cast<int>(LCD.getStringPxLen(singleCharStr)); currentResultWidth + char_width
            + suffixWidth <= maxPixelWidth)
        {
            resultString += text.charAt(i);
            currentResultWidth += char_width;
        }
        else
        {
            break;
        }
    }

    if (resultString.length() == 0 && suffixWidth <= maxPixelWidth)
    {
        return suffix;
    }

    return resultString + suffix;
}

/*! \brief Prompts the user to enter a 4-digit PIN using the rotary encoder and LCD.
 *  \param promptMessage The message to display above the PIN entry field.
 *  \param outPin A reference to a String where the entered PIN will be stored.
 *  \return True if PIN entry was completed, false otherwise (currently always true).
 *  Manages UI for digit selection, cursor movement, and input confirmation.
 */
bool getPinFromUser(const char* promptMessage, String& outPin)
{
    outPin = ""; // Clear output string
    resetInactivityTimer(); // Entering PIN entry mode is an activity
    char currentEnteredPin[PIN_DIGITS + 1];
    int currentDigitIndex = 0;

    constexpr int content_padding = 2;
    constexpr int text_x = BORDER_WIDTH + content_padding + 25; // Centered for "____"
    constexpr int pin_font_height = 13; // From DOGL_Consolas7x13
    constexpr int pin_line_height = pin_font_height + 3;

    constexpr int header_y = BORDER_WIDTH + content_padding;
    constexpr int pin_display_y = header_y + pin_line_height;
    constexpr int cursor_y = pin_display_y + pin_line_height;

    unsigned char* originalFont = LCD.font;
    LCD.set_font(const_cast<unsigned char*>(DOGL_Consolas7x13));

    LCD.cls();
    drawScreenBorder();
    LCD.locate(text_x, header_y); // Adjust x if prompt is long
    // For longer prompts, might need to calculate width and center or use smaller font for prompt
    //int prompt_width = calculate_string_pixel_width(promptMessage, LCD);
    const int prompt_width = static_cast<int>(LCD.getStringPxLen(promptMessage));
    int prompt_x = (LCD.width() - prompt_width) / 2;
    if (prompt_x < BORDER_WIDTH + content_padding)
        prompt_x = BORDER_WIDTH + content_padding;
    LCD.locate(prompt_x, header_y);
    LCD.print(promptMessage);

    for (int i = 0; i < PIN_DIGITS; ++i)
        currentEnteredPin[i] = '_';
    currentEnteredPin[PIN_DIGITS] = '\0';
    LCD.locate(text_x, pin_display_y);
    LCD.print(currentEnteredPin);

    int cursor_x_offset = LCD.calc_next_xPos(currentEnteredPin, currentDigitIndex, LCD);
    LCD.locate(text_x + cursor_x_offset, cursor_y);
    LCD.print("^");
    LCD.copy_to_lcd();

    while (currentDigitIndex < PIN_DIGITS)
    {
        int currentDigitValue = 0;
        int lastDisplayedDigit = -1;

        newPos.store(0); // Reset encoder for this digit
        resetPos.store(true);
        enterButtonPressed = false;

        while (!enterButtonPressed)
        {
            const int encoderVal = newPos.load();

            // Check for first encoder interaction if backlight is off due to timeout
            if (backlightOffDueToTimeout)
            {
                // newPos is reset to 0 for each digit. Any change from 0 means interaction.
                if (encoderVal != 0)
                {
                    digitalWrite(TFT_BL, HIGH); // Turn backlight on
                    backlightOffDueToTimeout = false; // Reset flag, backlight is now on for this PIN entry session
                }
            }

            currentDigitValue = (encoderVal % 10 + 10) % 10; // Ensure 0-9

            if (currentDigitValue != lastDisplayedDigit || detailViewNeedsRedraw.load())
            {
                currentEnteredPin[currentDigitIndex] = static_cast<char>('0' + currentDigitValue);
                LCD.locate(text_x, pin_display_y);
                LCD.print(currentEnteredPin);

                LCD.fillrect(BORDER_WIDTH + 1, cursor_y, LCD.width() - 1 - BORDER_WIDTH - 1,
                             cursor_y + pin_font_height - 1,
                             0); // Clear cursor line
                cursor_x_offset = LCD.calc_next_xPos(currentEnteredPin, currentDigitIndex, LCD);
                LCD.locate(text_x + cursor_x_offset, cursor_y);
                LCD.print("^");
                LCD.copy_to_lcd();
                lastDisplayedDigit = currentDigitValue;
                resetInactivityTimer(); // Encoder activity selecting a digit
                detailViewNeedsRedraw.store(false);
            }

            if (blipTone.load())
            {
                sendBlipTone(2000, 30, 15);
                blipTone.store(false);
                detailViewNeedsRedraw.store(true);
                // resetInactivityTimer(); // Will be reset by the redraw logic if digit changes
            }
            delay(10);
        }
        // Enter pressed for this digit
        sendBlipTone(2500, 50, 30); // Confirmation tone
        outPin += static_cast<char>('0' + currentDigitValue);
        currentDigitIndex++;
        delay(300); // Debounce/pause after digit entry
        enterButtonPressed = false;

        if (currentDigitIndex < PIN_DIGITS)
        {
            // Update cursor for next digit
            LCD.fillrect(BORDER_WIDTH + 1, cursor_y, LCD.width() - 1 - BORDER_WIDTH - 1,
                         cursor_y + pin_font_height - 1,
                         0);
            cursor_x_offset = LCD.calc_next_xPos(currentEnteredPin, currentDigitIndex, LCD);
            LCD.locate(text_x + cursor_x_offset, cursor_y);
            LCD.print("^");
            LCD.copy_to_lcd();
        }
    }
    LCD.set_font(originalFont); // Restore original font
    return true; // PIN entry completed
}

/*! \brief Asks the user if they wish to update their current PIN.
 *  Displays a prompt on the LCD and waits for the user to press
 *  the Enter button (Yes) or Print button (No).
 *  \return True if the user chooses to update the PIN, false otherwise.
 */
bool askToUpdatePin()
{
    unsigned char* originalFont = LCD.font; // Save current font
    resetInactivityTimer(); // Start of a user interaction sequence
    LCD.set_font(const_cast<unsigned char*>(Small_7)); // Use default small font for this prompt

    LCD.cls();
    drawScreenBorder();

    const auto promptLine1 = "Update PIN?";
    const auto promptLine2 = "(ENT=Yes / PRN=No)";
    const int L1_width = static_cast<int>(LCD.getStringPxLen(promptLine1));
    const int L2_width = static_cast<int>(LCD.getStringPxLen(promptLine2));
    int L1_x = (LCD.width() - L1_width) / 2;
    int L2_x = (LCD.width() - L2_width) / 2;
    if (L1_x < BORDER_WIDTH + 2)
        L1_x = BORDER_WIDTH + 2;
    if (L2_x < BORDER_WIDTH + 2)
        L2_x = BORDER_WIDTH + 2;

    constexpr int y_line1 = BORDER_WIDTH + 15;
    const int y_line2 = y_line1 + LCD.font[2] + 5; // font[2] is vertical size

    LCD.locate(L1_x, y_line1);
    LCD.print(promptLine1);
    LCD.locate(L2_x, y_line2);
    LCD.print(promptLine2);
    LCD.copy_to_lcd();

    enterButtonPressed = false;
    printButtonPressed = false;

    bool decisionMade = false;
    bool updatePin = false;

    while (!decisionMade)
    {
        if (enterButtonPressed)
        {
            enterButtonPressed = false;
            sendBlipTone(2500, 50, 30);
            // resetInactivityTimer(); // ISR already handled this
            updatePin = true;
            decisionMade = true;
        }
        if (printButtonPressed)
        {
            printButtonPressed = false;
            sendBlipTone(1500, 50, 30);
            // resetInactivityTimer(); // ISR already handled this
            updatePin = false;
            decisionMade = true;
        }
        delay(10); // Check flags interval
    }
    LCD.set_font(originalFont); // Restore original font
    return updatePin;
}

/*! \brief Manages the process of updating the user's PIN.
 *  Prompts the user to enter the new PIN twice for confirmation.
 *  If the PINs match and are valid, it saves the new PIN to non-volatile memory.
 *  Displays appropriate messages on the LCD for success, mismatch, or save failure.
 *  \return True if the PIN was successfully updated and saved, false otherwise.
 */
bool performPinUpdateProcess()
{
    unsigned char* originalFont = LCD.font; // Save current font
    // resetInactivityTimer(); // getPinFromUser will handle this
    // getPinFromUser will set its own font (DOGL_Consolas7x13) and restore

    String newPin1, newPin2;

    if (!getPinFromUser("Enter New PIN:", newPin1))
    {
        /* Handle potential abort */
    }
    if (!getPinFromUser("Confirm New PIN:", newPin2))
    {
        /* Handle potential abort */
    }

    LCD.set_font(const_cast<unsigned char*>(Small_7)); // Use small font for messages
    LCD.cls();
    drawScreenBorder();

    constexpr int msg_y = BORDER_WIDTH + 20;
    const int msg_y2 = msg_y + LCD.font[2] + 3;

    if (newPin1.equals(newPin2) && newPin1.length() == PIN_DIGITS)
    {
        if (savePinToNVM(newPin1))
        {
            const String msg = "PIN Updated!";
            //LCD.locate((LCD.width() - calculate_string_pixel_width(msg.c_str(), LCD)) / 2, msg_y);
            LCD.locate((LCD.width() - static_cast<int>(LCD.getStringPxLen(msg.c_str()))) / 2, msg_y);
            LCD.print(msg);
            LCD.copy_to_lcd();
            delay(2000);
            resetInactivityTimer(); // Successful PIN update
            LCD.set_font(originalFont);
            return true;
        }
        else
        {
            String msg = "Save Failed!";
            //LCD.locate((LCD.width() - calculate_string_pixel_width(msg.c_str(), LCD)) / 2, msg_y);
            LCD.locate((LCD.width() - static_cast<int>(LCD.getStringPxLen(msg.c_str()))) / 2, msg_y);
            LCD.print(msg);
            LCD.copy_to_lcd();
            delay(2000);
            // No timer reset on save failure
            LCD.set_font(originalFont);
            return false;
        }
    }
    else
    {
        const String msg1 = "Mismatch or Invalid.";
        const String msg2 = "PIN not updated.";
        //LCD.locate((LCD.width() - calculate_string_pixel_width(msg1.c_str(), LCD)) / 2, msg_y);
        LCD.locate((LCD.width() - static_cast<int>(LCD.getStringPxLen(msg1.c_str()))) / 2, msg_y);
        LCD.print(msg1);
        //LCD.locate((LCD.width() - calculate_string_pixel_width(msg2.c_str(), LCD)) / 2, msg_y2);
        LCD.locate((LCD.width() - static_cast<int>(LCD.getStringPxLen(msg2.c_str()))) / 2, msg_y2);
        LCD.print(msg2);
        LCD.copy_to_lcd();
        delay(2000);
        // No timer reset on mismatch
        LCD.set_font(originalFont);
        return false;
    }
}

/*! \brief Manages the process of updating the user's PIN.
 *  Prompts the user to enter the new PIN twice for confirmation.
 *  If the PINs match and are valid, it saves the new PIN to non-volatile memory.
 *  Displays appropriate messages on the LCD for success, mismatch, or save failure.
 *  \return True if the PIN was successfully updated and saved, false otherwise.
 */
bool savePinToNVM(const String& pinToSave)
{
    if (pinToSave.length() != PIN_DIGITS)
    {
        return false; // Should not happen if getPinFromUser is correct
    }

    digitalWrite(TFT_CS, HIGH);

    if (SD.exists(PIN_FILENAME))
    {
        SD.remove(PIN_FILENAME);
    }

    File pinFile = SD.open(PIN_FILENAME, FILE_WRITE);
    if (!pinFile)
    {
        digitalWrite(SD_CS_PIN, HIGH);
        return false;
    }

    char encryptedPin[PIN_DIGITS];
    for (int i = 0; i < PIN_DIGITS; ++i)
    {
        const char originalChar = pinToSave.charAt(i);
        const char rotatedChar = rotateLeft(originalChar, ROTATION_COUNT);
        encryptedPin[i] = rotatedChar ^ ENCRYPTION_KEY;
    }

    const size_t bytesWritten = pinFile.write(reinterpret_cast<const uint8_t*>(encryptedPin), PIN_DIGITS);
    pinFile.close();
    digitalWrite(SD_CS_PIN, HIGH);

    if (bytesWritten == PIN_DIGITS)
    {
        currentLoginPin = pinToSave; // Update in-memory PIN
        return true;
    }
    return false;
}

/*! \brief Loads the user's PIN from non-volatile memory (SD card).
 *  Attempts to read and decrypt the PIN from the file defined by PIN_FILENAME.
 *  If the file is not found, invalid, or decryption fails, it sets the PIN
 *  to `DEFAULT_LOGIN_PIN`, saves this default PIN, and displays a message.
 *  Updates the global `currentLoginPin` variable.
 *  \return True if a PIN was successfully loaded or a default PIN was set and saved, false on critical save error of default.
 */
bool loadPinFromNVM()
{
    digitalWrite(TFT_CS, HIGH);
    File pinFile = SD.open(PIN_FILENAME, FILE_READ);
    bool success = false;

    if (pinFile)
    {
        if (pinFile.size() == PIN_DIGITS)
        {
            char encryptedPin[PIN_DIGITS];
            char decryptedPinChars[PIN_DIGITS + 1]; // +1 for null terminator
            pinFile.readBytes(encryptedPin, PIN_DIGITS);

            bool validDigits = true;
            for (int i = 0; i < PIN_DIGITS; ++i)
            {
                char charAfterXor = encryptedPin[i] ^ ENCRYPTION_KEY;
                decryptedPinChars[i] = rotateRight(charAfterXor, ROTATION_COUNT);
                if (decryptedPinChars[i] < '0' || decryptedPinChars[i] > '9')
                {
                    validDigits = false;
                    break;
                }
            }

            if (validDigits)
            {
                decryptedPinChars[PIN_DIGITS] = '\0';
                currentLoginPin = String(decryptedPinChars);
                success = true;
            }
        }
        pinFile.close();
    }
    digitalWrite(SD_CS_PIN, HIGH);

    if (success)
    {
        resetInactivityTimer(); // Successfully loaded existing PIN
        return true;
    }

    // If loading failed, file invalid, or not found, set to default and save
    currentLoginPin = DEFAULT_LOGIN_PIN;
    // Display message that default PIN is being used/created
    unsigned char* originalFont = LCD.font;
    LCD.set_font(const_cast<unsigned char*>(Small_7));
    LCD.cls();
    drawScreenBorder();
    const String msg1 = F("PIN file not found");
    const String msg2 = F("or invalid.");
    const String msg3 = F("Using default PIN");
    //LCD.locate((LCD.width() - calculate_string_pixel_width(msg1.c_str(), LCD)) / 2, BORDER_WIDTH + 5);
    LCD.locate((LCD.width() - static_cast<int>(LCD.getStringPxLen(msg1.c_str()))) / 2, BORDER_WIDTH + 5);
    LCD.print(msg1);
    //LCD.locate((LCD.width() - calculate_string_pixel_width(msg2.c_str(), LCD)) / 2, BORDER_WIDTH + 5 + 10);
    LCD.locate((LCD.width() - static_cast<int>(LCD.getStringPxLen(msg2.c_str()))) / 2, BORDER_WIDTH + 5 + 10);
    LCD.print(msg2);
    //LCD.locate((LCD.width() - calculate_string_pixel_width(msg3.c_str(), LCD)) / 2, BORDER_WIDTH + 5 + 20);
    LCD.locate((LCD.width() - static_cast<int>(LCD.getStringPxLen(msg3.c_str()))) / 2, BORDER_WIDTH + 5 + 20);
    LCD.print(msg3);
    LCD.copy_to_lcd();
    delay(3000);
    LCD.set_font(originalFont);

    const bool defaultSaved = savePinToNVM(currentLoginPin); // Attempt to save the default PIN
    if (defaultSaved)
        resetInactivityTimer(); // Default PIN set and saved
    return defaultSaved;
}

void handleSerialCommands()
{
    //helper function
    auto extract_first_csv_field = [](const String& csvTail) -> String
    {
        String token = "";
        bool inQuotes = false;
        const int len = static_cast<int>(csvTail.length());
        for (int i = 0; i < len; ++i)
        {
            char c = csvTail[i];
            if (inQuotes)
            {
                if (c == '"')
                {
                    // lookahead for escaped quote
                    if (i + 1 < len && csvTail[i + 1] == '"')
                    {
                        token += '"';
                        ++i; // skip escaped quote
                    }
                    else
                    {
                        inQuotes = false; // end quote
                    }
                }
                else
                {
                    token += c;
                }
            }
            else
            {
                if (c == '"')
                {
                    inQuotes = true;
                }
                else if (c == ',')
                {
                    break; // first field ended
                }
                else
                {
                    token += c;
                }
            }
        }
        token.trim();
        return token;
    };

    while (Serial.available() > 0)
    {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        const int firstComma = line.indexOf(',');
        String cmd = (firstComma == -1) ? line : line.substring(0, firstComma);
        cmd.trim();
        cmd.toUpperCase();

        // Expect format CMD,<PIN>,<csvTail>
        int secondComma = -1;
        if (firstComma != -1)
            secondComma = line.indexOf(',', firstComma + 1);

        String pinToken = "";
        String csvTail = "";

        if (firstComma != -1 && secondComma != -1)
        {
            pinToken = line.substring(firstComma + 1, secondComma);
            csvTail = line.substring(secondComma + 1);
        }
        else if (firstComma != -1 && secondComma == -1)
        {
            // No csvTail present
            pinToken = line.substring(firstComma + 1);
            csvTail = "";
        }

        pinToken.trim();
        csvTail.trim();

        // Verify PIN if present (simple check); if no PIN provided, reject
        if (pinToken.length() == 0)
        {
            Serial.flush();
            Serial.clearWriteError();
            delay(100);
            Serial.println("ERR_NO_PIN");
            continue;
        }

        if (pinToken != currentLoginPin)
        {
            Serial.flush();
            Serial.clearWriteError();
            delay(100);
            Serial.println("ERR_BAD_PIN");
            continue;
        }

        if (cmd == "REBOOT") //host utillity sent a reboot request
        {
            Serial.println("DataSafe is Rebooting...");
            watchdog_reboot(0, 0, 0);
        }
        else if (cmd == "STREAMOUT")
        {
            // Call existing function to emit entire passwords file.
            streamOut();

            // Notify completion
            Serial.println("DONE: STREAMOUT");
        }
        else if (cmd == "STREAMIN")
        {
            // Acknowledge command receipt
            Serial.println("ACK: STREAMIN");
            streamIn();
            // Notify completion
            Serial.println("DONE: STREAMIN");
        }
        else if (cmd == "AT")
        {
            Serial.flush();
            Serial.clearWriteError();
            delay(100);
            Serial.println("Comm Test OK");
            Serial.flush();
            delay(50);
        }
        else if (cmd == "ADD")
        {
            // Find free slot
            int freeIdx = -1;
            for (int i = 0; i < MaxSites; ++i)
            {
                if (siteData[i][SITE_NAME_INDEX].length() == 0)
                {
                    freeIdx = i;
                    break;
                }
            }
            if (freeIdx == -1)
            {
                Serial.flush();
                Serial.clearWriteError();
                delay(100);
                Serial.println("ERR_FULL");
                continue;
            }

            // Parse and store the CSV tail into siteData[freeIdx]
            parseCSVLine(csvTail, freeIdx);
            // Sort and save
            sortSiteData();
            if (saveSiteDataToEncryptedFile(SD_FILENAME))
            {
                // if (Serial.write("ADD_OK\n") <= 6)
                // {
                //     // Serial write failed
                //     setLedColor(LOW, HIGH, LOW);
                // }
                Serial.flush();
                Serial.clearWriteError();
                delay(100);
                Serial.println("ADD_OK");
                Serial.flush();
                delay(50);
            }
            else
            {
                Serial.flush();
                Serial.clearWriteError();
                delay(100);
                Serial.println("ERR_SAVE");
                Serial.flush();
                delay(50);
            }
        }
        else if (cmd == "REM")
        {
            if (csvTail.length() == 0)
            {
                Serial.flush();
                Serial.clearWriteError();
                delay(100);
                Serial.println("ERR_NO_CSV");
                Serial.flush();
                continue;
            }

            // Extract the name (first CSV field) robustly supporting quotes
            String targetName = extract_first_csv_field(csvTail);
            if (targetName.length() == 0)
            {
                Serial.flush();
                Serial.clearWriteError();
                delay(100);
                Serial.println("ERR_NO_NAME");
                Serial.flush();
                continue;
            }

            // Find matching entry (first match)
            int foundIdx = -1;
            for (int i = 0; i < MaxSites; ++i)
            {
                if (siteData[i][SITE_NAME_INDEX].equals(targetName))
                {
                    foundIdx = i;
                    break;
                }
            }

            if (foundIdx == -1)
            {
                Serial.flush();
                Serial.clearWriteError();
                delay(100);
                Serial.println("REM_NOT_FOUND");
                Serial.flush();
                continue;
            }

            // Shift all rows after foundIdx up by one to remove the blank gap
            for (int i = foundIdx; i < MaxSites - 1; ++i)
            {
                for (int k = 0; k < SITE_DATA_FIELDS_COUNT; ++k)
                {
                    siteData[i][k] = siteData[i + 1][k];
                }
            }

            // Clear the last row (now duplicated)
            int lastIdx = MaxSites - 1;
            for (int k = 0; k < SITE_DATA_FIELDS_COUNT; ++k)
            {
                if (k == USAGE_COUNT_COLUMN_INDEX)
                    siteData[lastIdx][k] = "0";
                else
                    siteData[lastIdx][k] = "";
            }

            // Re-sort and save
            sortSiteData();
            if (saveSiteDataToEncryptedFile(SD_FILENAME))
            {
                Serial.flush();
                Serial.clearWriteError();
                delay(100);
                Serial.println("REM_OK");
                Serial.flush();
            }
            else
            {
                Serial.flush();
                Serial.clearWriteError();
                delay(100);
                Serial.println("ERR_SAVE");
            }
        }
        else
        {
            Serial.flush();
            Serial.clearWriteError();
            delay(100);
            Serial.println("ERR_UNKNOWN_CMD");
            Serial.flush();
        }
    }
}

// Draw the time immediately (single draw)
void drawTimeScreen()
{
    datetime_t dt;
    rtc_get_datetime(&dt); // pico RTC call

    char buf[20];
    char buf_date[20];

    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", static_cast<unsigned>(dt.hour), static_cast<unsigned>(dt.min),
             static_cast<unsigned>(dt.sec));
    snprintf(buf_date, sizeof(buf_date), "%02u/%02u/%04u", static_cast<unsigned>(dt.day),
             static_cast<unsigned>(dt.month), static_cast<unsigned>(dt.year));

    // Select font and clear area for the clock
    unsigned char* originalFont = LCD.font; // Save current font
    LCD.set_font(const_cast<unsigned char*>(DOGL_Arial12x20)); // set font to DOGL_Arial12x20 (project font)
    LCD.setMonospace(true);
    // Clear screen area (keep border if used)
    LCD.cls(); // clear full screen (or clearRect for partial)
    drawScreenBorder(); // redraw border if you need it

    // Compute centered position and draw
    int x = centerXForText(buf);
    constexpr int y = (64 - 20) / 2; // vertical center; font height = 20
    LCD.locate(x, y); // set cursor (method name may vary)
    LCD.print(buf); // draw text (method name may vary)
    LCD.copy_to_lcd();
    s_last_drawn_time = String(buf);

    LCD.set_font(originalFont); // Reset to default font
    LCD.setMonospace(false);
    x = centerXForText(buf_date);
    //LCD.locate(x, y + 22); // position below time
    LCD.locate(x, LCD.height() - 5 - LCD.font[2]); // position below time
    LCD.print(buf_date);

    snprintf(buf, sizeof(buf), "%s%s%s", monthsOfYear[dt.month - 1], "-", daysOfWeek[dt.dotw]);
    x = centerXForText(buf);
    LCD.locate(x, 5);
    LCD.print(buf);
    LCD.copy_to_lcd();
}

// Helper: center text horizontally
static int centerXForText(const char* text)
{
    //int w = calculate_string_pixel_width(text, LCD);
    const unsigned int w = LCD.getStringPxLen(text);
    return (LCD.width() - static_cast<int>(w)) / 2;
}

void doClockDisplay()
{
    // Wait until enter button flag is set (user pressed encoder/enter)
    //resetInactivityTimer();
    while (!enterButtonPressed.load(std::memory_order_acquire))
    {
        if (millis() - lastActivityTime > INACTIVITY_TIMEOUT_MS)
        {
            //timed out due to inactivity
            // Turn off backlight
            digitalWrite(TFT_BL, LOW);
            backlightOffDueToTimeout = true;
        }
        drawTimeScreen();
        Serial.println(F("PICO_READY")); // Periodically send ready signal - for host to connect
        delay(1000); // Update every second
    }

    // Wait for the physical button to be released (active LOW) to avoid immediate retrigger
    while (digitalRead(ENTERBUTTON_PIN) == LOW)
    {
        sleep_ms(10);
    }

    // Consume the button event so home screen does not process it again
    enterButtonPressed.store(false, std::memory_order_release);

    digitalWrite(TFT_BL, HIGH);

    //do pin entry only if clock was shown due to inactivity timeout
    if (backlightOffDueToTimeout)
    {
        if (!handlePinEntry())
        {
            while (true)
            {
                setLedColor(LED_STATE_RED); // Indicate locked state
                delay(500);
            }
        }
    }
    backlightOffDueToTimeout = false;
    currentScreen = SCREEN_HOME;
    positionHold = 0;
    drawHomeScreen(positionHold);
    resetInactivityTimer();
}
