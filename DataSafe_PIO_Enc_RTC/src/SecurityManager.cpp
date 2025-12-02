#include "SecurityManager.h"
#include <Crypto.h>
#include <AES.h>
#include <SHA256.h>
#include <CTR.h>
#include <string.h>
#include "hardware/structs/rosc.h"

// Helper for RNG using RP2040 ROSC
void fillRandom(uint8_t *buf, size_t size)
{
  for (size_t i = 0; i < size; i++)
  {
    uint8_t byte = 0;
    for (int bit = 0; bit < 8; bit++)
    {
      byte = (byte << 1) | (rosc_hw->randombit & 1);
    }
    buf[i] = byte;
  }
}

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
  // Initialize RNG with some entropy if available, or rely on hardware RNG if platform supports it
  // For RP2040, we might want to seed it better, but for now we rely on the library's default or add entropy later.
  // RNG::AutoSeeded(); // If available in the library version
  // We do NOT load PIN here anymore, as SD card might not be ready.
  // Main loop will handle PIN checking.
}

/**
 * @brief Checks if the PIN file exists on the SD card
 * @return true if PIN file exists, false otherwise
 */
bool SecurityManager::hasPinFile()
{
  return SD.exists(PIN_FILENAME);
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

    bool pinCorrect = false;

    if (SD.exists(PIN_FILENAME))
    {
      // Verify against encrypted file
      ui.locate(BORDER_WIDTH + 2, BORDER_WIDTH + 50);
      ui.print("Checking PIN...    ");
      ui.copyToLcd();
      digitalWrite(TFT_CS, HIGH);
      File pinFile = SD.open(PIN_FILENAME, FILE_READ);
      if (pinFile && pinFile.size() == 100)
      {
        uint8_t header[4];
        pinFile.read(header, 4);
        if (memcmp(header, FILE_HEADER_MAGIC, 4) == 0)
        {
          uint8_t salt[SALT_SIZE];
          pinFile.read(salt, SALT_SIZE);
          uint8_t iv[AES_IV_SIZE];
          pinFile.read(iv, AES_IV_SIZE);
          uint8_t encryptedMK[AES_KEY_SIZE];
          pinFile.read(encryptedMK, AES_KEY_SIZE);
          uint8_t storedHash[32];
          pinFile.read(storedHash, 32);

          uint8_t kek[AES_KEY_SIZE];
          deriveKey(pinAttemptStr, salt, kek);

          uint8_t masterKey[AES_KEY_SIZE];
          CTR<AES256> ctr;
          ctr.setKey(kek, AES_KEY_SIZE);
          ctr.setIV(iv, AES_IV_SIZE);
          ctr.decrypt(masterKey, encryptedMK, AES_KEY_SIZE);

          SHA256 hash;
          uint8_t computedHash[32];
          hash.update(masterKey, AES_KEY_SIZE);
          hash.finalize(computedHash, 32);

          if (memcmp(computedHash, storedHash, 32) == 0)
          {
            pinCorrect = true;
            currentLoginPin = pinAttemptStr; // Update current PIN on success
          }
        }
      }
      pinFile.close();
      digitalWrite(SD_CS_PIN, HIGH);
    }
    else
    {
      // No file, compare against default (first run)
      if (pinAttemptStr == currentLoginPin)
      {
        pinCorrect = true;
      }
    }

    if (pinCorrect)
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
  const auto promptLine2 = "(ENT=Yes / AUX=No)";

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
 * @brief Derives a 256-bit key from the PIN using PBKDF2-HMAC-SHA256
 *
 * @param pin The input PIN string
 * @param salt The salt (SALT_SIZE bytes)
 * @param outputKey The output buffer for the derived key (AES_KEY_SIZE bytes)
 */
void SecurityManager::deriveKey(const std::string &pin, const uint8_t *salt, uint8_t *outputKey)
{
  SHA256 hmac;
  // PBKDF2 implementation using SHA256 HMAC
  // Since rweather/Crypto doesn't have a direct PBKDF2 function exposed easily in all versions,
  // we can use the simple PBKDF2 implementation provided by the library or implement it.
  // Actually, rweather/Crypto has a PBKDF2 class usually? Or we can use the one if available.
  // Checking library docs: It usually has PBKDF2.
  // If not, we can implement a simple loop.
  // For now, let's assume we can use a helper or implement it.
  // To be safe and self-contained, I'll implement a standard PBKDF2 loop here using the SHA256 class as HMAC.

  // Actually, rweather/Crypto usually provides PBKDF2. Let's try to include it if possible.
  // But to avoid dependency issues if it's not in the specific version, I'll implement it manually using HMAC-SHA256.
  // It is straightforward.

  // Standard PBKDF2(Password, Salt, c, dkLen, PRF)
  // PRF = HMAC-SHA256

  // We only need 1 block of output (32 bytes) since AES key is 32 bytes and SHA256 is 32 bytes.
  // So we just need U_1 ^ U_2 ... ^ U_c

  uint8_t U[32];
  uint8_t T[32];

  // Initial HMAC with Salt + INT(1)
  hmac.resetHMAC(pin.c_str(), pin.length());
  hmac.update(salt, SALT_SIZE);
  uint8_t blockNum[4] = {0, 0, 0, 1}; // Big-endian 1
  hmac.update(blockNum, 4);
  hmac.finalizeHMAC(pin.c_str(), pin.length(), U, sizeof(U));

  memcpy(T, U, sizeof(T));

  for (int i = 1; i < PBKDF2_ITERATIONS; ++i)
  {
    hmac.resetHMAC(pin.c_str(), pin.length());
    hmac.update(U, sizeof(U));
    hmac.finalizeHMAC(pin.c_str(), pin.length(), U, sizeof(U));

    for (int j = 0; j < 32; ++j)
    {
      T[j] ^= U[j];
    }
  }

  memcpy(outputKey, T, AES_KEY_SIZE);
}

/**
 * @brief Saves encrypted PIN to SD card storage
 *
 * In this new scheme, we verify the PIN by checking if we can
 * decrypt a known token or simply by hash.
 * We store: [Salt] [IV] [Encrypted Master Key] [Hash of Master Key]
 */
bool SecurityManager::savePinToNVM(const std::string &pinToSave)
{
  digitalWrite(TFT_CS, HIGH); // Deselect display

  uint8_t masterKey[AES_KEY_SIZE];
  bool haveMasterKey = false;

  // Try to load existing Master Key using currentLoginPin (Old PIN)
  if (SD.exists(PIN_FILENAME))
  {
    File oldFile = SD.open(PIN_FILENAME, FILE_READ);
    if (oldFile)
    {
      if (oldFile.size() == 100)
      {
        uint8_t header[4];
        oldFile.read(header, 4);
        if (memcmp(header, FILE_HEADER_MAGIC, 4) == 0)
        {
          uint8_t oldSalt[SALT_SIZE];
          oldFile.read(oldSalt, SALT_SIZE);
          uint8_t oldIV[AES_IV_SIZE];
          oldFile.read(oldIV, AES_IV_SIZE);
          uint8_t encryptedMK[AES_KEY_SIZE];
          oldFile.read(encryptedMK, AES_KEY_SIZE);
          uint8_t oldHash[32];
          oldFile.read(oldHash, 32);

          uint8_t oldKek[AES_KEY_SIZE];
          deriveKey(currentLoginPin, oldSalt, oldKek);

          CTR<AES256> ctr;
          ctr.setKey(oldKek, AES_KEY_SIZE);
          ctr.setIV(oldIV, AES_IV_SIZE);
          ctr.decrypt(masterKey, encryptedMK, AES_KEY_SIZE);

          // Verify
          SHA256 hash;
          uint8_t computedHash[32];
          hash.update(masterKey, AES_KEY_SIZE);
          hash.finalize(computedHash, 32);

          if (memcmp(computedHash, oldHash, 32) == 0)
          {
            haveMasterKey = true;
          }
        }
      }
      oldFile.close();
    }
  }

  if (SD.exists(PIN_FILENAME))
    SD.remove(PIN_FILENAME);

  File pinFile = SD.open(PIN_FILENAME, FILE_WRITE);
  if (!pinFile)
  {
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  if (!haveMasterKey)
  {
    // Generate new Master Key
    fillRandom(masterKey, AES_KEY_SIZE);
  }

  // 1. Generate Salt
  uint8_t salt[SALT_SIZE];
  fillRandom(salt, SALT_SIZE);

  // 2. Derive Key from PIN (KEK - Key Encryption Key)
  uint8_t kek[AES_KEY_SIZE];
  deriveKey(pinToSave, salt, kek);

  // 4. Encrypt Master Key with New PIN
  uint8_t iv[AES_IV_SIZE];
  fillRandom(iv, AES_IV_SIZE);

  uint8_t encryptedMK[AES_KEY_SIZE];
  CTR<AES256> ctr;
  ctr.setKey(kek, AES_KEY_SIZE);
  ctr.setIV(iv, AES_IV_SIZE);
  ctr.encrypt(encryptedMK, masterKey, AES_KEY_SIZE);

  // 5. Calculate Hash of Master Key (for verification)
  uint8_t mkHash[32];
  SHA256 hash;
  hash.update(masterKey, AES_KEY_SIZE);
  hash.finalize(mkHash, 32);

  // 6. Write to file
  pinFile.write((const uint8_t *)FILE_HEADER_MAGIC, 4);
  pinFile.write(salt, SALT_SIZE);
  pinFile.write(iv, AES_IV_SIZE);
  pinFile.write(encryptedMK, AES_KEY_SIZE);
  pinFile.write(mkHash, 32);

  pinFile.close();
  digitalWrite(SD_CS_PIN, HIGH);

  currentLoginPin = pinToSave;
  return true;
}

/**
 * @brief Loads and decrypts PIN from SD card storage
 *
 * In the new scheme, we verify the PIN by checking if we can
 * decrypt a known token or simply by hash.
 * This function handles initial setup if the PIN file doesn't exist.
 *
 * @return true if valid PIN was loaded (verified), false otherwise
 */
bool SecurityManager::loadPinFromNVM()
{
  digitalWrite(TFT_CS, HIGH);

  if (!SD.exists(PIN_FILENAME))
  {
    // File missing - we do NOT create default anymore.
    // Caller should handle this (e.g. prompt for new PIN).
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  // Validate file format - check size and header
  File pinFile = SD.open(PIN_FILENAME, FILE_READ);
  if (!pinFile)
  {
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  bool isValidFormat = false;
  if (pinFile.size() == 100) // Expected size: 4 (header) + 16 (salt) + 16 (IV) + 32 (encrypted MK) + 32 (hash)
  {
    uint8_t header[4];
    pinFile.read(header, 4);
    if (memcmp(header, FILE_HEADER_MAGIC, 4) == 0)
    {
      isValidFormat = true;
    }
  }
  pinFile.close();

  if (!isValidFormat)
  {
    // Old or corrupted file - delete it
    SD.remove(PIN_FILENAME);
    // We do NOT create default here.
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  digitalWrite(SD_CS_PIN, HIGH);
  return true;
}

/**
 * @brief Encrypts a file using AES-256-CTR
 */
bool SecurityManager::encryptFile(const char *sourceFilename, const char *destEncryptedFilename, UiManager *ui)
{
  digitalWrite(TFT_CS, HIGH);

  if (ui)
  {
    ui->locate(BORDER_WIDTH + 2, BORDER_WIDTH + 30);
    ui->print("Encrypting...   ");
    ui->copyToLcd();
  }

  // 1. Get Master Key
  uint8_t masterKey[AES_KEY_SIZE];

  File pinFile = SD.open(PIN_FILENAME, FILE_READ);
  if (!pinFile)
  {
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  // Read Salt, IV, EncryptedMK
  pinFile.seek(4); // Skip header
  uint8_t salt[SALT_SIZE];
  pinFile.read(salt, SALT_SIZE);
  uint8_t pinIV[AES_IV_SIZE];
  pinFile.read(pinIV, AES_IV_SIZE);
  uint8_t encryptedMK[AES_KEY_SIZE];
  pinFile.read(encryptedMK, AES_KEY_SIZE);
  pinFile.close();

  // Derive KEK
  uint8_t kek[AES_KEY_SIZE];
  deriveKey(currentLoginPin, salt, kek);

  // Decrypt Master Key
  CTR<AES256> ctrMK;
  ctrMK.setKey(kek, AES_KEY_SIZE);
  ctrMK.setIV(pinIV, AES_IV_SIZE);
  ctrMK.decrypt(masterKey, encryptedMK, AES_KEY_SIZE);

  File sourceFile = SD.open(sourceFilename, FILE_READ);
  if (!sourceFile)
  {
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  if (SD.exists(destEncryptedFilename))
    SD.remove(destEncryptedFilename);

  File destFile = SD.open(destEncryptedFilename, FILE_WRITE);
  if (!destFile)
  {
    sourceFile.close();
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  // Generate IV for the file
  uint8_t fileIV[AES_IV_SIZE];
  fillRandom(fileIV, AES_IV_SIZE);

  // Write IV to beginning of file
  destFile.write(fileIV, AES_IV_SIZE);

  CTR<AES256> fileCTR;
  fileCTR.setKey(masterKey, AES_KEY_SIZE);
  fileCTR.setIV(fileIV, AES_IV_SIZE);

  constexpr size_t bufferSize = 64;
  uint8_t buffer[bufferSize];
  int bytesRead;

  while ((bytesRead = sourceFile.read(buffer, bufferSize)) > 0)
  {
    fileCTR.encrypt(buffer, buffer, bytesRead);
    destFile.write(buffer, bytesRead);
  }

  sourceFile.close();
  destFile.close();
  digitalWrite(SD_CS_PIN, HIGH);
  return true;
}

/**
 * @brief Decrypts a file using AES-256-CTR
 */
bool SecurityManager::decryptFile(const char *sourceEncryptedFilename, const char *destPlainFilename, UiManager *ui)
{
  digitalWrite(TFT_CS, HIGH);

  if (ui)
  {
    ui->clear();
    ui->drawScreenBorder();
    int centerX = ui->centerXForText("Decrypting...");
    int centerY = ui->centerYForText("Decrypting...");
    ui->locate(centerX, centerY);
    ui->print("Decrypting...");
    ui->copyToLcd();
  }

  // 1. Get Master Key
  uint8_t masterKey[AES_KEY_SIZE];

  File pinFile = SD.open(PIN_FILENAME, FILE_READ);
  if (!pinFile)
  {
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  // Read Salt, IV, EncryptedMK
  pinFile.seek(4); // Skip header
  uint8_t salt[SALT_SIZE];
  pinFile.read(salt, SALT_SIZE);
  uint8_t pinIV[AES_IV_SIZE];
  pinFile.read(pinIV, AES_IV_SIZE);
  uint8_t encryptedMK[AES_KEY_SIZE];
  pinFile.read(encryptedMK, AES_KEY_SIZE);
  pinFile.close();

  // Derive KEK
  uint8_t kek[AES_KEY_SIZE];
  deriveKey(currentLoginPin, salt, kek);

  // Decrypt Master Key
  CTR<AES256> ctrMK;
  ctrMK.setKey(kek, AES_KEY_SIZE);
  ctrMK.setIV(pinIV, AES_IV_SIZE);
  ctrMK.decrypt(masterKey, encryptedMK, AES_KEY_SIZE);

  File sourceFile = SD.open(sourceEncryptedFilename, FILE_READ);
  if (!sourceFile)
  {
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  if (SD.exists(destPlainFilename))
    SD.remove(destPlainFilename);

  File destFile = SD.open(destPlainFilename, FILE_WRITE);
  if (!destFile)
  {
    sourceFile.close();
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  // Read IV from beginning of file
  uint8_t fileIV[AES_IV_SIZE];
  if (sourceFile.read(fileIV, AES_IV_SIZE) != AES_IV_SIZE)
  {
    sourceFile.close();
    destFile.close();
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  CTR<AES256> fileCTR;
  fileCTR.setKey(masterKey, AES_KEY_SIZE);
  fileCTR.setIV(fileIV, AES_IV_SIZE);

  constexpr size_t bufferSize = 64;
  uint8_t buffer[bufferSize];
  int bytesRead;

  while ((bytesRead = sourceFile.read(buffer, bufferSize)) > 0)
  {
    fileCTR.decrypt(buffer, buffer, bytesRead); // Decrypt in place
    destFile.write(buffer, bytesRead);
  }

  sourceFile.close();
  destFile.close();
  digitalWrite(SD_CS_PIN, HIGH);
  return true;
}
