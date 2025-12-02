#pragma once

#include <Arduino.h>
#include <SD.h>
#include <string>
#include <atomic>
#include "Constants.h"
#include "SecurityManager.h"
#include "UiManager.h" // For feedback during load

class DataManager
{
public:
  /** @brief Default constructor */
  DataManager();

  // Data Access
  /** @brief Returns pointer to the internal siteData array */
  std::string (*getSiteData())[SITE_DATA_FIELDS_COUNT];

  // File Operations
  /** @brief Loads password data from SD card (encrypted or plain CSV) */
  bool loadData(const char *baseFilePath, UiManager *ui = nullptr);

  /** @brief Saves password data to encrypted file on SD card */
  bool saveData(const char *baseFilePath, UiManager *ui = nullptr);

  /** @brief Deletes all data files (.enc, .csv, .scv) for the given base path */
  void deleteAllDataFiles(const char *baseFilePath);

  // Data Manipulation
  /** @brief Sorts password entries by usage count (descending) then alphabetically */
  void sortData();

  /** @brief Increments the usage counter for a password entry */
  void incrementUsage(int index);

  /** @brief Parses a CSV line and populates a site data entry */
  void parseCSVLine(const std::string &lineContent, int siteIdx);

  // Streaming
  /** @brief Streams decrypted password data to host via Serial */
  void streamOut(const char *baseFilePath, std::atomic<bool> &cancelFlag, UiManager *ui = nullptr);

  /** @brief Receives CSV data from host via Serial and imports it */
  void streamIn(const char *baseFilePath, UiManager *ui = nullptr);

private:
  std::string siteData[MaxSites][SITE_DATA_FIELDS_COUNT];

  /** @brief Swaps two site data rows in the siteData array */
  void swapSiteRows(int r1, int r2);

  /** @brief Encrypts or decrypts a file using XOR and bit rotation */
  bool fileEncrypt(const char *sourceFilename, const char *destEncryptedFilename, bool deleteSource);
};
