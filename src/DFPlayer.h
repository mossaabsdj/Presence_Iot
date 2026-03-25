#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

// ===== Error codes returned by begin() =====
#define DFPLAYER_OK               0
#define DFPLAYER_ERROR_NO_RESPONSE  1   // Module didn't reply at all
#define DFPLAYER_ERROR_TIMEOUT      2   // Reply started but incomplete
#define DFPLAYER_ERROR_CHECKSUM     3   // Reply checksum mismatch
#define DFPLAYER_ERROR_BUSY         4   // Module replied but reported busy/error
#define DFPLAYER_ERROR_NO_SDCARD    5   // Module reported no SD card

class DFPlayer {
public:
    DFPlayer(HardwareSerial &serialPort);

    // Returns one of the DFPLAYER_* error codes above
    // Prints human-readable diagnostics to Serial
    int begin(HardwareSerial &serialPort, uint32_t baud = 9600);

    void play(uint16_t track);
    void pause();
    void resume();
    void stop();
    void next();
    void previous();
    void volume(uint8_t vol);

    // Returns last raw error code received from module (0 = none)
    uint8_t lastError() { return _lastError; }

private:
    HardwareSerial *serial;
    uint8_t _lastError = 0;

    void     sendCommand(uint8_t cmd, uint16_t param = 0);
    bool     readResponse(uint8_t *buf, uint16_t timeoutMs = 500);
    bool     verifyChecksum(uint8_t *buf);
    uint16_t calcChecksum(uint8_t *buf);
};