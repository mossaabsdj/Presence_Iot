#ifndef DATA_H
#define DATA_H

#include <Arduino.h>

void dataInit();                 // load data from flash
int  getSall();                  // read value
void setSall(int newSall);        // update + save (server only)
// SESSION DELAY
unsigned long getSessionDelay();
void setSessionDelay(unsigned long newDelay);
void setServerIP(const String& newIP);
String getServerIP();
#endif