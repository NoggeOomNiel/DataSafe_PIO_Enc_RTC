#pragma once

#include <Arduino.h>
#include <SD.h>
#include <string>
#include "Constants.h"
#include "UiManager.h"
#include "InputManager.h"

class SecurityManager
{
public:
  /** @brief Default constructor */
  SecurityManager();

  /** @brief Initializes the SecurityManager by loading saved PIN from SD card */
  void begin();

  // PIN Management
  /** @brief Prompts user to enter PIN and validates it with retry limit */
  bool handlePinEntry(UiManager &ui, InputManager &input);

  /** @brief Prompts user whether they want to update their PIN */
  bool askToUpdatePin(UiManager &ui, InputManager &input);

  /** @brief Handles the complete PIN update workflow */
  bool performPinUpdateProcess(UiManager &ui, InputManager &input);

  /** @brief Returns the current login PIN */
  std::string getCurrentPin() const;

  /** @brief Checks if the PIN file exists on the SD card */
  bool hasPinFile();

  // Encryption Helpers
  /** @brief Encrypts a file using AES-256-CTR */
  bool encryptFile(const char *sourceFilename, const char *destEncryptedFilename, UiManager *ui = nullptr);

  /** @brief Decrypts a file using AES-256-CTR */
  bool decryptFile(const char *sourceEncryptedFilename, const char *destPlainFilename, UiManager *ui = nullptr);

private:
  /** @brief Loads and decrypts PIN from SD card storage */
  bool loadPinFromNVM();

  /** @brief Saves encrypted PIN to SD card storage */
  bool savePinToNVM(const std::string &pinToSave);

  /** @brief Displays PIN entry UI and collects PIN from user via rotary encoder */
  bool getPinFromUser(const char *promptMessage, std::string &outPin, UiManager &ui, InputManager &input);

  /** @brief Derives a 256-bit key from the PIN using PBKDF2 */
  void deriveKey(const std::string &pin, const uint8_t *salt, uint8_t *outputKey);

  std::string currentLoginPin = DEFAULT_LOGIN_PIN;
};
