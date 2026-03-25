#include "DFPlayer.h"

// ─────────────────────────────────────────────
//  DFPlayer module response frame layout
//
//  Byte  0   : 0x7E  (start)
//  Byte  1   : 0xFF  (version)
//  Byte  2   : 0x06  (length)
//  Byte  3   : CMD   (command echo or 0x41 ACK / 0x40 error)
//  Byte  4   : 0x00  (feedback flag)
//  Byte  5   : param HIGH
//  Byte  6   : param LOW
//  Byte  7   : checksum HIGH
//  Byte  8   : checksum LOW
//  Byte  9   : 0xEF  (end)
// ─────────────────────────────────────────────

// Known error codes the module sends in param when CMD = 0x40
#define DFP_ERR_BUSY        0x01
#define DFP_ERR_SLEEPING    0x02
#define DFP_ERR_FRAME       0x03
#define DFP_ERR_CHECKSUM    0x04
#define DFP_ERR_TRACK_OOB   0x05
#define DFP_ERR_SDCARD      0x06  // No or unreadable SD card
#define DFP_ERR_ENTER_SLEEP 0x07
#define DFP_ERR_POWER       0x08  // Not commonly seen

// ─────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────
DFPlayer::DFPlayer(HardwareSerial &serialPort) {
    serial = &serialPort;
}

// ─────────────────────────────────────────────
//  begin() — initialise and validate the module
//
//  Steps:
//   1. Open serial port
//   2. Wait for module power-up (DFPlayer needs ~1.5 s)
//   3. Flush any junk bytes left in RX buffer
//   4. Send a "get status" query (CMD 0x42)
//   5. Read and validate the response frame
//   6. Decode any error the module reports
//
//  Returns: DFPLAYER_OK (0) or one of the error codes
// ─────────────────────────────────────────────
int DFPlayer::begin(HardwareSerial &serialPort, uint32_t baud) {
    serial = &serialPort;
    serial->begin(baud);

    // ── Step 1: give the module time to boot ──────────────────────────
    Serial.println("[DFPlayer] Waiting for module boot...");
    delay(1500);  // DFPlayer mini needs ~1-1.5 s after power-on

    // ── Step 2: flush garbage bytes that may have arrived during boot ──
    while (serial->available()) serial->read();

    // ── Step 3: send CMD 0x42 = "query current status" ────────────────
    //  This is the safest probe: it does not change playback state and
    //  always produces a response frame if the module is alive.
    Serial.println("[DFPlayer] Sending status query (0x42)...");
    sendCommand(0x42, 0);

    // ── Step 4: read the response ──────────────────────────────────────
    uint8_t buf[10] = {0};
    bool gotReply = readResponse(buf, 800); // 800 ms timeout

    if (!gotReply) {
        Serial.println("[DFPlayer] ERROR: No response from module.");
        Serial.println("           Check wiring: TX->RX, RX->TX, GND, 3.3 V/5 V.");
        Serial.println("           Check baud rate (default 9600).");
        return DFPLAYER_ERROR_NO_RESPONSE;
    }

    // ── Step 5: validate checksum ─────────────────────────────────────
    if (!verifyChecksum(buf)) {
        Serial.print("[DFPlayer] ERROR: Checksum mismatch. Received: ");
        for (int i = 0; i < 10; i++) {
            Serial.printf("0x%02X ", buf[i]);
        }
        Serial.println();
        return DFPLAYER_ERROR_CHECKSUM;
    }

    // ── Step 6: check if module sent an error frame (CMD = 0x40) ──────
    uint8_t  replyCmd   = buf[3];
    uint16_t replyParam = ((uint16_t)buf[5] << 8) | buf[6];

    if (replyCmd == 0x40) {
        _lastError = (uint8_t)replyParam;
        Serial.printf("[DFPlayer] ERROR: Module reported error code 0x%02X — ", _lastError);

        switch (_lastError) {
            case DFP_ERR_BUSY:
                Serial.println("Module busy (still initialising). Try increasing boot delay.");
                return DFPLAYER_ERROR_BUSY;

            case DFP_ERR_SLEEPING:
                Serial.println("Module is sleeping.");
                return DFPLAYER_ERROR_BUSY;

            case DFP_ERR_FRAME:
                Serial.println("Received malformed frame.");
                return DFPLAYER_ERROR_CHECKSUM;

            case DFP_ERR_CHECKSUM:
                Serial.println("Module detected checksum error in our command.");
                return DFPLAYER_ERROR_CHECKSUM;

            case DFP_ERR_SDCARD:
                Serial.println("No SD card detected or SD card unreadable.");
                Serial.println("           Insert a FAT32 SD card with audio files.");
                return DFPLAYER_ERROR_NO_SDCARD;

            default:
                Serial.printf("Unknown module error 0x%02X.\n", _lastError);
                return DFPLAYER_ERROR_BUSY;
        }
    }

    // ── Step 7: all good ───────────────────────────────────────────────
    Serial.printf("[DFPlayer] OK — module alive. Status param: 0x%04X\n", replyParam);
    return DFPLAYER_OK;
}

// ─────────────────────────────────────────────
//  readResponse()
//  Waits up to timeoutMs for a complete 10-byte frame.
//  Returns true if a full frame was received.
// ─────────────────────────────────────────────
bool DFPlayer::readResponse(uint8_t *buf, uint16_t timeoutMs) {
    unsigned long deadline = millis() + timeoutMs;
    int index = 0;

    while (millis() < deadline) {
        if (serial->available()) {
            uint8_t byte = serial->read();

            // Sync to start byte
            if (index == 0 && byte != 0x7E) continue;

            buf[index++] = byte;

            if (index == 10) {
                // Verify end byte
                if (buf[9] != 0xEF) {
                    Serial.println("[DFPlayer] WARNING: End byte not 0xEF — frame misaligned.");
                    index = 0; // try to re-sync
                    continue;
                }
                return true; // complete frame received
            }
        }
        delay(1);
    }

    if (index > 0 && index < 10) {
        Serial.printf("[DFPlayer] ERROR: Partial frame received (%d/10 bytes).\n", index);
        return false; // DFPLAYER_ERROR_TIMEOUT
    }

    return false; // DFPLAYER_ERROR_NO_RESPONSE
}

// ─────────────────────────────────────────────
//  calcChecksum() — two's complement of bytes 1..6
// ─────────────────────────────────────────────
uint16_t DFPlayer::calcChecksum(uint8_t *buf) {
    uint16_t sum = 0;
    for (int i = 1; i <= 6; i++) sum += buf[i];
    return (uint16_t)(0 - sum);
}

// ─────────────────────────────────────────────
//  verifyChecksum()
// ─────────────────────────────────────────────
bool DFPlayer::verifyChecksum(uint8_t *buf) {
    uint16_t expected = calcChecksum(buf);
    uint16_t received = ((uint16_t)buf[7] << 8) | buf[8];
    return expected == received;
}

// ─────────────────────────────────────────────
//  sendCommand()
// ─────────────────────────────────────────────
void DFPlayer::sendCommand(uint8_t cmd, uint16_t param) {
    uint8_t buffer[10];

    buffer[0] = 0x7E;
    buffer[1] = 0xFF;
    buffer[2] = 0x06;
    buffer[3] = cmd;
    buffer[4] = 0x00;               // no feedback requested
    buffer[5] = (param >> 8) & 0xFF;
    buffer[6] = param & 0xFF;

    uint16_t checksum = calcChecksum(buffer);
    buffer[7] = (checksum >> 8) & 0xFF;
    buffer[8] = checksum & 0xFF;
    buffer[9] = 0xEF;

    for (int i = 0; i < 10; i++) serial->write(buffer[i]);
}

// ─────────────────────────────────────────────
//  Playback controls
// ─────────────────────────────────────────────
void DFPlayer::play(uint16_t track)  { sendCommand(0x03, track); }
void DFPlayer::pause()               { sendCommand(0x0E, 0); }
void DFPlayer::resume()              { sendCommand(0x0D, 0); }
void DFPlayer::stop()                { sendCommand(0x16, 0); }
void DFPlayer::next()                { sendCommand(0x01, 0); }
void DFPlayer::previous()            { sendCommand(0x02, 0); }

void DFPlayer::volume(uint8_t vol) {
    if (vol > 30) vol = 30;
    sendCommand(0x06, vol);
}