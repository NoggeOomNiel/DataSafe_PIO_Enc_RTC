#pragma once

#include <Arduino.h>
#include <atomic>
#include <string>
#include <Keyboard.h>
#include "Constants.h"
#include "quadrature_encoder.pio.h"

class InputManager
{
public:
  /** @brief Default constructor */
  InputManager();

  /** @brief Initializes encoder PIO, buttons, buzzer, and launches Core 1 task */
  void begin();

  /** @brief Main loop update (currently placeholder) */
  void update(); // Called in main loop

  // Encoder
  /** @brief Returns the current encoder position */
  int getEncoderPosition();

  /** @brief Sets the encoder position to a specific value */
  void setEncoderPosition(int pos);

  /** @brief Resets the encoder position to zero and triggers PIO reset */
  void resetEncoderPosition();

  /** @brief Checks if encoder has moved (app should compare position directly) */
  bool hasEncoderMoved();

  // Buttons
  /** @brief Checks if Enter button was pressed */
  bool isEnterPressed();

  /** @brief Checks if Print button was pressed */
  bool isPrintPressed();

  /** @brief Clears the Enter button pressed flag */
  void clearEnterButton();

  /** @brief Clears the Print button pressed flag */
  void clearPrintButton();

  // Feedback
  /** @brief Checks if a blip tone should be played */
  bool shouldBlip();

  /** @brief Clears the blip tone flag */
  void clearBlip();

  /** @brief Manually triggers a blip tone */
  void triggerBlip();

  /** @brief Plays a tone on the buzzer with specified parameters */
  void sendBlipTone(uint32_t freq, int ratio, uint32_t duration);

  // Inactivity
  /** @brief Resets the inactivity timer to current time */
  void resetInactivityTimer();

  /** @brief Checks if inactivity timeout has been exceeded */
  bool checkInactivityTimeout();

  /** @brief Returns the last activity timestamp */
  unsigned long getLastActivityTime();

  // Keyboard
  /** @brief Sends a string as USB keyboard keystrokes */
  void sendStringAsKeystrokes(const std::string &textToSend);

  /** @brief Starts USB keyboard emulation */
  void beginKeyboard();

  /** @brief Stops USB keyboard emulation */
  void endKeyboard();

  /** @brief Checks if USB keyboard emulation is currently active */
  bool isKeyboardRunning();

  // Core 1 Task (Encoder)
  /** @brief Core 1 task that continuously monitors encoder */
  static void core1Task(); // Static to be called by multicore_launch_core1

  // Access to atomics (if needed by friends or static methods)
  static std::atomic<int> newPos;
  static std::atomic<bool> resetPos;
  static std::atomic<bool> blipTone;

private:
  static void ButtonPressISR();

  static std::atomic<bool> enterButtonPressed;
  static std::atomic<bool> printButtonPressed;

  unsigned long lastActivityTime = 0;
  bool keyboardRunning = false;
};
