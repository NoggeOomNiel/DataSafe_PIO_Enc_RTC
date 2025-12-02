#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "Constants.h"
#include "UiManager.h"
#include "InputManager.h"
#include "SecurityManager.h"
#include "DataManager.h"
#include "ds3231.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "hardware/watchdog.h"

// Managers
UiManager ui;
InputManager input;
SecurityManager security;
DataManager data;

// Global State
ScreenState currentScreen = SCREEN_HOME;
int positionHold = 0;
int detailSelectedItemIndex = 0;
bool detailViewNeedsRedraw = false;
bool returnFromDetails = false;

// Helper functions
/** @brief Processes serial commands from host PC */
void handleSerialCommands();

/** @brief Synchronizes the Pico's RTC with the external DS3231 RTC module */
bool syncRTCWithDS3231();

/** @brief Synchronizes the RTC with time received from host PC via serial */
bool syncRTCWithHost();

/** @brief Converts Unix timestamp to Pico SDK datetime structure */
bool unix_to_datetime(time_t ts, datetime_t *_dt);

/** @brief Updates clock display */
void doClockDisplay();

/** @brief Displays clock screen and waits for user to press Enter button */
void showClockAndWait();

/** @brief Trims leading and trailing whitespace from a string */
static void trim(std::string &s);
