#include "Filtering.h"

#include "Filter.h"
#include <stdlib.h> 
#include <string.h>

#include <stdio.h>
extern "C"
{
#include "fidlib.h"
}

Filtering::Filtering(int sampleRate)
    :m_sampleRate(sampleRate)
{
}

void Filtering::initBuffers()
{
    for (int i = 0; i < MAX_INTERNAL_BUF; i++)
    {
        m_buf1[i] = 0;
        m_buf2[i] = 0;
    }
}

void Filtering::setBandPassFrequencyCorners(double freqCorner1, double freqCorner2)
{
    double coef[MAX_COEFS];
    char spec_d[6] = { 'B', 'p', 'B', 'u', '8', '\0' };

    coef[0] = fid_design_coef(coef + 1, MAX_INTERNAL_BUF, spec_d, m_sampleRate, freqCorner1, freqCorner2, 0);
    for (int i = 0; i < MAX_COEFS; i++)
    {
        m_coef[i] = coef[i];
    }
    initBuffers();
}

void Filtering::setBandStopFrequencyCorners(double freqCorner1, double freqCorner2)
{
    double coef[MAX_COEFS];
    char spec_d[6] = { 'B', 's', 'B', 'u', '8', '\0' };

    coef[0] = fid_design_coef(coef + 1, MAX_INTERNAL_BUF, spec_d, m_sampleRate, freqCorner1, freqCorner2, 0);
    for (int i = 0; i < MAX_COEFS; i++)
    {
        m_coef[i] = coef[i];
    }
    initBuffers();
}

inline double Filtering::processBandpass(double* coef, double* buf, double val)
{
    double tmp, fir, iir;
    tmp = buf[0]; memmove(buf, buf + 1, (MAX_INTERNAL_BUF - 1) * sizeof(double));
    iir = val * coef[0];
    iir -= coef[1] * tmp; fir = tmp;
    iir -= coef[2] * buf[0]; fir += -buf[0] - buf[0];
    fir += iir;
    tmp = buf[1]; buf[1] = iir; val = fir;
    iir = val;
    iir -= coef[3] * tmp; fir = tmp;
    iir -= coef[4] * buf[2]; fir += -buf[2] - buf[2];
    fir += iir;
    tmp = buf[3]; buf[3] = iir; val = fir;
    iir = val;
    iir -= coef[5] * tmp; fir = tmp;
    iir -= coef[6] * buf[4]; fir += -buf[4] - buf[4];
    fir += iir;
    tmp = buf[5]; buf[5] = iir; val = fir;
    iir = val;
    iir -= coef[7] * tmp; fir = tmp;
    iir -= coef[8] * buf[6]; fir += -buf[6] - buf[6];
    fir += iir;
    tmp = buf[7]; buf[7] = iir; val = fir;
    iir = val;
    iir -= coef[9] * tmp; fir = tmp;
    iir -= coef[10] * buf[8]; fir += buf[8] + buf[8];
    fir += iir;
    tmp = buf[9]; buf[9] = iir; val = fir;
    iir = val;
    iir -= coef[11] * tmp; fir = tmp;
    iir -= coef[12] * buf[10]; fir += buf[10] + buf[10];
    fir += iir;
    tmp = buf[11]; buf[11] = iir; val = fir;
    iir = val;
    iir -= coef[13] * tmp; fir = tmp;
    iir -= coef[14] * buf[12]; fir += buf[12] + buf[12];
    fir += iir;
    tmp = buf[13]; buf[13] = iir; val = fir;
    iir = val;
    iir -= coef[15] * tmp; fir = tmp;
    iir -= coef[16] * buf[14]; fir += buf[14] + buf[14];
    fir += iir;
    buf[15] = iir; val = fir;
    return val;
}

inline double Filtering::processBandstop(double* coef, double* buf, double val)
{
    double tmp, fir, iir;
    tmp = buf[0]; memmove(buf, buf + 1, (MAX_INTERNAL_BUF - 1) * sizeof(double));
    iir = val * coef[0];
    iir -= coef[1] * tmp; fir = tmp;
    iir -= coef[2] * buf[0]; fir += -buf[0] - buf[0];
    fir += iir;
    tmp = buf[1]; buf[1] = iir; val = fir;
    iir = val;
    iir -= coef[3] * tmp; fir = tmp;
    iir -= coef[4] * buf[2]; fir += -buf[2] - buf[2];
    fir += iir;
    tmp = buf[3]; buf[3] = iir; val = fir;
    iir = val;
    iir -= coef[5] * tmp; fir = tmp;
    iir -= coef[6] * buf[4]; fir += -buf[4] - buf[4];
    fir += iir;
    tmp = buf[5]; buf[5] = iir; val = fir;
    iir = val;
    iir -= coef[7] * tmp; fir = tmp;
    iir -= coef[8] * buf[6]; fir += -buf[6] - buf[6];
    fir += iir;
    tmp = buf[7]; buf[7] = iir; val = fir;
    iir = val;
    iir -= coef[9] * tmp; fir = tmp;
    iir -= coef[10] * buf[8]; fir += buf[8] + buf[8];
    fir += iir;
    tmp = buf[9]; buf[9] = iir; val = fir;
    iir = val;
    iir -= coef[11] * tmp; fir = tmp;
    iir -= coef[12] * buf[10]; fir += buf[10] + buf[10];
    fir += iir;
    tmp = buf[11]; buf[11] = iir; val = fir;
    iir = val;
    iir -= coef[13] * tmp; fir = tmp;
    iir -= coef[14] * buf[12]; fir += buf[12] + buf[12];
    fir += iir;
    tmp = buf[13]; buf[13] = iir; val = fir;
    iir = val;
    iir -= coef[15] * tmp; fir = tmp;
    iir -= coef[16] * buf[14]; fir += buf[14] + buf[14];
    fir += iir;
    buf[15] = iir; val = fir;
    return val;
}

void Filtering::processBP(const double* pIn, double* pOutput, const int iBufferSize)
{
    for (int i = 0; i < iBufferSize; i += 2)
    {
        pOutput[i] = processBandpass(m_coef, m_buf1, pIn[i]);
        pOutput[i + 1] = processBandpass(m_coef, m_buf2, pIn[i + 1]);
        if (pOutput[i] != pOutput[i])    //Check for NaN
            pOutput[i] = 0;
        if (pOutput[i + 1] != pOutput[i + 1])    //Check for NaN
            pOutput[i + 1] = 0;
    }
}

void Filtering::processBS(const double* pIn, double* pOutput, const int iBufferSize)
{
    for (int i = 0; i < iBufferSize; i += 2)
    {
        pOutput[i] = processBandstop(m_coef, m_buf1, pIn[i]);
        pOutput[i + 1] = processBandstop(m_coef, m_buf2, pIn[i + 1]);
        if (pOutput[i] != pOutput[i])    //Check for NaN
            pOutput[i] = 0;
        if (pOutput[i + 1] != pOutput[i + 1])    //Check for NaN
            pOutput[i + 1] = 0;
    }
}

