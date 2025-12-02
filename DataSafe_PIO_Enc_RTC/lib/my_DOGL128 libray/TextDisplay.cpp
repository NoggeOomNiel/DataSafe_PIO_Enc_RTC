/* mbed TextDisplay Display Library Base Class
 * Copyright (c) 2007-2009 sford
 * Released under the MIT License: http://mbed.org/license/mit
 */

#include "TextDisplay.h"

TextDisplay::TextDisplay(const char *name) : Stream() { // Corrected base class constructor call
    _row = 0;
    _column = 0;
    if (name == NULL) {
        _path = NULL;
    } else {
        _path = new char[strlen(name) + 2];
        sprintf(_path, "/%s", name);
    }
}

TextDisplay::~TextDisplay() {
    if (_path != NULL) {
        delete[] _path;
        _path = NULL;
    }
}

int TextDisplay::_putc(int value) {
    if(value == '\n') {
        _column = 0;
        _row++;
        if(_row >= rows()) {
            _row = 0;
        }
    } else {
        character(_column, _row, value);
        _column++;
        if(_column >= columns()) {
            _column = 0;
            _row++;
            if(_row >= rows()) {
                _row = 0;
            }
        }
    }
    return value;
}

// crude cls implementation, should generally be overwritten in derived class
void TextDisplay::cls() {
    locate(0, 0);
    for(int i=0; i<columns()*rows(); i++) {
        _putc(' ');
    }
}

void TextDisplay::locate(int column, int row) {
    _column = column;
    _row = row;
}

int TextDisplay::_getc() {
    return -1;
}
        
void TextDisplay::foreground(uint16_t colour) {
    _foreground = colour;
}

void TextDisplay::background(uint16_t colour) {
    _background = colour;
}

bool TextDisplay::claim (FILE *stream) {
    if ( _path == NULL) {
        fprintf(stderr, "claim requires a name to be given in the instantioator of the TextDisplay instance!\r\n");
        return false;
    }
    if (freopen(_path, "w", stream) == NULL) {
        // Failed, should not happen
        return false;
    }
    // make sure we use line buffering
    setvbuf(stdout, NULL, _IOLBF, columns());
    return true;
}

// Implementations for Arduino Stream/Print pure virtuals
size_t TextDisplay::write(uint8_t value) {
    if (_putc(static_cast<int>(value)) == EOF) { // Check for EOF as an error indicator
        setWriteError(); // Notify Print base class of an error
        return 0;
    }
    return 1; // Successfully wrote 1 byte
}

int TextDisplay::available() {
    // For a display, typically no data is "available" for reading from it.
    return 0;
}

int TextDisplay::read() {
    // _getc() is implemented to return -1, indicating no data or end of stream.
    return _getc();
}

int TextDisplay::peek() {
    // If read() consumes data and there's no internal buffer for peeking,
    // returning -1 is standard for "no character available for peeking".
    return -1;
}
