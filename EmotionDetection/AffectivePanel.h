#pragma once

#include <QWidget>
#include <QTimer>
#include <QPropertyAnimation>
#include <deque>
#include <array>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

class AffectivePanel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double displayValence READ displayValence WRITE setDisplayValence)
    Q_PROPERTY(double displayArousal READ displayArousal WRITE setDisplayArousal)

public:
    explicit AffectivePanel(QWidget* parent = nullptr);

    void updateAffect(const double alphaPow[8], const double betaPow[8]);

    // Calibration control
    void startCalibration();   // begins 60 s baseline
    void stopCalibration();    // locks normalization params
    void startSession();
    void stopSession();

    bool isCalibrating()  const { return m_calibrating; }
    bool isCalibrated()   const { return m_calibrated; }

    // Q-property accessors
    double displayValence() const { return m_dispValence; }
    double displayArousal() const { return m_dispArousal; }
    void   setDisplayValence(double v) { m_dispValence = v; update(); }
    void   setDisplayArousal(double v) { m_dispArousal = v; update(); }

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private slots:
    void onCalibTick();

private:
    // Drawing helpers
    void drawCircumplex(QPainter& p, QRectF rect);
    void drawMetricBars(QPainter& p, QRectF rect);
    void drawBandTable (QPainter& p, QRectF rect);
    void drawStatusRow (QPainter& p, QRectF rect);
    void drawSectionLabel(QPainter& p, QRectF rect, const QString& text);

    QColor affectColor(double valence, double arousal) const;
    QString quadrantLabel(double valence, double arousal) const;

    // Min-max normalisation
    struct MinMax { double mn = 1e30, mx = -1e30; };
    static double applyMinMax(double v, const MinMax& mm);
    void   updateMinMax(double raw, MinMax& mm);

    // State
    double m_rawValence  = 0.0;
    double m_rawArousal  = 0.0;
    double m_normValence = 0.0;   // [-1, +1]
    double m_normArousal = 0.0;   // [-1, +1]

    // Animated display values
    double m_dispValence = 0.0;
    double m_dispArousal = 0.0;

    // Per-channel band powers (latest)
    double m_alpha[8]{};
    double m_beta [8]{};

    // Normalization bookkeeping
    MinMax m_valMM, m_aroMM;
    bool   m_calibrating = false;
    bool   m_calibrated  = false;
    int    m_calibSeconds = 0;
    static constexpr int kCalibDuration = 60;

    // History trail for the circumplex plot
    struct Point2D { double v, a; };
    std::deque<Point2D> m_trail;
    static constexpr int kTrailLen = 120;   // ~12 s of history @ ~10 Hz updates

    // Calibration timer
    QTimer* m_calibTimer = nullptr;

    // Smooth animation
    QPropertyAnimation* m_animV = nullptr;
    QPropertyAnimation* m_animA = nullptr;

    bool m_sessionRunning = false;
};
