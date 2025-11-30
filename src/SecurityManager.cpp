#include "SecurityManager.h"

/**
 * @brief Constructor for SecurityManager
 */
SecurityManager::SecurityManager()
{
}

/**
 * @brief Initializes the SecurityManager by loading saved PIN from SD card
 */
void SecurityManager::begin()
{
  loadPinFromNVM();
}

/**
 * @brief Returns the current login PIN
 * @return Current PIN as a std::string
 */
std::string SecurityManager::getCurrentPin() const
{
  return currentLoginPin;
}

/**
 * @brief Prompts user to enter PIN and validates it with retry limit
 *
 * Allows up to MAX_PIN_ATTEMPTS attempts. Shows remaining attempts on failure.
 * Locks device if all attempts are exhausted.
 *
 * @param ui UiManager instance for display output
 * @param input InputManager instance for user input
 * @return true if PIN was entered correctly, false if max attempts exceeded
 */
bool SecurityManager::handlePinEntry(UiManager &ui, InputManager &input)
{
  char enteredPin[PIN_DIGITS + 1];
  int attempts = 0;

  constexpr int content_padding = 2;
  constexpr int text_x = BORDER_WIDTH + content_padding + 25;
  constexpr int pin_font_height = 13;
  constexpr int pin_line_height = pin_font_height + 3;
  constexpr int msg_y = BORDER_WIDTH + content_padding + 45;

  while (attempts < MAX_PIN_ATTEMPTS)
  {
    std::string pinAttemptStr;
    if (!getPinFromUser("Enter PIN:", pinAttemptStr, ui, input))
    {
      // do nothing for now
    }
    strncpy(enteredPin, pinAttemptStr.c_str(), PIN_DIGITS);
    enteredPin[PIN_DIGITS] = '\0';
    if (pinAttemptStr == currentLoginPin)
    {
      ui.setFontNormal();
      return true;
    }

    attempts++;
    ui.locate(text_x - 20, msg_y + pin_line_height);
    ui.print("Attempts left: ");
    ui.print(std::to_string(MAX_PIN_ATTEMPTS - attempts));
  }
  ui.copyToLcd();
  delay(2000);

  if (attempts >= MAX_PIN_ATTEMPTS)
  {
    ui.clear();
    ui.drawScreenBorder();
    ui.locate(text_x - 20, msg_y);
    ui.print("Too many attempts!");
    ui.locate(text_x - 20, msg_y + pin_line_height);
    ui.print("Device locked.");
    ui.copyToLcd();
    ui.setFontNormal();
    return false;
  }
  return true;
}

/**
 * @brief Prompts user whether they want to update their PIN
 *
 * Displays a simple yes/no prompt. User presses Enter for Yes or Print button for No.
 *
 * @param ui UiManager instance for display output
 * @param input InputManager instance for user input
 * @return true if user chose to update PIN, false otherwise
 */
bool SecurityManager::askToUpdatePin(UiManager &ui, InputManager &input)
{
  unsigned char *originalFont = ui.getCurrentFont();
  input.resetInactivityTimer();
  ui.setFontNormal(); // Small_7

  ui.clear();
  ui.drawScreenBorder();

  const auto promptLine1 = "Update PIN?";
  const auto promptLine2 = "(ENT=Yes / PRN=No)";

  int L1_x = ui.centerXForText(promptLine1);
  int L2_x = ui.centerXForText(promptLine2);
  if (L1_x < BORDER_WIDTH + 2)
    L1_x = BORDER_WIDTH + 2;
  if (L2_x < BORDER_WIDTH + 2)
    L2_x = BORDER_WIDTH + 2;

  constexpr int y_line1 = BORDER_WIDTH + 15;
  const int y_line2 = y_line1 + ui.getFontHeight() + 5;

  ui.locate(L1_x, y_line1);
  ui.print(promptLine1);
  ui.locate(L2_x, y_line2);
  ui.print(promptLine2);
  ui.copyToLcd();

  while (digitalRead(ENTERBUTTON_PIN) == LOW || digitalRead(PRINTBUTTON_PIN) == LOW)
  {
    delay(10);
  }

  input.clearEnterButton();
  input.clearPrintButton();

  bool decisionMade = false;
  bool updatePin = false;

  while (!decisionMade)
  {
    if (input.isEnterPressed())
    {
      input.clearEnterButton();
      input.sendBlipTone(2500, 50, 30);
      updatePin = true;
      decisionMade = true;
    }
    if (input.isPrintPressed())
    {
      input.clearPrintButton();
      input.sendBlipTone(1500, 50, 30);
      updatePin = false;
      decisionMade = true;
    }
    delay(10);
  }
  ui.restoreFont(originalFont);
  return updatePin;
}

/**
 * @brief Handles the complete PIN update workflow
 *
 * Prompts user to enter a new PIN twice for confirmation. If PINs match and are valid,
 * saves the new PIN to NVM and updates currentLoginPin.
 *
 * @param ui UiManager instance for display output
 * @param input InputManager instance for user input
 * @return true if PIN was successfully updated, false on mismatch or save failure
 */
bool SecurityManager::performPinUpdateProcess(UiManager &ui, InputManager &input)
{
  unsigned char *originalFont = ui.getCurrentFont();
  std::string newPin1, newPin2;

  if (!getPinFromUser("Enter New PIN:", newPin1, ui, input))
  {
  }
  if (!getPinFromUser("Confirm New PIN:", newPin2, ui, input))
  {
  }

  ui.setFontNormal();
  ui.clear();
  ui.drawScreenBorder();

  constexpr int msg_y = BORDER_WIDTH + 20;
  const int msg_y2 = msg_y + ui.getFontHeight() + 3;

  if (newPin1 == newPin2 && newPin1.length() == PIN_DIGITS)
  {
    if (savePinToNVM(newPin1))
    {
      const std::string msg = "PIN Updated!";
      ui.locate(ui.centerXForText(msg.c_str()), msg_y);
      ui.print(msg);
      ui.copyToLcd();
      delay(2000);
      input.resetInactivityTimer();
      ui.restoreFont(originalFont);
      return true;
    }
    else
    {
      std::string msg = "Save Failed!";
      ui.locate(ui.centerXForText(msg.c_str()), msg_y);
      ui.print(msg);
      ui.copyToLcd();
      delay(2000);
      ui.restoreFont(originalFont);
      return false;
    }
  }
  else
  {
    const std::string msg1 = "Mismatch or Invalid.";
    const std::string msg2 = "PIN not updated.";
    ui.locate(ui.centerXForText(msg1.c_str()), msg_y);
    ui.print(msg1);
    ui.locate(ui.centerXForText(msg2.c_str()), msg_y2);
    ui.print(msg2);
    ui.copyToLcd();
    delay(2000);
    ui.restoreFont(originalFont);
    return false;
  }
}

/**
 * @brief Displays PIN entry UI and collects PIN from user via rotary encoder
 *
 * User rotates encoder to select each digit (0-9) and presses Enter to confirm each digit.
 * Process continues until all PIN_DIGITS are entered.
 *
 * @param promptMessage Message to display as prompt (e.g., "Enter PIN:")
 * @param outPin Output parameter - stores the entered PIN
 * @param ui UiManager instance for display output
 * @param input InputManager instance for user input
 * @return true when PIN entry is complete (always succeeds in current implementation)
 */
bool SecurityManager::getPinFromUser(const char *promptMessage, std::string &outPin, UiManager &ui, InputManager &input)
{
  outPin = "";
  input.resetInactivityTimer();
  char currentEnteredPin[PIN_DIGITS + 1];
  int currentDigitIndex = 0;

  constexpr int content_padding = 2;
  constexpr int text_x = BORDER_WIDTH + content_padding + 25;
  constexpr int pin_font_height = 13;
  constexpr int pin_line_height = pin_font_height + 3;
  constexpr int header_y = BORDER_WIDTH + content_padding;
  constexpr int pin_display_y = header_y + pin_line_height;
  constexpr int cursor_y = pin_display_y + pin_line_height;

  unsigned char *originalFont = ui.getCurrentFont();
  ui.setFontConsolas();

  ui.clear();
  ui.drawScreenBorder();

  int prompt_x = ui.centerXForText(promptMessage);
  if (prompt_x < BORDER_WIDTH + content_padding)
    prompt_x = BORDER_WIDTH + content_padding;

  ui.locate(prompt_x, header_y);
  ui.print(promptMessage);

  for (int i = 0; i < PIN_DIGITS; ++i)
    currentEnteredPin[i] = '_';
  currentEnteredPin[PIN_DIGITS] = '\0';

  ui.locate(text_x, pin_display_y);
  ui.print(currentEnteredPin);

  int cursor_x_offset = ui.calcNextXPos(currentEnteredPin, currentDigitIndex);
  ui.locate(text_x + cursor_x_offset, cursor_y);
  ui.print("^");
  ui.copyToLcd();

  while (currentDigitIndex < PIN_DIGITS)
  {
    int currentDigitValue = 0;
    int lastDisplayedDigit = -1;

    input.resetEncoderPosition();
    input.clearEnterButton();

    while (!input.isEnterPressed())
    {
      const int encoderVal = input.getEncoderPosition();

      currentDigitValue = (encoderVal % 10 + 10) % 10;

      if (currentDigitValue != lastDisplayedDigit)
      { // || detailViewNeedsRedraw check?
        currentEnteredPin[currentDigitIndex] = static_cast<char>('0' + currentDigitValue);
        ui.locate(text_x, pin_display_y);
        ui.print(currentEnteredPin);

        ui.fillrect(BORDER_WIDTH + 1, cursor_y, ui.getWidth() - 1 - BORDER_WIDTH - 1, cursor_y + pin_font_height - 1, 0);
        cursor_x_offset = ui.calcNextXPos(currentEnteredPin, currentDigitIndex);
        ui.locate(text_x + cursor_x_offset, cursor_y);
        ui.print("^");
        ui.copyToLcd();
        lastDisplayedDigit = currentDigitValue;
        input.resetInactivityTimer();
      }

      if (input.shouldBlip())
      {
        input.sendBlipTone(2000, 30, 15);
        input.clearBlip();
      }
      delay(10);
    }

    input.sendBlipTone(2500, 50, 30);
    outPin += static_cast<char>('0' + currentDigitValue);
    currentDigitIndex++;
    delay(300);
    input.clearEnterButton();

    if (currentDigitIndex < PIN_DIGITS)
    {
      ui.fillrect(BORDER_WIDTH + 1, cursor_y, ui.getWidth() - 1 - BORDER_WIDTH - 1, cursor_y + pin_font_height - 1, 0);
      cursor_x_offset = ui.calcNextXPos(currentEnteredPin, currentDigitIndex);
      ui.locate(text_x + cursor_x_offset, cursor_y);
      ui.print("^");
      ui.copyToLcd();
    }
  }
  ui.restoreFont(originalFont);
  return true;
}

/**
 * @brief Saves encrypted PIN to SD card storage
 *
 * Encrypts the PIN using XOR and bit rotation, then saves it to PIN_FILENAME on the SD card.
 * Updates currentLoginPin on successful save.
 *
 * @param pinToSave PIN to encrypt and save (must be PIN_DIGITS length)
 * @return true if PIN was successfully saved, false on error
 */
bool SecurityManager::savePinToNVM(const std::string &pinToSave)
{
  if (pinToSave.length() != PIN_DIGITS)
    return false;

  digitalWrite(TFT_CS, HIGH); // Deselect display

  if (SD.exists(PIN_FILENAME))
    SD.remove(PIN_FILENAME);

  File pinFile = SD.open(PIN_FILENAME, FILE_WRITE);
  if (!pinFile)
  {
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  char encryptedPin[PIN_DIGITS];
  for (int i = 0; i < PIN_DIGITS; ++i)
  {
    encryptedPin[i] = rotateLeft(pinToSave[i], ROTATION_COUNT) ^ ENCRYPTION_KEY;
  }

  size_t bytesWritten = pinFile.write(reinterpret_cast<const uint8_t *>(encryptedPin), PIN_DIGITS);
  pinFile.close();
  digitalWrite(SD_CS_PIN, HIGH);

  if (bytesWritten == PIN_DIGITS)
  {
    currentLoginPin = pinToSave;
    return true;
  }
  return false;
}

/**
 * @brief Loads and decrypts PIN from SD card storage
 *
 * Reads encrypted PIN from PIN_FILENAME, decrypts it using XOR and bit rotation.
 * If file doesn't exist or PIN is invalid, uses DEFAULT_LOGIN_PIN and saves it.
 *
 * @return true if valid PIN was loaded from file, false if default PIN was used
 */
bool SecurityManager::loadPinFromNVM()
{
  digitalWrite(TFT_CS, HIGH);
  File pinFile = SD.open(PIN_FILENAME, FILE_READ);
  bool success = false;

  if (pinFile)
  {
    if (pinFile.size() == PIN_DIGITS)
    {
      char encryptedPin[PIN_DIGITS];
      char decryptedPinChars[PIN_DIGITS + 1];
      pinFile.readBytes(encryptedPin, PIN_DIGITS);

      bool validDigits = true;
      for (int i = 0; i < PIN_DIGITS; ++i)
      {
        decryptedPinChars[i] = rotateRight(encryptedPin[i] ^ ENCRYPTION_KEY, ROTATION_COUNT);
        if (decryptedPinChars[i] < '0' || decryptedPinChars[i] > '9')
        {
          validDigits = false;
          break;
        }
      }

      if (validDigits)
      {
        decryptedPinChars[PIN_DIGITS] = '\0';
        currentLoginPin = std::string(decryptedPinChars);
        success = true;
      }
    }
    pinFile.close();
  }
  digitalWrite(SD_CS_PIN, HIGH);

  if (success)
    return true;

  currentLoginPin = DEFAULT_LOGIN_PIN;
  savePinToNVM(currentLoginPin);
  return false;
}

/**
 * @brief Encrypts or decrypts a file using XOR and bit rotation
 *
 * Reads source file in 64-byte chunks, applies encryption (XOR + rotate left),
 * and writes to destination file. The same algorithm works for both encryption
 * and decryption.
 *
 * @param sourceFilename Path to source file on SD card
 * @param destEncryptedFilename Path to destination file on SD card
 * @param deleteSource If true, deletes source file after successful encryption
 * @return true if file was successfully encrypted/decrypted, false on error
 */
bool SecurityManager::fileEncrypt(const char *sourceFilename, const char *destEncryptedFilename, bool deleteSource)
{
  digitalWrite(TFT_CS, HIGH);

  File sourceFile = SD.open(sourceFilename, FILE_READ);
  if (!sourceFile)
  {
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  if (SD.exists(destEncryptedFilename))
  {
    if (!SD.remove(destEncryptedFilename))
    {
      sourceFile.close();
      digitalWrite(SD_CS_PIN, HIGH);
      return false;
    }
  }

  File destFile = SD.open(destEncryptedFilename, FILE_WRITE);
  if (!destFile)
  {
    sourceFile.close();
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  constexpr size_t bufferSize = 64;
  uint8_t buffer[bufferSize];
  int bytesRead;

  while ((bytesRead = sourceFile.read(buffer, bufferSize)) > 0)
  {
    for (int i = 0; i < bytesRead; i++)
    {
      buffer[i] = rotateLeft(buffer[i], ROTATION_COUNT) ^ ENCRYPTION_KEY;
    }
    if (destFile.write(buffer, bytesRead) != static_cast<size_t>(bytesRead))
    {
      sourceFile.close();
      destFile.close();
      SD.remove(destEncryptedFilename);
      digitalWrite(SD_CS_PIN, HIGH);
      return false;
    }
  }

  sourceFile.close();
  destFile.close();

  if (deleteSource)
  {
    SD.remove(sourceFilename);
  }
  digitalWrite(SD_CS_PIN, HIGH);
  return true;
}

/**
 * @brief Rotates bits of a byte to the left
 *
 * @param value Byte value to rotate
 * @param shift Number of positions to rotate (modulo 8)
 * @return Rotated byte value
 */
uint8_t SecurityManager::rotateLeft(uint8_t value, int shift)
{
  shift %= 8;
  if (shift == 0)
    return value;
  return (value << shift) | (value >> (8 - shift));
}

/**
 * @brief Rotates bits of a byte to the right
 *
 * @param value Byte value to rotate
 * @param shift Number of positions to rotate (modulo 8)
 * @return Rotated byte value
 */
uint8_t SecurityManager::rotateRight(uint8_t value, int shift)
{
  shift %= 8;
  if (shift == 0)
    return value;
  return (value >> shift) | (value << (8 - shift));
}
