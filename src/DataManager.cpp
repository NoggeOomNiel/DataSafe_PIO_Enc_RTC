#include "DataManager.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include <algorithm>
#include <cctype>

static void trim(std::string &s)
{
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                  { return !std::isspace(ch); }));
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                       { return !std::isspace(ch); })
              .base(),
          s.end());
}

/**
 * @brief Default constructor for DataManager
 */
DataManager::DataManager()
{
}

/**
 * @brief Returns pointer to the internal siteData array
 * @return Pointer to 2D array containing all site/password records
 */
/**
 * @brief Returns pointer to the internal siteData array
 * @return Pointer to 2D array containing all site/password records
 */
std::string (*DataManager::getSiteData()) [SITE_DATA_FIELDS_COUNT]
{
  return siteData;
}

/**
 * @brief Loads password data from SD card (encrypted or plain CSV)
 *
 * Attempts to load from encrypted .enc file first. If not found or if only plain
 * .csv exists, loads from plain file and encrypts it. Displays progress via UI.
 *
 * @param baseFilePath Base filename (without extension) on SD card
 * @param ui Optional UiManager pointer for displaying load progress
 * @return true if data was successfully loaded (or initialized empty), false on error
 */
bool DataManager::loadData(const char *baseFilePath, UiManager *ui)
{
  const std::string encryptedFilePath = std::string(baseFilePath) + ".enc";
  const std::string plainFilePath = std::string(baseFilePath);
  bool loadedData = false;
  File dataFile;

  digitalWrite(TFT_CS, HIGH);

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
      std::string currentLine = "";

      while (dataFile.available() && site < MaxSites)
      {
        const char encryptedChar = dataFile.read();
        const char charAfterXor = encryptedChar ^ ENCRYPTION_KEY;
        const char fullyDecryptedChar = SecurityManager::rotateRight(charAfterXor, ROTATION_COUNT);

        if (fullyDecryptedChar == '\n')
        {
          trim(currentLine);
          parseCSVLine(currentLine, site);
          currentLine = "";
          if (++site >= MaxSites)
            break;
        }
        else if (fullyDecryptedChar == '\r')
        {
          // ignore
        }
        else
        {
          currentLine += fullyDecryptedChar;
        }
      }
      if (currentLine.length() > 0 && site < MaxSites)
      {
        parseCSVLine(currentLine, site);
        site++;
      }
      dataFile.close();
      loadedData = true;
      if (ui)
      {
        ui->locate(4, 30);
        ui->print("Secure load OK    ");
        ui->copyToLcd();
        delay(2000);
      }
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
        std::string line = dataFile.readStringUntil('\n').c_str();
        parseCSVLine(line, site);
        if (++site >= MaxSites)
          break;
      }
      dataFile.close();
      loadedData = true;

      if (ui)
      {
        ui->locate(4, 30);
        ui->print(".csv File load OK");
        ui->copyToLcd();
        ui->locate(4, 40);
        ui->print("Securing data...");
        ui->copyToLcd();
      }

      SecurityManager sec;
      if (sec.fileEncrypt(plainFilePath.c_str(), encryptedFilePath.c_str(), DELETE_UNENCRYPTED_FILE))
      {
        if (ui)
        {
          ui->print("OK");
          ui->copyToLcd();
        }
      }
      else
      {
        if (ui)
        {
          ui->print("FAIL");
          ui->copyToLcd();
        }
      }
      delay(2000);
      return true;
    }
  }

  if (!loadedData)
  {
    if (ui)
    {
      ui->locate(4, 20);
      ui->print("No valid file found.");
      ui->copyToLcd();
      ui->locate(4, 30);
      ui->print("Initializing empty.");
      ui->copyToLcd();
    }
    for (auto &s : siteData)
    {
      for (int i = 0; i < SITE_DATA_FIELDS_COUNT; ++i)
      {
        if (i == USAGE_COUNT_COLUMN_INDEX)
          s[i] = "0";
        else
          s[i] = "";
      }
    }
    loadedData = true;
    delay(2000);
    return true;
  }

  digitalWrite(SD_CS_PIN, HIGH);
  return false;
}

/**
 * @brief Saves password data to encrypted file on SD card
 *
 * Writes data to temporary plain file, then encrypts it to .enc file.
 * Deletes temporary file after successful encryption.
 *
 * @param baseFilePath Base filename (without extension) on SD card
 * @return true if data was successfully saved and encrypted, false on error
 */
bool DataManager::saveData(const char *baseFilePath)
{
  digitalWrite(TFT_CS, HIGH);

  const std::string plainFilePathTemp = std::string(baseFilePath) + ".tmp";
  const std::string finalEncryptedPath = std::string(baseFilePath) + ".enc";

  File tempFile = SD.open(plainFilePathTemp.c_str(), FILE_WRITE);

  if (!tempFile)
  {
    digitalWrite(SD_CS_PIN, HIGH);
    return false;
  }

  for (const auto &i : siteData)
  {
    if (i[SITE_NAME_INDEX].length() == 0)
      break;
    for (int j = 0; j < SITE_DATA_FIELDS_COUNT; ++j)
    {
      tempFile.print(i[j].c_str());
      if (j < SITE_DATA_FIELDS_COUNT - 1)
        tempFile.print(",");
    }
    tempFile.println();
  }
  tempFile.close();

  SecurityManager sec;
  const bool success = sec.fileEncrypt(plainFilePathTemp.c_str(), finalEncryptedPath.c_str(), true);

  digitalWrite(SD_CS_PIN, HIGH);
  return success;
}

/**
 * @brief Parses a CSV line and populates a site data entry
 *
 * Handles quoted fields, escaped quotes, and various CSV formats:
 * - Full format: Name,URL,Login...,User,Pass,Return...,Count
 * - Short format: Name,URL,User,Pass (auto-adds Login... and Return...)
 * Validates and ensures usage count is numeric.
 *
 * @param lineContent CSV line to parse
 * @param siteIdx Index in siteData array to populate
 */
void DataManager::parseCSVLine(const std::string &lineContent, int siteIdx)
{
  std::string currentLine = lineContent;
  trim(currentLine);

  for (int k = 0; k < SITE_DATA_FIELDS_COUNT; ++k)
  {
    if (k == USAGE_COUNT_COLUMN_INDEX)
      siteData[siteIdx][k] = "0";
    else
      siteData[siteIdx][k] = "";
  }

  constexpr int MAX_PARTS = 16;
  std::string parts[MAX_PARTS];
  int partsCount = 0;

  std::string token = "";
  bool inQuotes = false;
  const int len = static_cast<int>(currentLine.length());

  for (int i = 0; i <= len; ++i)
  {
    const char c = (i < len) ? currentLine[i] : '\0';

    if (inQuotes)
    {
      if (c == '"')
      {
        if (i + 1 < len && currentLine[i + 1] == '"')
        {
          token += '"';
          ++i;
        }
        else
        {
          inQuotes = false;
        }
      }
      else if (c == '\0')
      {
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
        trim(token);
        if (partsCount < MAX_PARTS)
          parts[partsCount++] = token;
        token = "";
      }
      else
      {
        token += c;
      }
    }
  }

  for (int i = 0; i < partsCount; ++i)
    trim(parts[i]);

  std::string finalFields[6];
  bool ok = false;

  if (partsCount >= 6)
  {
    if (parts[2] == "Login..." && parts[5] == "Return...")
    {
      for (int i = 0; i < 6; ++i)
        finalFields[i] = parts[i];
      ok = true;
    }
    else
    {
      for (int i = 0; i < 6; ++i)
        finalFields[i] = parts[i];
      ok = true;
    }
  }
  else if (partsCount >= 4)
  {
    finalFields[0] = parts[0];
    finalFields[1] = parts[1];
    finalFields[2] = std::string("Login...");
    finalFields[3] = parts[2];
    finalFields[4] = parts[3];
    finalFields[5] = std::string("Return...");
    ok = true;
  }
  else
  {
    int item = 0;
    const int limit = partsCount < SITE_DATA_FIELDS_COUNT ? partsCount : SITE_DATA_FIELDS_COUNT;
    for (int i = 0; i < limit; ++i)
      siteData[siteIdx][item++] = parts[i];
    ok = false;
  }

  if (ok)
  {
    constexpr int copyCount = SITE_DATA_FIELDS_COUNT < 6 ? SITE_DATA_FIELDS_COUNT : 6;
    for (int j = 0; j < copyCount; ++j)
      siteData[siteIdx][j] = finalFields[j];
    for (int j = copyCount; j < SITE_DATA_FIELDS_COUNT; ++j)
    {
      if (j == USAGE_COUNT_COLUMN_INDEX)
        siteData[siteIdx][j] = "0";
      else
        siteData[siteIdx][j] = "";
    }
  }

  if (siteData[siteIdx][SITE_NAME_INDEX].length() > 0)
  {
    const std::string &usageStr = siteData[siteIdx][USAGE_COUNT_COLUMN_INDEX];
    bool is_digits = true;
    if (usageStr.length() == 0)
      is_digits = false;
    else
    {
      for (unsigned int i = 0; i < usageStr.length(); ++i)
      {
        if (!isDigit(usageStr[i]))
        {
          is_digits = false;
          break;
        }
      }
    }
    if (!is_digits)
      siteData[siteIdx][USAGE_COUNT_COLUMN_INDEX] = "0";
  }
}

/**
 * @brief Sorts password entries by usage count (descending) then alphabetically
 *
 * Uses selection sort. Higher usage count appears first. Entries with equal
 * usage are sorted alphabetically by site name.
 */
void DataManager::sortData()
{
  int n = 0;
  for (const auto &i : siteData)
  {
    if (i[SITE_NAME_INDEX].length() == 0)
      break;
    n++;
  }
  if (n <= 1)
    return;

  for (int i = 0; i < n - 1; i++)
  {
    int best_idx = i;
    for (int j = i + 1; j < n; j++)
    {
      const long usageJ = atol(siteData[j][USAGE_COUNT_COLUMN_INDEX].c_str());
      const long usageBest = atol(siteData[best_idx][USAGE_COUNT_COLUMN_INDEX].c_str());

      if (usageJ > usageBest || (usageJ == usageBest && siteData[j][SITE_NAME_INDEX].compare(siteData[best_idx][SITE_NAME_INDEX]) < 0))
      {
        best_idx = j;
      }
    }
    if (best_idx != i)
      swapSiteRows(i, best_idx);
  }
}

/**
 * @brief Swaps two site data rows in the siteData array
 *
 * @param r1 Index of first row
 * @param r2 Index of second row
 */
void DataManager::swapSiteRows(int r1, int r2)
{
  if (r1 == r2 || r1 < 0 || r1 >= MaxSites || r2 < 0 || r2 >= MaxSites)
    return;
  for (int j = 0; j < SITE_DATA_FIELDS_COUNT; ++j)
  {
    std::swap(siteData[r1][j], siteData[r2][j]);
  }
}

/**
 * @brief Increments the usage counter for a password entry
 *
 * @param index Index of the site entry to increment
 */
void DataManager::incrementUsage(int index)
{
  long currentUsage = atol(siteData[index][USAGE_COUNT_COLUMN_INDEX].c_str());
  currentUsage++;
  siteData[index][USAGE_COUNT_COLUMN_INDEX] = std::to_string(currentUsage);
}

/**
 * @brief Streams decrypted password data to host via Serial
 *
 * Reads encrypted .enc file, decrypts it line by line, and sends to Serial.
 * Displays progress on UI. Supports cancellation via atomic flag.
 *
 * @param baseFilePath Base filename (without extension) on SD card
 * @param cancelFlag Atomic boolean flag to cancel operation early
 * @param ui Optional UiManager pointer for displaying progress
 */
void DataManager::streamOut(const char *baseFilePath, std::atomic<bool> &cancelFlag, UiManager *ui)
{
  digitalWrite(TFT_CS, HIGH);
  std::string encryptedFilePath = std::string(baseFilePath) + ".enc";
  File dataFile = SD.open(encryptedFilePath.c_str(), FILE_READ);

  if (!dataFile)
  {
    if (ui)
    {
      ui->clear();
      ui->locate(0, 20);
      ui->print("Error: Encrypted");
      ui->locate(0, 30);
      ui->print("file not found!");
      ui->copyToLcd();
      delay(3000);
    }
    digitalWrite(SD_CS_PIN, HIGH);
    return;
  }

  if (ui)
  {
    ui->clear();
    ui->drawScreenBorder();
    ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 12);
    ui->print("Sending CSV...");
    ui->copyToLcd();
  }

  std::string currentLine = "";
  while (dataFile.available())
  {
    const char encryptedChar = dataFile.read();
    const char afterXor = encryptedChar ^ ENCRYPTION_KEY;
    const char dec = SecurityManager::rotateRight(afterXor, ROTATION_COUNT);

    if (dec == '\n')
    {
      trim(currentLine);
      Serial.println(currentLine.c_str());
      currentLine = "";
      if (cancelFlag.load())
        break;
    }
    else if (dec == '\r')
    {
      // ignore
    }
    else
    {
      currentLine += dec;
    }
    sleep_ms(1);
  }

  if (currentLine.length() > 0)
  {
    trim(currentLine);
    Serial.println(currentLine.c_str());
  }

  dataFile.close();
  digitalWrite(SD_CS_PIN, HIGH);

  if (ui)
  {
    ui->clear();
    ui->drawScreenBorder();
    ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 12);
    ui->print("CSV sent!");
    ui->copyToLcd();
    delay(3000);
  }
}

/**
 * @brief Receives CSV data from host via Serial and imports it
 *
 * Receives data with timeout, backs up existing encrypted file (timestamped),
 * encrypts new data, and loads it. Restores backup on failure.
 * Displays progress and status on UI.
 *
 * @param baseFilePath Base filename (without extension) on SD card
 * @param ui Optional UiManager pointer for displaying progress
 */
void DataManager::streamIn(const char *baseFilePath, UiManager *ui)
{

  if (ui)
  {
    ui->clear();
    ui->drawScreenBorder();
    ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 12);
    ui->print("Receiving CSV...");
    ui->copyToLcd();
  }

  std::string basePath = std::string(baseFilePath);
  std::string tempPlainPath = basePath + ".tmp";
  std::string finalEncryptedPath = basePath + ".enc";

  digitalWrite(TFT_CS, HIGH);

  File tempFile = SD.open(tempPlainPath.c_str(), FILE_WRITE);
  if (!tempFile)
  {
    if (ui)
    {
      ui->clear();
      ui->drawScreenBorder();
      ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 18);
      ui->print("Err open tmp");
      ui->copyToLcd();
      delay(2000);
    }
    digitalWrite(SD_CS_PIN, HIGH);
    return;
  }

  constexpr unsigned long overallTimeout = 30000;
  constexpr unsigned long idleAfterDataTimeout = 2000;
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
      break;
    }
    sleep_ms(10);
  }
  tempFile.close();

  if (!receivedAny)
  {
    SD.remove(tempPlainPath.c_str());
    if (ui)
    {
      ui->clear();
      ui->drawScreenBorder();
      ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 18);
      ui->print("No data received");
      ui->copyToLcd();
      delay(1500);
    }
    digitalWrite(SD_CS_PIN, HIGH);
    return;
  }

  std::string backupPath = "";
  bool backedUp = false;
  if (SD.exists(finalEncryptedPath.c_str()))
  {
    datetime_t t;
    std::string ts;
    if (rtc_get_datetime(&t))
    {
      char buf[32];
      sprintf(buf, "%04d%02d%02d_%02d%02d%02d", t.year, t.month, t.day, t.hour, t.min, t.sec);
      ts = std::string(buf);
    }
    else
    {
      ts = std::to_string(millis());
    }
    backupPath = finalEncryptedPath + "." + ts;

    if (SD.rename(finalEncryptedPath.c_str(), backupPath.c_str()))
    {
      backedUp = true;
    }
    else
    {
      // Copy fallback
      if (File src = SD.open(finalEncryptedPath.c_str(), FILE_READ))
      {
        if (File dst = SD.open(backupPath.c_str(), FILE_WRITE))
        {
          constexpr size_t bufSize = 64;
          uint8_t buf[bufSize];
          int r;
          while ((r = src.read(buf, bufSize)) > 0)
          {
            dst.write(buf, r);
          }
          dst.close();
          if (File chk = SD.open(backupPath.c_str(), FILE_READ))
          {
            backedUp = (chk.size() > 0);
            chk.close();
          }
        }
        src.close();
      }
      if (backedUp)
        SD.remove(finalEncryptedPath.c_str());
      else
      {
        SD.remove(tempPlainPath.c_str());
        if (ui)
        {
          ui->clear();
          ui->drawScreenBorder();
          ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 18);
          ui->print("Backup failed");
          ui->copyToLcd();
          delay(2000);
        }
        digitalWrite(SD_CS_PIN, HIGH);
        return;
      }
    }
  }

  if (ui)
  {
    ui->clear();
    ui->drawScreenBorder();
    ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 12);
    ui->print("Securing data...");
    ui->copyToLcd();
  }

  SecurityManager sec;
  if (!sec.fileEncrypt(tempPlainPath.c_str(), finalEncryptedPath.c_str(), true))
  {
    if (backedUp)
    {
      SD.rename(backupPath.c_str(), finalEncryptedPath.c_str());
    }
    if (ui)
    {
      ui->clear();
      ui->drawScreenBorder();
      ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 18);
      ui->print("Encrypt FAIL");
      ui->copyToLcd();
      delay(2000);
    }
    digitalWrite(SD_CS_PIN, HIGH);
    return;
  }

  if (ui)
  {
    ui->clear();
    ui->drawScreenBorder();
    ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 12);
    ui->print("Loading new data...");
    ui->copyToLcd();
  }

  if (!loadData(baseFilePath, ui))
  {
    if (backedUp)
    {
      SD.remove(finalEncryptedPath.c_str());
      SD.rename(backupPath.c_str(), finalEncryptedPath.c_str());
      loadData(baseFilePath, ui);
    }
    if (ui)
    {
      ui->clear();
      ui->drawScreenBorder();
      ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 18);
      ui->print("Load Failed");
      ui->copyToLcd();
      delay(2000);
    }
    digitalWrite(SD_CS_PIN, HIGH);
    return;
  }

  digitalWrite(SD_CS_PIN, HIGH);
  sortData();

  if (ui)
  {
    ui->clear();
    ui->drawScreenBorder();
    ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 12);
    ui->print("Receive complete");
    ui->locate(BORDER_WIDTH + 4, BORDER_WIDTH + 24);
    if (backedUp)
      ui->print("Backup saved");
    else
      ui->print("No prior file");
    ui->copyToLcd();
    delay(1500);
  }
}
