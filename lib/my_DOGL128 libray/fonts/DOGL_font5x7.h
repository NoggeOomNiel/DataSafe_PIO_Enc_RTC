/*
 * font5x7.h
 *
 * Created: 28/03/2012 1:52:20 AM
 *  Author: andy
 */ 

// Title		: Graphic LCD Font (Ascii Charaters)
// Author		: Pascal Stang

#ifndef DOGL_FONT5X7_H_
#define DOGL_FONT5X7_H_

#include <Arduino.h>

// standard ascii 5x7 font
// defines ascii characters 0x20-0x7F (32-127)
const unsigned char DOGL_Font5x7[] PROGMEM = {
	5,7,5,2,         // Length,horz,vert,byte/vert    

};




#endif /* DOGL_FONT5X7_H_ */
