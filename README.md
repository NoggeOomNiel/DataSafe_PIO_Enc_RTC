Please Note: This project is still Work In Progress!!!

Added the installer for the DataSafePlus utility program. (setup.exe and DataSafePlus Installer.msi)

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

   In the depository there is a file named 'passwords.csv'
   This file is an example of the file that must be placed in the root of the SD card.
   Enter your own data into the file using MS Word, or any other compattible spreadsheet editor.
   When the system boots up it tries to find this file. If found, the file will be loaded, encrypted and saved back to the SD card, replacing the existing encoded file(if any).
   The original passwords.csv file will be deleted from the SD card.

   When the application is run for the first time the user will be informed that no pin could be found and ask for the system default pin. This pin is '0000'
   The user pin should then be changed for a new unique pin number (4 digits). Follow the screen prompts.

   The system is using a simple encription method and will be possible to crack by security boffins. However, it is good enough for the purpose.

   Here is a short description of how to use this application:

   1) Plug the usb cable into an available port on the host PC. The Pico device is powered by the port. Windows 11 was tested.
   2) The system will start looking for the pin file. If found it propts the user to enter the pin. If not found the user must enter the default.
   3) Entries are made by rotation of the encoder knob. To enter data the encoder knob should be pushed in (encoder push button switch)
   4) After the pin is authenticated, the system looks for the passwords file. If not found it will enter an infinite loop.
   5) After loading the password information it will try to sync time with the RTC module. If successful, the pico chip rtc will be loaded with the time and then proceed to show a clock screen.
   6) If the sync process fails, as it would when starting for the 1st time, the system time may be loaded from the host PC by using the utility Windows app (DataSafePlus.exe)
   7) In the clock screen the user can push the encoder button to proceed to the site selection screen where the required site can be selected from the list.
   8) Once again, selection is done by rotating the encoder knob and pushing the knob to enter.
   9) The system will then show the details of the selected item. The user should select the item to be keyed out to the host.
   10) The system emulates a HID keyboard and will type the information to wherever the cursor on the host screen is located.
   11) If the 'Login...' option is selected the system will first key out the password, followed by a tab key, then key out the password followed by tab and enter keys. (for sites where the username and password entry boxes are a tab seperated. The enter key will initiate the login.
   12) The 'Return...' option will send focus back to the site list screen
   13) The system will time out after 30 seconds of inactivity after which the clock screen will be shown and the baclight is switched off. Press the encoder knob to wake up.

 Windows utility app (DataSafePlus.exe)
 This app is used to conveniently communicate with the Pico device. Comms is via the same usb cable. The features are as follows:
 1) Sync the host time to the device.
 2) Pulls the password list from the device into a grid control where it can be added to or edited.
 3) Push the new data back to the device.
 4) Do a remote reboot of the device.
 5) This app can only connect to the device when the device is showing the clock screen.
 6) After connection, the user should enter the site listing screen. The host app can communicate with the device only while the site list screen is active.

    NOTE: I am busy writing an installer for the DataSafePlus utility. I will post this asap.

    Please excuse my coding style, I am not a trained programmer. Any improvements/mods would be appreciated.
    
    


