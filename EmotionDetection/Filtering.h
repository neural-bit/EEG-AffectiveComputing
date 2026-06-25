#ifndef FILTER_H
#define FILTER_H

#define MAX_COEFS  17
#define MAX_INTERNAL_BUF 16

class Filtering
{
public:
    Filtering(int sampleRate);


    void setBandPassFrequencyCorners(double freqCorner1, double freqCorner2);
    void setBandStopFrequencyCorners(double freqCorner1, double freqCorner2);
    void processBP(const double* pIn, double* pOut, const int iBufferSize);
    void processBS(const double* pIn, double* pOut, const int iBufferSize);

private:
    void initBuffers();
    inline double processBandpass(double* coef, double* buf, double val);
    inline double processBandstop(double* coef, double* buf, double val);

protected:
    int m_sampleRate;
    double m_coef[MAX_COEFS];
    int m_bufSize;
    double m_buf1[MAX_INTERNAL_BUF];
    double m_buf2[MAX_INTERNAL_BUF];

};

#endif
