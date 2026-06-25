#ifndef SCOPEWIDGET_H
#define SCOPEWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <vector>

#define N_CHANNELS          8
#define SR                  250 // sampling rate
#define SAMPLE_IN_SECONDS   10
#define N_SAMPLES           (SAMPLE_IN_SECONDS * SR)

// EEG channel names (10-20 system)
static const char* const kChannelNames[N_CHANNELS] = {
    "Fp1", "Fp2", "F3", "F4", "T3", "T4", "P3", "P4"
};

// Channel indices for easy reference
enum EEGChannel {
    CH_Fp1 = 0,
    CH_Fp2 = 1,
    CH_F3  = 2,
    CH_F4  = 3,
    CH_T3  = 4,
    CH_T4  = 5,
    CH_P3  = 6,
    CH_P4  = 7
};

// Margins
static constexpr int kLeftMargin   = 50;   // px – channel number labels
static constexpr int kBottomMargin = 18;   // px – time axis labels

// Pixels per division for waveform rendering
static constexpr int kPixelsPerDiv = 20;


class ScopeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScopeWidget(QWidget* parent = nullptr);
    void addSample(const double* data, int nChannels);
    void   setUvPerDiv(double uvPerDiv);
    double uvPerDiv() const { return m_uvPerDiv; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawSample(const std::vector<int>& sample);

    int plotWidth()  const { return width() - kLeftMargin; }
    int plotHeight() const { return height() - kBottomMargin; }

    // µV -> pixels conversion derived from the current scale setting
    double uvPerPixel() const { return m_uvPerDiv / kPixelsPerDiv; }

    QString formatTime(qint64 totalSamples) const;

    QColor  m_colors[N_CHANNELS];
    std::vector<std::vector<int>> m_channelData;   // stored in display pixels

    QPixmap m_tracePixmap;
    int     m_currentX = 0;
    float   m_currentXL = 0.f;
    int     m_sampleCount = 0;
    qint64  m_totalSamples = 0;

    // Vertical scale in µV per division
    double  m_uvPerDiv = 100.0;
};

#endif // SCOPEWIDGET_H
