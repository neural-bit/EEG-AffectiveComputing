#pragma once
#include <vector>
#include <cstddef>

// Welch's method PSD estimator
// Uses overlapping Hann-windowed segments, averaged periodogram
class WelchPSD
{
public:

    WelchPSD(int sampleRate, int windowSamples, int hopSamples);

    // Push one new sample; returns true when a fresh PSD estimate is ready
    bool pushSample(double x);

    // After pushSample returns true, query band power [Hz]
    double bandPower(double fLow, double fHigh) const;

    // Frequency resolution in Hz
    double freqResolution() const { return static_cast<double>(m_sampleRate) / m_winLen; }

    // Number of PSD bins (winLen/2 + 1)
    int numBins() const { return m_numBins; }

private:
    void   computePSD();
    double hann(int n, int N) const;

    int m_sampleRate;
    int m_winLen;
    int m_hopSamples;
    int m_numBins;

    std::vector<double> m_ringBuf;    // circular sample buffer (length = winLen)
    int m_writePos;
    int m_hopCounter; // samples since last PSD update

    std::vector<double> m_window;     // Hann window
    std::vector<double> m_psd;        // current PSD estimate (power/Hz)

    // Scratch FFT buffers (real/imag)
    std::vector<double> m_fftRe;
    std::vector<double> m_fftIm;

    void fftReal(std::vector<double>& re, std::vector<double>& im); // in-place radix-2 DIT FFT
};
