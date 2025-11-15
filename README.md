/*
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

*/
