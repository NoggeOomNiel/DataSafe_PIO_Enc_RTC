
This application:

This is a short description of the device and how to use it.
Purpose:

This is a password manager for a Raspberry Pi Pico with DOGL128 LCD and rotary encoder input.
It stores site records on an SD card as encrypted .enc files (AES-256), can import/export CSV via Serial, and stream/type data to a host using USB Keyboard or Serial.
It also supports PIN-protected access with encrypted PIN saved on SD, supports PIN update and retry limits.
It also supports RTC synchronization (DS3231 or host), usage-based sorting, usage counters, buzzer and RGB LED feedback, and background encoder handling on core1.
It also supports communicating with host PC via USB Serial for importing/exporting data.

Features:

- AES-256 encryption for data files
- PIN-protected access with encrypted PIN saved on SD
- PIN update and retry limits
- RTC synchronization (DS3231 or host)
- Usage-based sorting, usage counters
- Buzzer and RGB LED feedback
- Background encoder handling on core1
- Communicating with host PC via USB Serial for importing/exporting data
 
DataSafe Host PC Application:

An application for easier data management is available separately. It can be downloaded from the projects GitHub repository.
Features:

- Import/Export CSV via Serial
- Makes the maintenance of the data files easier
- Password data can be easily edited, added or deleted in a datagrid control
- Password data can be exported to a CSV file
- Password data can be imported from a CSV file
- DataSafe device can be time synced with a host PC via serial
- DataSafe device can be re-booted via serial
- Runs on Windows 10/11

Operation:

The DataSafe device is connected to a host PC via USB Serial from where it is also powered.
The device is equipped with a removable SD card where the password and PIN data is stored.
All data on the SD card is encrypted using AES-256.
The fron panel of the device is equipped with a LCD display, a rotary encoder, an Auxiliary button and a RGB Led.
A buzzer is also mounted on the device.
The rotary encoder is used for Pin entry, data input, navigation and selection.
The Auxiliary button is used for auxiliary functions (Prompted on screen).
The RGB Led is used for feedback. Usually the color is Blue for normal states and Red for error conditions.

Normal operation is as follows:

- The device is powered on and the SD card is mounted.
- The user is prompted to enter the PIN. This is done by rotating the encoder to select the appropriate pin digit.
  The rotary switch button is pressed to enter the selected digit.
- The PIN is checked and if it is correct the device will ask the user if the Pin should be updated. Press the encoder switch button to change the pin or the Aux button to decline.
- The user have three attempts for a correct PIN entry. If the PIN is incorrect the device will show an error message and the device will be locked.
- Once the pin entry is complete the device will show the Clock screen.
- In order to proceed to the password screen the user must press the encoder switch button.
- In the passwords screen, the decrypted password info is listed.
- The user can navigate the list using the encoder.
- The user can select a password item to view the details by pressing the encoder switch button.
- After selecting the appropriate password item the device will show the password details screen.
- The following items are listed:
  - Site Name
  - Website URL
  - Login...
  - Username
  - Password
  - Return...

- The user can navigate the details using the encoder.
- Apart from the 'Login...' and 'Return...' items, the remaining items will be typed out to the host PC (Simulating a USB connected kwyboard) when the item is selected.
- The 'Login...' item will automatically type out the username, followed by a TAB key, followed by the password, folowed by a TAB and ENTER keys.
- This is usefull if the website requires the username, password and login button is sequenced by a TAB key entry.
- The 'Return...' item will return to the previous screen.
- The device will return to the Clock screen (and dim the display) after a period (30 min.) of inactivity.

Important notes:

- If the Pin file is not present on the SD card the device will erase all saved password files and will prompt the user to set a new PIN.
- This is done to prevent unauthorized access to the device by simply deleting the pin file from the SD card.  
- MAKE SURE TO SAVE A COPY OF THE SD CARD AT ALL TIMES.

Using the DataSafe Host PC Application:

- Most of the functionality of the app is self-explanatory.
- When started, the app will immediatelly start scanning all available COM ports for a DataSafe device.
- For the device to be identified, it must be connected to the host PC via USB Serial and be showing the Clock screen.
- When the device is found the app will display the COM port and the device connect status in the app status bar.
- The user should now enter the device PIN into the 'Enter Pin:' text box.
- The following functions are available:
  - Load DataSafe Items: This will upload all password items from the SD card and display them in the datagrid control.
  - Update DataSafe Items: This will download the contents of the datagrid from the host PC to the device where it will be encrypted and saved on the SD card.
  - Load CSV File: This will load a CSV file from the host PC storage into the datagrid control.
  - Save CSV File: This will save the contents of the datagrid to a CSV file on the host PC storage.
  - Update Time on DataSafe: This will sync the RTC of the DataSafe device with the host PC.
  - Reboot DataSafe: This will reboot the DataSafe device.
 
Important Notes:

- The DataSafe device must be showing the Clock screen while the host app is trying to connect.
- The user Pin must be entered for any of the button functions to work.
- Do NOT try to alter the format of the datagrid control.

Setting up the system for the first time:

- Since the SD card is blank at this time the system will not find the pin.txt file.
- The device will prompt the user to set a new PIN. Follow the instructions on the screen.
- The system will boot up and show the clock screen. The passwords list will be empty at this time.
- If the clock time is not set up correctly, the user should use the DataSafe Host PC Application to sync the clock time.
- Password information can be imported from a CSV file. A default passwords.csv file is available in the repository.
- The user should use the DataSafe Host PC Application to import the default passwords.csv file and then proceed to enter the PIN.
  the password info using the datagrid control
- The new data can then be sent to the DataSafe device using the 'Update DataSafe Items' button.

- NOTE: In the depository is a file named 'passwords.csv'
   This file is an template of the file that must be placed in the root of the SD card.
   Enter your own data into the file using any compattible spreadsheet editor.
   When the system boots up it tries to find this file. If found, the file will be loaded,
   encrypted and saved back to the SD card, replacing the existing encoded file(if any).
   The original passwords.csv file will be deleted from the SD card.

Parts list:
 - Raspberry Pi Pico (versions 1 or 2)
 - GMG12864-06D LCD module
 - EC-11 Rotary encoder
 - SD card holder module
 - Piezo Buzzer (12 mm diameter)
 - RGB LED Common Cathode
 - RTC module (DS3231)
 - BS170 MOSFET
 - 27 ohm resistor
 - 8k2 ohm resistor
 - 22k ohm resistor
 - 82k ohm resistor
 - 47 uF capacitor
 - 100 nF capacitor
 - 3D printed box parts (see Github repository for 3D models)

The schematic diagram is available in the Github repository.

PS: I will publish the source code for the host pc application as well as soon as possible.



