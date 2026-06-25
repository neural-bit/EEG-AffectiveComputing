#include "ScopeWidget.h"
#include <QPainter>
#include <cmath>


static const QColor kChannelColors[N_CHANNELS] =
{
    QColor(0,   255, 255),   // Fp1 – cyan
    QColor(255, 255,   0),   // Fp2 – yellow
    QColor(0,   255,   0),   // F3 – green
    QColor(255, 128,   0),   // F4 – orange
    QColor(255,   0, 255),   // T3  – magenta
    QColor(100, 180, 255),   // T4  – sky blue
    QColor(255,  80,  80),   // P3  – red
    QColor(180, 255, 128),   // P4  – lime
};

QString ScopeWidget::formatTime(qint64 totalSamples) const
{
    const qint64 totalSec = totalSamples / SR;
    const int    hh = static_cast<int>(totalSec / 3600);
    const int    mm = static_cast<int>((totalSec % 3600) / 60);
    const int    ss = static_cast<int>(totalSec % 60);

    if (hh > 0)
        return QString("%1:%2:%3")
        .arg(hh, 2, 10, QLatin1Char('0'))
        .arg(mm, 2, 10, QLatin1Char('0'))
        .arg(ss, 2, 10, QLatin1Char('0'));
    else
        return QString("%1:%2")
        .arg(mm, 2, 10, QLatin1Char('0'))
        .arg(ss, 2, 10, QLatin1Char('0'));
}

ScopeWidget::ScopeWidget(QWidget* parent) : QWidget(parent)
{
    for (int i = 0; i < N_CHANNELS; ++i)
        m_colors[i] = kChannelColors[i];

    m_channelData.resize(N_CHANNELS);
    for (int i = 0; i < N_CHANNELS; ++i)
        m_channelData[i].resize(N_SAMPLES, 0);

    m_tracePixmap = QPixmap(plotWidth(), plotHeight());
    m_tracePixmap.fill(Qt::black);
}

void ScopeWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_tracePixmap = QPixmap(plotWidth(), plotHeight());
    m_tracePixmap.fill(Qt::black);
    for (int i = 0; i < N_CHANNELS; ++i)
        m_channelData[i].assign(N_SAMPLES, 0);
    m_sampleCount = 0;
    m_currentX = 0;
}

void ScopeWidget::setUvPerDiv(double uvPerDiv)
{
    m_uvPerDiv = uvPerDiv;
    m_tracePixmap.fill(Qt::black);
    update();
}

void ScopeWidget::addSample(const double* data, int nChannels)
{
    const double scale = uvPerPixel();
    std::vector<int> intSample(N_CHANNELS, 0);
    const int n = std::min(nChannels, N_CHANNELS);
    for (int ch = 0; ch < n; ++ch)
        intSample[ch] = static_cast<int>(std::round(data[ch] / scale));
    drawSample(intSample);
}

void ScopeWidget::drawSample(const std::vector<int>& sample)
{
    const int    ph = plotHeight();
    const int    pw = plotWidth();
    const int    heightSegment = ph / N_CHANNELS;
    const double boxWidth = static_cast<double>(pw) / N_SAMPLES;
    const int    prevX = (m_currentX - 1 + N_SAMPLES) % N_SAMPLES;

    QPainter painter(&m_tracePixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Erase the column just ahead of the write head (sweep-line effect)
    m_currentXL = static_cast<float>(m_currentX * boxWidth);
    painter.setPen(Qt::black);
    painter.drawLine(static_cast<int>(m_currentXL) + 1, 0,
        static_cast<int>(m_currentXL) + 1, ph);

    // Draw segment from previous sample to current sample
    if (m_sampleCount >= 1 && (m_currentX != 0 || m_sampleCount < N_SAMPLES))
    {
        for (int ch = 0; ch < N_CHANNELS; ++ch)
        {
            const int midY = heightSegment * ch + heightSegment / 2;
            const int prevY = midY - m_channelData[ch][prevX];
            const int currY = midY - sample[ch];

            const float pX = static_cast<float>(prevX * boxWidth);
            const float cX = static_cast<float>(m_currentX * boxWidth);

            painter.setPen(m_colors[ch]);
            painter.drawLine(QPointF(pX, prevY), QPointF(cX, currY));
        }
    }

    // Grid tick on the centre-line
    {
        QPen gridPen(QColor(40, 40, 40));
        painter.setPen(gridPen);
        for (int ch = 0; ch < N_CHANNELS; ++ch)
        {
            const int midY = heightSegment * ch + heightSegment / 2;
            painter.drawPoint(static_cast<int>(m_currentXL), midY);
        }
    }

    // Store
    for (int ch = 0; ch < N_CHANNELS; ++ch)
        m_channelData[ch][m_currentX] = sample[ch];

    m_currentX = (m_currentX + 1) % N_SAMPLES;
    ++m_sampleCount;
    ++m_totalSamples;

    update();
}

void ScopeWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);

    // Fill entire widget black
    painter.fillRect(rect(), Qt::black);

    painter.drawPixmap(kLeftMargin, 0, m_tracePixmap);

    // Moving cursor
    {
        const double boxWidth = static_cast<double>(plotWidth()) / N_SAMPLES;
        m_currentXL = static_cast<float>(m_currentX * boxWidth);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawLine(kLeftMargin + static_cast<int>(m_currentXL) + 1, 0,
            kLeftMargin + static_cast<int>(m_currentXL) + 1, plotHeight());
    }

    // Channel labels (using 10-20 names)
    {
        const int heightSegment = plotHeight() / N_CHANNELS;
        QFont font = painter.font();
        font.setPointSize(9);
        font.setBold(true);
        painter.setFont(font);

        for (int ch = 0; ch < N_CHANNELS; ++ch)
        {
            const int midY = ch * heightSegment + heightSegment / 2;
            painter.setPen(m_colors[ch]);
            const QRect labelRect(0, midY - 8, kLeftMargin - 3, 16);
            painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
                             QString(kChannelNames[ch]));
        }
    }

    // Time axis
    {
        QFont font = painter.font();
        font.setPointSize(7);
        font.setBold(false);
        painter.setFont(font);

        const int    pw = plotWidth();
        const double boxWidth = static_cast<double>(pw) / N_SAMPLES;

        const qint64 sweepBase = (m_totalSamples / N_SAMPLES) * static_cast<qint64>(N_SAMPLES);

        auto absSampleForCol = [&](int col) -> qint64 {
            if (m_totalSamples <= N_SAMPLES)
                return static_cast<qint64>(col);
            if (col < m_currentX)
                return sweepBase + col;
            else
                return sweepBase - N_SAMPLES + col;
            };

        int tickEvery = SR;
        if (pw / SAMPLE_IN_SECONDS < 40) tickEvery = SR * 2;
        if (pw / SAMPLE_IN_SECONDS < 20) tickEvery = SR * 5;

        const int yBase = plotHeight() + 2;

        painter.setPen(QColor(160, 160, 160));
        for (int col = 0; col < N_SAMPLES; ++col)
        {
            const qint64 abs = absSampleForCol(col);
            if (abs % tickEvery != 0) continue;

            const int xPos = kLeftMargin + static_cast<int>(col * boxWidth);
            painter.drawLine(xPos, plotHeight() - 3, xPos, plotHeight() + 2);

            const QString label = formatTime(abs);
            const QRect textRect(xPos - 22, yBase + 1, 44, kBottomMargin - 2);
            painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, label);
        }

        // Separator line
        painter.setPen(QColor(60, 60, 60));
        painter.drawLine(kLeftMargin, plotHeight(), width(), plotHeight());
    }
}
