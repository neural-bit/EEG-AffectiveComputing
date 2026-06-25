#include "OpenBCIReader.h"

#include <iostream>

static std::string lastWinError()
{
    DWORD err = ::GetLastError();
    char  buf[256]{};
    ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, 0, buf, sizeof(buf), nullptr);
    return buf;
}

OpenBCIReader::OpenBCIReader(const std::string& portName)
    : m_portName(portName)
{
    // Ports above COM9 need the "\\.\" prefix on Windows
    std::string fullPort = (portName.rfind("\\\\.\\", 0) == 0)
        ? portName
        : "\\\\.\\" + portName;

    m_hSerial = ::CreateFileA(fullPort.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (m_hSerial == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot open port " + portName + ": " + lastWinError());

    // Serial port settings
    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!::GetCommState(m_hSerial, &dcb))
        throw std::runtime_error("GetCommState failed: " + lastWinError());

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;

    if (!::SetCommState(m_hSerial, &dcb))
        throw std::runtime_error("SetCommState failed: " + lastWinError());

    // Timeouts: ReadFile blocks up to 2 s per call
    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout = 0;
    to.ReadTotalTimeoutMultiplier = 2;
    to.ReadTotalTimeoutConstant = 2000;
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = 1000;
    ::SetCommTimeouts(m_hSerial, &to);

    ::PurgeComm(m_hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

OpenBCIReader::~OpenBCIReader()
{
    if (m_hSerial != INVALID_HANDLE_VALUE)
        ::CloseHandle(m_hSerial);
}

void OpenBCIReader::startStreaming()
{
    ::PurgeComm(m_hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
    sendCommand('b');
}

void OpenBCIReader::stopStreaming()
{
    sendCommand('s');
}

bool OpenBCIReader::readData(double* data)
{
    uint8_t byte{};

    // Sync to start byte 0xA0
    for (;;)
    {
        if (!readExact(&byte, 1))
            return false;   // serial error or timeout

        if (byte == 0xA0)
            break;          // found the start byte

        // Stray byte – discard and keep scanning
        std::cerr << "[OpenBCIReader] Skipping stray byte: 0x"
            << std::hex << (int)byte << std::dec << "\n";
    }

    // Read the remaining 32 bytes
    uint8_t raw[PACKET_SIZE]{};
    raw[0] = 0xA0;

    if (!readExact(raw + 1, PACKET_SIZE - 1))
        return false;

    // Validate stop byte
    uint8_t stopByte = raw[PACKET_SIZE - 1];
    if (stopByte != 0xC0 && stopByte != 0xC1)
    {
        std::cerr << "[OpenBCIReader] Bad stop byte: 0x"
            << std::hex << (int)stopByte << std::dec << "\n";
        // Don't return false – caller can retry; we just report and let them loop
        return false;
    }

    // Decode to µV
    decodePacket(raw, data);
    return true;
}

bool OpenBCIReader::readExact(uint8_t* buf, DWORD count)
{
    DWORD total = 0;
    while (total < count)
    {
        DWORD got = 0;
        if (!::ReadFile(m_hSerial, buf + total, count - total, &got, nullptr))
        {
            std::cerr << "[OpenBCIReader] ReadFile error: " << lastWinError() << "\n";
            return false;
        }
        if (got == 0)
            return false;   // timeout with no data
        total += got;
    }
    return true;
}

void OpenBCIReader::sendCommand(char cmd)
{
    DWORD written = 0;
    ::WriteFile(m_hSerial, &cmd, 1, &written, nullptr);
}

void OpenBCIReader::decodePacket(const uint8_t* raw, double* out)
{
    // EEG bytes start at index 2, 3 bytes per channel, big-endian signed
    for (int ch = 0; ch < NUM_CHANNELS; ++ch)
    {
        const uint8_t* p = raw + 2 + ch * 3;
        out[ch] = interpret24bitAsInt32(p) * SCALE_UV;
    }
}

int32_t OpenBCIReader::interpret24bitAsInt32(const uint8_t* bytes)
{
    int32_t value = (static_cast<int32_t>(bytes[0]) << 16)
        | (static_cast<int32_t>(bytes[1]) << 8)
        | static_cast<int32_t>(bytes[2]);

    // Sign-extend bit 23 into the upper byte
    if (value & 0x800000)
        value |= 0xFF000000;

    return value;
}
