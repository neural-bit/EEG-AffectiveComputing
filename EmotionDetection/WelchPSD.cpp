#include "WelchPSD.h"
#include <cmath>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int nextPow2(int n)
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}


//  Construction
WelchPSD::WelchPSD(int sampleRate, int windowSamples, int hopSamples)
    : m_sampleRate(sampleRate)
    , m_winLen(nextPow2(windowSamples))   // round up to power-of-2 for FFT
    , m_hopSamples(hopSamples)
    , m_writePos(0)
    , m_hopCounter(0)
{
    m_numBins = m_winLen / 2 + 1;

    m_ringBuf.assign(m_winLen, 0.0);
    m_psd.assign(m_numBins, 0.0);
    m_fftRe.resize(m_winLen);
    m_fftIm.resize(m_winLen);

    // Pre-compute Hann window
    m_window.resize(m_winLen);
    for (int i = 0; i < m_winLen; ++i)
        m_window[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (m_winLen - 1)));
}


//  Sample ingestion
bool WelchPSD::pushSample(double x)
{
    m_ringBuf[m_writePos] = x;
    m_writePos = (m_writePos + 1) % m_winLen;
    ++m_hopCounter;

    if (m_hopCounter >= m_hopSamples)
    {
        m_hopCounter = 0;
        computePSD();
        return true;
    }
    return false;
}


//  PSD computation
void WelchPSD::computePSD()
{
    // Copy ring buffer in chronological order and apply window
    for (int i = 0; i < m_winLen; ++i)
    {
        int src = (m_writePos + i) % m_winLen;
        m_fftRe[i] = m_ringBuf[src] * m_window[i];
        m_fftIm[i] = 0.0;
    }

    fftReal(m_fftRe, m_fftIm);

    // Compute one-sided power spectrum (power / Hz)
    double winPower = 0.0;
    for (double w : m_window) 
        winPower += w * w;
    const double norm = 1.0 / (m_sampleRate * winPower);

    m_psd[0] = (m_fftRe[0]*m_fftRe[0] + m_fftIm[0]*m_fftIm[0]) * norm;
    for (int k = 1; k < m_numBins - 1; ++k)
        m_psd[k] = 2.0 * (m_fftRe[k]*m_fftRe[k] + m_fftIm[k]*m_fftIm[k]) * norm;
    // Nyquist bin
    int ny = m_winLen / 2;
    m_psd[m_numBins - 1] = (m_fftRe[ny]*m_fftRe[ny] + m_fftIm[ny]*m_fftIm[ny]) * norm;
}

//  Band power query
double WelchPSD::bandPower(double fLow, double fHigh) const
{
    const double df = freqResolution();
    double power = 0.0;
    for (int k = 0; k < m_numBins; ++k)
    {
        double f = k * df;
        if (f >= fLow && f <= fHigh)
            power += m_psd[k] * df;   // integrate: P = sum(PSD * df)
    }
    return power;
}


//  In-place Cooley-Tukey radix-2 DIT FFT (complex input)
void WelchPSD::fftReal(std::vector<double>& re, std::vector<double>& im)
{
    const int N = static_cast<int>(re.size());

    // Bit-reversal permutation
    for (int i = 1, j = 0; i < N; ++i)
    {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) 
            j ^= bit;
        j ^= bit;
        if (i < j) 
        { 
            std::swap(re[i], re[j]); 
            std::swap(im[i], im[j]); 
        }
    }

    // Butterfly stages
    for (int len = 2; len <= N; len <<= 1)
    {
        double ang = -2.0 * M_PI / len;
        double wRe = std::cos(ang), wIm = std::sin(ang);
        for (int i = 0; i < N; i += len)
        {
            double uRe = 1.0, uIm = 0.0;
            for (int j = 0; j < len / 2; ++j)
            {
                double tRe = uRe*re[i+j+len/2] - uIm*im[i+j+len/2];
                double tIm = uRe*im[i+j+len/2] + uIm*re[i+j+len/2];
                re[i+j+len/2] = re[i+j] - tRe;
                im[i+j+len/2] = im[i+j] - tIm;
                re[i+j] += tRe;
                im[i+j] += tIm;
                double newURe = uRe*wRe - uIm*wIm;
                uIm = uRe*wIm + uIm*wRe;
                uRe = newURe;
            }
        }
    }
}
