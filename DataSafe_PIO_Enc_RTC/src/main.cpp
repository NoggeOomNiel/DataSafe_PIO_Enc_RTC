/*
 This application:

    Author: Niel Malan
    Date: November 2025

    * Embedded password manager for a Raspberry Pi Pico with DOGL128 LCD and
 rotary encoder input.
    * Stores site records on an SD card as encrypted .enc files (XOR + rotate),
 can import/export CSV via Serial, and stream/type data to a host using USB
 Keyboard or Serial.
    * PIN-protected access with encrypted PIN saved on SD, supports PIN update
 and retry limits.
    * RTC synchronization (DS3231 or host), usage-based sorting, usage counters,
 buzzer and RGB LED feedback, and background encoder handling on core1.
    * Communicates with host PC via USB Serial for importing/exporting data and
 debugging.
    * Host PC application for easier data management is available separately.

    This code was written using:
    https://github.com/maxgerhardt/platform-raspberrypi.git platform and the
    Arduino framework on Platfomio for the Raspberry Pi Pico (board_build.core =
    earlephilhower)

    Any IDE may be used provided that the Platformio extension can be installed.
    Various AI tools to assist with code generation have been used.
    The Arduino String class was avoided to prevent memory issues.

    NB: This code is provided as-is, without any warranties or liabilities.

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
    I have modified it to add some functions.

    The PIO quadrature encoder program has the following copyright notice:
        Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
        SPDX-License-Identifier: BSD-3-Clause

*/

/*
Mods:
29-11-2025 - Restructured original code to use managers
30-11-2025 - Removed usage of Arduino String class, replaced with std::string
1-12-2025 - Implemented AES-256 encryption for data files

*/

#include "main.h"
#include <string>
#include <algorithm>
#include <cctype>

/**
 * @brief Arduino setup function - initializes all hardware and manager instances
 *
 * Performs the following initialization sequence:
 * - Initializes UI, input, and security managers
 * - Configures SPI1 for SD card communication
 * - Configures I2C for DS3231 RTC module
 * - Initializes RGB LED pins
 * - Mounts SD card (halts on failure)
 * - Handles PIN entry and optional PIN update
 * - Synchronizes RTC with DS3231 or host
 * - Loads and sorts password data from SD card
 * - Shows clock screen and waits for user to press Enter
 */
void setup()
{
    ui.begin();
    input.begin();
    security.begin();

    // Initialize SPI1 for SD Card
    SPI1.setRX(SD_SPI1_MISO_PIN);
    SPI1.setTX(SD_SPI1_MOSI_PIN);
    SPI1.setSCK(SD_SPI1_SCK_PIN);
    SPI1.begin();

    // Initialize I2C for RTC
    Wire1.setSCL(D15);
    Wire1.setSDA(D14);
    Wire1.begin();
    DS3231_init(DS3231_CONTROL_INTCN);

    // Initialize RGB LED pins
    pinMode(LED_R_PIN, OUTPUT);
    pinMode(LED_G_PIN, OUTPUT);
    pinMode(LED_B_PIN, OUTPUT);
    ui.setLedColor(LED_STATE_OFF);

    sleep_ms(200);

    // Initialize SD Card
    if (!SD.begin(SD_CS_PIN, SPI_DIV6_SPEED, SPI1))
    {
        ui.clear();
        ui.drawScreenBorder();
        ui.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 10);
        ui.print("SD Card Error!");
        ui.copyToLcd();
        while (true)
            delay(500);
    }

    // PIN Handling
    if (!security.hasPinFile())
    {
        // No PIN file -> Reset device
        ui.clear();
        ui.drawScreenBorder();
        ui.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 10);
        ui.print("PIN File Missing!");
        ui.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 20);
        ui.print("Resetting Data...");
        ui.copyToLcd();
        delay(2000);

        data.deleteAllDataFiles(SD_FILENAME);

        ui.clear();
        ui.drawScreenBorder();
        ui.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 10);
        ui.print("Set New PIN");
        ui.copyToLcd();
        delay(1000);

        while (!security.performPinUpdateProcess(ui, input))
        {
            // Loop until successful PIN creation
            ui.clear();
            ui.drawScreenBorder();
            ui.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 10);
            ui.print("PIN Setup Failed");
            ui.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 20);
            ui.print("Try Again...");
            ui.copyToLcd();
            delay(2000);
        }
    }
    else
    {
        // PIN file exists -> Normal boot
        if (!security.handlePinEntry(ui, input))
        {
            while (true)
            {
                ui.setLedColor(LED_STATE_RED);
                delay(500);
            }
        }

        // PIN Update Check
        if (security.askToUpdatePin(ui, input))
        {
            security.performPinUpdateProcess(ui, input);
        }
    }

    ui.clear();
    ui.drawScreenBorder();

    Serial.begin(115200);
    delay(500);

    // RTC Sync
    if (!syncRTCWithDS3231())
    {
        bool rtc_synced = syncRTCWithHost();
        ui.clear();
        ui.drawScreenBorder();
        ui.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 10);
        if (rtc_synced)
        {
            ui.print("RTC Sync OK");
            ui.setLedColor(LED_STATE_GREEN);
        }
        else
        {
            ui.print("RTC Sync Failed!");
            ui.setLedColor(LED_STATE_RED);
        }
        ui.copyToLcd();
        sleep_ms(2000);
    }

    // Load Data
    if (data.loadData(SD_FILENAME, &ui))
    {
        data.sortData();
    }

    ui.clear();
    ui.drawScreenBorder();
    // int centerX = ui.centerXForText("Data loaded.");
    // int centerY = ui.centerYForText("Data loaded.");
    // ui.locate(centerX, centerY);
    // ui.print("Data loaded.");
    // ui.copyToLcd();
    // sleep_ms(2000);

    // Show Clock (Wait for user to press Enter)
    showClockAndWait();

    input.resetInactivityTimer();
    currentScreen = SCREEN_HOME;
    ui.drawHomeScreen(positionHold, data.getSiteData());
}

/**
 * @brief Main application loop - handles screen states and user interactions
 *
 * Manages three screen states (HOME, DETAILS, CLOCK) and handles:
 * - Inactivity timeout detection and clock display
 * - Audio feedback (blip tones)
 * - Home screen: encoder navigation, site selection, header time updates
 * - Details screen: field selection and keyboard output
 * - Print button: password output or long-press reboot
 * - Serial command processing
 * - Usage tracking and dynamic sorting
 */
void loop()
{
    // Inactivity Check
    if (input.checkInactivityTimeout())
    {

        doClockDisplay();

        // After clock display (which blocks until input), we are back.
        // If PIN entry was required inside doClockDisplay, it handled it.
        // If it failed, it looped forever.

        currentScreen = SCREEN_HOME;
        positionHold = 0;
        input.resetEncoderPosition();
        ui.drawHomeScreen(positionHold, data.getSiteData());
        ui.setLedColor(LED_STATE_BLUE);
        input.resetInactivityTimer();
    }

    // Blip Tone
    if (input.shouldBlip())
    {
        if (currentScreen == SCREEN_DETAILS)
        {
            detailViewNeedsRedraw = true;
        }
        input.sendBlipTone(2000, 50, 20);
        input.clearBlip();
    }

    if (currentScreen == SCREEN_CLOCK)
    {
        doClockDisplay();
        currentScreen = SCREEN_HOME;
        positionHold = 0;
        input.resetEncoderPosition();
        ui.drawHomeScreen(positionHold, data.getSiteData());
        input.resetInactivityTimer();
    }
    else if (currentScreen == SCREEN_HOME)
    {
        handleSerialCommands();

        static unsigned long lastHeaderUpdateTime = 0;
        if (millis() - lastHeaderUpdateTime >= 1000)
        {
            datetime_t dt;
            rtc_get_datetime(&dt);
            ui.updateHomeScreenHeaderTime(dt);
            lastHeaderUpdateTime = millis();
        }

        int newPos = input.getEncoderPosition();
        if (positionHold != newPos)
        {
            if (newPos < 0)
            {
                input.resetEncoderPosition();
                newPos = 0;
            }
            positionHold = newPos;
            ui.drawHomeScreen(positionHold, data.getSiteData());
            input.resetInactivityTimer();
        }

        if (input.isEnterPressed())
        {
            input.clearEnterButton();
            ui.blinkLed(LED_STATE_GREEN, 200);
            delay(500);

            currentScreen = SCREEN_DETAILS;
            detailSelectedItemIndex = 0;
            input.resetEncoderPosition();
            ui.drawDetailsScreen(positionHold, detailSelectedItemIndex, data.getSiteData());
        }
    }
    else if (currentScreen == SCREEN_DETAILS)
    {
        returnFromDetails = false;

        int newSelection = input.getEncoderPosition();

        if (detailViewNeedsRedraw || newSelection != detailSelectedItemIndex)
        {
            input.beginKeyboard();

            if (newSelection < 0)
            {
                newSelection = 0;
                if (input.getEncoderPosition() != 0)
                    input.resetEncoderPosition();
            }
            else if (newSelection >= NUM_DETAIL_ITEMS_ON_SCREEN)
            {
                newSelection = NUM_DETAIL_ITEMS_ON_SCREEN - 1;
                if (input.getEncoderPosition() != (NUM_DETAIL_ITEMS_ON_SCREEN - 1))
                {
                    input.setEncoderPosition(NUM_DETAIL_ITEMS_ON_SCREEN - 1);
                }
            }

            detailSelectedItemIndex = newSelection;
            ui.drawDetailsScreen(positionHold, detailSelectedItemIndex, data.getSiteData());
            input.resetInactivityTimer();
            detailViewNeedsRedraw = false;
        }

        if (input.isEnterPressed())
        {
            input.clearEnterButton();
            ui.blinkLed(LED_STATE_GREEN, 200);
            delay(500);

            std::string(*siteData)[SITE_DATA_FIELDS_COUNT] = data.getSiteData();
            std::string selectedItemText = siteData[positionHold][detailSelectedItemIndex + 1];

            bool shouldReturnHome = false;
            bool itemWasUsedAction = false;

            if (detailSelectedItemIndex == 0)
            { // WebAddress
                input.sendStringAsKeystrokes(selectedItemText);
                Keyboard.write(KEY_RETURN);
                delay(50);
            }
            else if (detailSelectedItemIndex == 1 && selectedItemText == "Login...")
            {
                input.sendStringAsKeystrokes(siteData[positionHold][3]); // Username
                delay(500);
                Keyboard.write(KEY_TAB);
                delay(500);
                input.sendStringAsKeystrokes(siteData[positionHold][4]); // Password
                delay(500);
                Keyboard.write(KEY_RETURN);
                itemWasUsedAction = true;
            }
            else if (detailSelectedItemIndex == (NUM_DETAIL_ITEMS_ON_SCREEN - 1) && selectedItemText == "Return...")
            {
                shouldReturnHome = true;
            }
            else
            {
                input.sendStringAsKeystrokes(selectedItemText);
                if (detailSelectedItemIndex == 2 || detailSelectedItemIndex == 3)
                {
                    itemWasUsedAction = true;
                }
            }

            if (shouldReturnHome)
            {
                input.endKeyboard();
                if (!Serial.available())
                {
                    Serial.begin(115200);
                    delay(500);
                }
                currentScreen = SCREEN_HOME;
                input.resetEncoderPosition();
                positionHold = 0;
                ui.drawHomeScreen(positionHold, data.getSiteData());
                returnFromDetails = true;
            }
            else
            {
                if (itemWasUsedAction)
                {
                    std::string siteNameToKeepSelected = siteData[positionHold][SITE_NAME_INDEX];
                    data.incrementUsage(positionHold);
                    data.saveData(SD_FILENAME, &ui);
                    data.sortData();

                    int newIdxOfSite = -1;
                    for (int i = 0; i < MaxSites; ++i)
                    {
                        if (siteData[i][SITE_NAME_INDEX].length() == 0)
                            break;
                        if (siteData[i][SITE_NAME_INDEX] == siteNameToKeepSelected)
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
                        currentScreen = SCREEN_HOME;
                        input.resetEncoderPosition();
                        positionHold = 0;
                        ui.drawHomeScreen(positionHold, data.getSiteData());
                        return;
                    }
                }
                ui.drawDetailsScreen(positionHold, detailSelectedItemIndex, data.getSiteData());
            }
        }
    }

    if (input.isPrintPressed())
    {
        input.clearPrintButton();
        ui.blinkLed(LED_STATE_YELLOW, 200);

        unsigned long pressStart = millis();
        while (digitalRead(PRINTBUTTON_PIN) == LOW)
        {
            if (millis() - pressStart >= 5000)
            {
                ui.clear();
                ui.drawScreenBorder();
                ui.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 20);
                ui.print("Rebooting...");
                ui.copyToLcd();
                watchdog_reboot(0, 0, 1000);
            }
            sleep_ms(50);
        }
    }
}

// Helper Functions

/**
 * @brief Processes serial commands from host PC
 *
 * Handles the following PIN-protected commands:
 * - AT: Communication test (no PIN required)
 * - RESYNC: Synchronize RTC with host time
 * - REBOOT: Reboot the device via watchdog
 * - STREAMOUT: Export all password data to host
 * - STREAMIN: Import password data from host
 * - ADD: Add a new password entry
 * - REM: Remove a password entry by name
 *
 * All commands except AT require a valid PIN as the second CSV field.
 */
void handleSerialCommands()
{
    while (Serial.available() > 0)
    {
        std::string line = Serial.readStringUntil('\n').c_str();
        trim(line);
        if (line.length() == 0)
            continue;

        int firstComma = line.find(',');
        std::string cmd = (firstComma == -1) ? line : line.substr(0, firstComma);
        trim(cmd);
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

        int secondComma = -1;
        if (firstComma != -1)
            secondComma = line.find(',', firstComma + 1);

        std::string pinToken = "";
        std::string csvTail = "";

        if (firstComma != -1 && secondComma != -1)
        {
            pinToken = line.substr(firstComma + 1, secondComma - (firstComma + 1));
            csvTail = line.substr(secondComma + 1);
        }
        else if (firstComma != -1 && secondComma == -1)
        {
            pinToken = line.substr(firstComma + 1);
        }

        trim(pinToken);
        trim(csvTail);

        if (cmd == "AT")
        {
            Serial.println("Comm Test OK");
            continue;
        }

        if (pinToken.length() == 0)
        {
            Serial.println("ERR_NO_PIN");
            continue;
        }

        if (pinToken != security.getCurrentPin())
        {
            Serial.println("ERR_BAD_PIN");
            continue;
        }

        if (cmd == "RESYNC")
        {
            syncRTCWithHost();
            Serial.println("RTC Resynced");
        }
        else if (cmd == "REBOOT")
        {
            Serial.println("DataSafe is Rebooting...");
            watchdog_reboot(0, 0, 200);
        }
        else if (cmd == "STREAMOUT")
        {
            std::atomic<bool> cancel(false);
            data.streamOut(SD_FILENAME, cancel, &ui);
            // restore home screen
            ui.drawHomeScreen(positionHold, data.getSiteData());
            Serial.println("DONE: STREAMOUT");
        }
        else if (cmd == "STREAMIN")
        {
            Serial.println("ACK: STREAMIN");
            data.streamIn(SD_FILENAME, &ui);
            Serial.println("DONE: STREAMIN");
            // restore home screen
            ui.drawHomeScreen(positionHold, data.getSiteData());
        }
    }
}

/**
 * @brief Displays clock screen and waits for user to press Enter button
 *
 * Continuously updates the time display every second and sends PICO_READY
 * status to the host via serial along with the current Unix timestamp.
 * Handles inactivity timeout by dimming the backlight.
 * Returns when Enter button is pressed.
 */
void showClockAndWait()
{
    input.resetInactivityTimer();
    unsigned long lastUpdate = 0;
    while (!input.isEnterPressed())
    {
        if (input.checkInactivityTimeout())
        {
            digitalWrite(TFT_BL, LOW);
        }

        handleSerialCommands();

        if (millis() - lastUpdate >= 1000)
        {
            datetime_t dt;
            rtc_get_datetime(&dt);
            ui.drawTimeScreen(dt);

            Serial.println(F("PICO_READY"));
            // Also send unix timestamp of current RTC time - if time does not match the host time the host
            //  may decide to send updated time (hanled in handleSerialCommands function)
            struct tm tmTime{};
            tmTime.tm_year = dt.year - 1900;
            tmTime.tm_mon = dt.month - 1;
            tmTime.tm_mday = static_cast<unsigned char>(dt.day);
            tmTime.tm_hour = static_cast<unsigned char>(dt.hour);
            tmTime.tm_min = static_cast<unsigned char>(dt.min);
            tmTime.tm_sec = static_cast<unsigned char>(dt.sec);
            tmTime.tm_isdst = 0;                            // no daylight savings adjustment
            const time_t currentUnixTime = mktime(&tmTime); // use timegm(&tmTime) for UTC if available
            Serial.println(currentUnixTime);

            lastUpdate = millis();
        }

        delay(10);
    }

    while (digitalRead(ENTERBUTTON_PIN) == LOW)
        sleep_ms(10);
    input.clearEnterButton();
    digitalWrite(TFT_BL, HIGH);
}

/**
 * @brief Displays clock screen and re-authenticates user with PIN
 *
 * Shows the clock screen via showClockAndWait(), then prompts for PIN entry.
 * If PIN entry fails, enters infinite error state with red LED.
 * Used after inactivity timeout to lock the device.
 */
void doClockDisplay()
{
    showClockAndWait();

    if (!security.handlePinEntry(ui, input))
    {
        while (true)
        {
            ui.setLedColor(LED_STATE_RED);
            delay(500);
        }
    }
}

/**
 * @brief Synchronizes the Pico's RTC with the external DS3231 RTC module
 *
 * Checks if the Pico's RTC has a valid year (2025-2125). If invalid,
 * reads time from the DS3231 module and updates the Pico's RTC.
 *
 * @return true if RTC was already valid or successfully synchronized
 * @return false if DS3231 also has invalid time
 */
bool syncRTCWithDS3231()
{
    datetime_t dt;
    rtc_get_datetime(&dt);
    if (dt.year < 2025 || dt.year > 2125)
    {
        ts t{};
        DS3231_get(&t);
        if (t.year < 2025 || t.year > 2125)
        {
            ui.setLedColor(LED_STATE_RED);
            return false;
        }
        dt.day = t.mday;
        dt.month = t.mon;
        dt.dotw = t.wday % 7;
        dt.hour = t.hour;
        dt.min = t.min;
        dt.sec = t.sec;
        dt.year = t.year;
        rtc_init();
        rtc_set_datetime(&dt);
        ui.setLedColor(LED_STATE_BLUE);
        return true;
    }
    return true;
}

/**
 * @brief Synchronizes the RTC with time received from host PC via serial
 *
 * @return true if synchronization was successful
 * @return false if synchronization failed (currently returns false as placeholder)
 */
bool syncRTCWithHost()
{
    constexpr unsigned long timeoutMillis = 20000; // 20 seconds timeout for sync
    unsigned long startTime = millis();
    bool timeReceived = false;
    std::string timeStr = "";

    ui.clear();
    ui.drawScreenBorder();
    constexpr int content_padding = 2;
    constexpr int text_x = BORDER_WIDTH + content_padding;
    constexpr int line_height = 7 + 3; // Small font
    constexpr int y_line0 = BORDER_WIDTH + content_padding;
    constexpr int y_line1 = y_line0 + line_height;
    constexpr int y_line2 = y_line1 + line_height;
    constexpr int y_line3 = y_line2 + line_height;
    constexpr int y_line4 = y_line3 + line_height;

    ui.locate(text_x, y_line0);
    ui.print("RTC Sync with Host:");
    ui.locate(text_x, y_line1);
    ui.print("Waiting for Host...");
    ui.copyToLcd();

    const unsigned long waitHostStartTime = millis();
    bool hostAckReceived = false;
    constexpr unsigned long hostConnectTimeout = 25000;

    while (millis() - waitHostStartTime < hostConnectTimeout)
    {
        if (!Serial)
        {
            delay(100);
            continue;
        }
        Serial.println(F("PICO_READY"));

        ui.copyToLcd();

        const unsigned long ackWaitStartTime = millis();
        while (millis() - ackWaitStartTime < 1000)
        {
            if (Serial.available() > 0)
            {
                std::string ack = Serial.readStringUntil('\n').c_str();
                trim(ack);
                if (ack == "HOST_ACK")
                {
                    hostAckReceived = true;
                    ui.locate(text_x, y_line3);
                    ui.print("Host ACK Received! ");
                    ui.copyToLcd();
                    delay(500);
                    break;
                }
            }
            delay(10);
        }
        if (hostAckReceived)
            break;
        delay(1000);
    }

    if (!hostAckReceived)
    {
        ui.locate(text_x, y_line3);
        ui.print("Host Connect Timeout");
        ui.copyToLcd();
        return false;
    }

    // Host is connected, request time
    Serial.println(F("SYNC_TIME_REQUEST"));
    ui.locate(text_x, y_line2);
    ui.print("Req Time, Wait Resp ");
    ui.copyToLcd();

    startTime = millis();
    while (millis() - startTime < timeoutMillis)
    {
        if (Serial.available() > 0)
        {
            timeStr = Serial.readStringUntil('\n').c_str();
            trim(timeStr);
            if (timeStr.length() > 0)
            {
                timeReceived = true;
                break;
            }
        }
        delay(10);
    }

    if (timeReceived)
    {
        ui.locate(text_x, y_line3);
        ui.print("Received: ");
        std::string truncatedTime = timeStr.substr(0, 16 - strlen("Received: "));
        ui.print(truncatedTime.c_str());
        ui.copyToLcd();
        delay(1000);

        char *endptr;
        long unix_ts = strtol(timeStr.c_str(), &endptr, 10);

        if (endptr != timeStr.c_str() && unix_ts > 0)
        {
            unix_ts += 7200L; // adjust for GMT+2 (2 * 60 * 60 = 7200 seconds.)
            datetime_t dt;
            if (unix_to_datetime(static_cast<time_t>(unix_ts), &dt))
            {
                rtc_init(); // Initialize RTC
                if (rtc_set_datetime(&dt))
                {
                    // also set DS3231
                    ts t{};
                    t.hour = dt.hour;
                    t.min = dt.min;
                    t.sec = dt.sec;
                    t.mday = dt.day;
                    t.mon = dt.month;
                    t.year = dt.year;
                    t.wday = dt.dotw;
                    DS3231_set(t);

                    ui.locate(text_x, y_line4);
                    ui.print("RTC/DS3231 Set OK! ");
                    return true;
                }
                else
                {
                    ui.locate(text_x, y_line4);
                    ui.print("RTC Set Fail!      ");
                    ui.copyToLcd();
                }
            }
            else
            {
                ui.locate(text_x, y_line4);
                ui.print("Time Conv Fail!    ");
                ui.copyToLcd();
            }
        }
        else
        {
            ui.locate(text_x, y_line4);
            ui.print("Invalid TimeVal!   ");
            ui.copyToLcd();
        }
    }
    else
    {
        ui.locate(text_x, y_line3);
        ui.print("Sync Timeout!      ");
        ui.copyToLcd();
    }
    return false;
}

/**
 * @brief Converts Unix timestamp to Pico SDK datetime structure
 *
 * @param ts Unix timestamp (seconds since epoch)
 * @param _dt Pointer to datetime_t structure to populate
 * @return true if conversion was successful
 * @return false if gmtime_r failed
 */
bool unix_to_datetime(time_t ts, datetime_t *_dt)
{
    struct tm ti{};
    if (gmtime_r(&ts, &ti) == nullptr)
        return false;
    _dt->year = ti.tm_year + 1900;
    _dt->month = ti.tm_mon + 1;
    _dt->day = ti.tm_mday;
    _dt->dotw = ti.tm_wday;
    _dt->hour = ti.tm_hour;
    _dt->min = ti.tm_min;
    _dt->sec = ti.tm_sec;
    return true;
}

/**
 * @brief Trims leading and trailing whitespace from a string
 *
 * @param s Reference to string to trim
 */
static void trim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                    { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                         { return !std::isspace(ch); })
                .base(),
            s.end());
}