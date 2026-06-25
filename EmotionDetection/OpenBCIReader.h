#pragma once

#include <windows.h>
#include <string>
#include <stdexcept>
#include <cstdint>

// ─────────────────────────────────────────────
//  OpenBCI Cyton – 8-channel serial reader
//
//  Usage:
//    OpenBCIReader reader("COM3");
//    reader.startStream();
//
//    // your thread loop:
//    double data[8];
//    while (running)
//        if (reader.readData(data))
//            processData(data);
//
//    reader.stopStream();
//
//  Protocol reference:
//  https://docs.openbci.com/Cyton/CytonDataFormat/
//
//  Packet layout (33 bytes):
//   [0]      0xA0        – start byte
//   [1]      sample #    – 0–255, wraps
//   [2..25]  3 bytes × 8 – EEG channels (24-bit signed, big-endian)
//   [26..31] 6 bytes     – AUX (accelerometer)
//   [32]     0xC0/0xC1   – stop byte
// ─────────────────────────────────────────────

class OpenBCIReader
{
public:
    static constexpr int    NUM_CHANNELS = 8;
    static constexpr int    PACKET_SIZE = 33;
    static constexpr double SCALE_UV = 4.5 / 24.0 / (1 << 23); // ~0.02235 µV/LSB

    explicit OpenBCIReader(const std::string& portName);
    ~OpenBCIReader();

    // Sends 'b' to the board and flushes the RX buffer.
    void startStreaming();

    // Sends 's' to the board.
    void stopStreaming();

    // Blocks until one valid 33-byte packet arrives.
    // Fills data[0..7] with EEG voltages in µV.
    // Returns true on success, false on serial error or timeout.
    bool readData(double* data);

private:
    bool    readExact(uint8_t* buf, DWORD count);
    void    sendCommand(char cmd);

    static void    decodePacket(const uint8_t* raw, double* out);
    static int32_t interpret24bitAsInt32(const uint8_t* bytes);

    std::string m_portName;
    HANDLE      m_hSerial{ INVALID_HANDLE_VALUE };
};
