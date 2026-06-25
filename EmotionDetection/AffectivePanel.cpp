#include "AffectivePanel.h"

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Palette
static const QColor kBg          {  8,  10,  18};
static const QColor kCard        { 16,  20,  34};
static const QColor kCardBorder  { 32,  40,  62};
static const QColor kText        {210, 220, 240};
static const QColor kSubText     {100, 115, 155};
static const QColor kGridLine    { 28,  35,  55};

// Quadrant tint colours (HVAC-style)
static const QColor kHappy   { 50, 220, 120, 90};   // +V +A  excited/happy
static const QColor kRelaxed {  0, 180, 255, 70};   // +V −A  calm/content
static const QColor kAngry   {255,  75,  75, 70};   // −V +A  tense/angry
static const QColor kSad     {120,  80, 200, 70};   // −V −A  sad/depressed

static const QColor kAccentCyan  { 30, 230, 200};
static const QColor kAccentBlue  {  0, 160, 255};
static const QColor kWarn        {255, 200,  50};
static const QColor kAlert       {255,  75,  75};
static const QColor kFocusGreen  { 50, 220, 120};


AffectivePanel::AffectivePanel(QWidget* parent) : QWidget(parent)
{
    setMinimumWidth(340);

    m_calibTimer = new QTimer(this);
    m_calibTimer->setInterval(1000);
    connect(m_calibTimer, &QTimer::timeout, this, &AffectivePanel::onCalibTick);

    m_animV = new QPropertyAnimation(this, "displayValence", this);
    m_animV->setDuration(250);
    m_animV->setEasingCurve(QEasingCurve::OutCubic);

    m_animA = new QPropertyAnimation(this, "displayArousal", this);
    m_animA->setDuration(250);
    m_animA->setEasingCurve(QEasingCurve::OutCubic);
}

void AffectivePanel::updateAffect(const double alphaPow[8], const double betaPow[8])
{
    for (int i = 0; i < 8; ++i) 
    { 
        m_alpha[i] = alphaPow[i]; 
        m_beta[i] = betaPow[i]; 
    }

    // Valence  (Frontal Alpha Asymmetry)
    // "Fp1", "Fp2", "F3", "F4", "T3", "T4", "P3", "P4"
    // 
    // Right frontal: F4 (idx 3), Fp2 (idx 1)
    // Left  frontal: F3 (idx 2), Fp1 (idx 0)
    const double eps = 1e-30;
    double alphaRight = (m_alpha[3] + m_alpha[1]) * 0.5 + eps;
    double alphaLeft  = (m_alpha[2] + m_alpha[0]) * 0.5 + eps;
    m_rawValence = std::log(alphaRight) - std::log(alphaLeft);

    // Arousal  (Beta/Alpha across T3,T4,P3,P4)
    // T3=4, T4=5, P3=6, P4=7
    double arSum = 0.0;
    int    arN   = 0;
    for (int idx : {4, 5, 6, 7})
    {
        double al = m_alpha[idx] + eps;
        double be = m_beta [idx] + eps;
        arSum += be / al;
        ++arN;
    }
    m_rawArousal = arSum / arN;

    // ── Normalisation ─────────────────────────────────────────────────────────
    if (m_calibrating)
    {
        updateMinMax(m_rawValence, m_valMM);
        updateMinMax(m_rawArousal, m_aroMM);
    }

    if (m_calibrated)
    {
        // Map to [−1, +1]:  min→−1, max→+1
        double vn = applyMinMax(m_rawValence, m_valMM) * 2.0 - 1.0;
        double an = applyMinMax(m_rawArousal, m_aroMM) * 2.0 - 1.0;
        m_normValence = std::clamp(vn, -1.0, 1.0);
        m_normArousal = std::clamp(an, -1.0, 1.0);
    }
    else
    {
        // Before calibration: soft-clamp raw values to rough display range
        m_normValence = std::clamp(m_rawValence / 2.0, -1.0, 1.0);
        m_normArousal = std::clamp((m_rawArousal - 1.0) / 4.0, -1.0, 1.0);
    }

    // Trail
    m_trail.push_back({m_normValence, m_normArousal});
    if (static_cast<int>(m_trail.size()) > kTrailLen)
        m_trail.pop_front();

    // Animate
    m_animV->stop(); m_animV->setStartValue(m_dispValence); m_animV->setEndValue(m_normValence); m_animV->start();
    m_animA->stop(); m_animA->setStartValue(m_dispArousal); m_animA->setEndValue(m_normArousal); m_animA->start();
}

void AffectivePanel::startCalibration()
{
    m_valMM = {}; m_aroMM = {};
    m_calibSeconds = 0;
    m_calibrating  = true;
    m_calibrated   = false;
    m_calibTimer->start();
    update();
}

void AffectivePanel::stopCalibration()
{
    m_calibrating = false;
    m_calibrated  = true;
    m_calibTimer->stop();
    update();
}

void AffectivePanel::startSession()
{
    m_trail.clear();
    m_sessionRunning = true;
    update();
}

void AffectivePanel::stopSession()
{
    m_sessionRunning = false;
    update();
}

void AffectivePanel::onCalibTick()
{
    ++m_calibSeconds;
    if (m_calibSeconds >= kCalibDuration)
        stopCalibration();
    update();
}

void AffectivePanel::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
}

double AffectivePanel::applyMinMax(double v, const MinMax& mm)
{
    if (mm.mx <= mm.mn) 
        return 0.5;
    return std::clamp((v - mm.mn) / (mm.mx - mm.mn), 0.0, 1.0);
}

void AffectivePanel::updateMinMax(double v, MinMax& mm)
{
    mm.mn = std::min(mm.mn, v);
    mm.mx = std::max(mm.mx, v);
}

QColor AffectivePanel::affectColor(double valence, double arousal) const
{
    // Blend between four quadrant colours
    double vn = (valence + 1.0) * 0.5;   // 0..1
    double an = (arousal + 1.0) * 0.5;   // 0..1

    auto blend = [](QColor a, QColor b, double t) 
        {
        return QColor(
            static_cast<int>(a.red()   + t * (b.red()   - a.red())),
            static_cast<int>(a.green() + t * (b.green() - a.green())),
            static_cast<int>(a.blue()  + t * (b.blue()  - a.blue()))
        );
    };

    // Bottom row (low arousal): sad → relaxed
    QColor bottom = blend(QColor(120, 80, 200), QColor(0, 180, 255), vn);
    // Top row (high arousal):   angry → happy
    QColor top    = blend(QColor(255, 75,  75), QColor(50, 220, 120), vn);
    return blend(bottom, top, an);
}

QString AffectivePanel::quadrantLabel(double valence, double arousal) const
{
    if (valence >= 0 && arousal >= 0)  return "Excited / Happy";
    if (valence >= 0 && arousal <  0)  return "Calm / Content";
    if (valence <  0 && arousal >= 0)  return "Tense / Angry";
    return "Sad / Depressed";
}

void AffectivePanel::drawSectionLabel(QPainter& p, QRectF rect, const QString& text)
{
    QFont f("Segoe UI", 7, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    p.setFont(f);
    p.setPen(kSubText);
    p.drawText(rect, Qt::AlignLeft | Qt::AlignTop, text.toUpper());
}

void AffectivePanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    p.fillRect(rect(), kBg);

    const int W   = width();
    const int H   = height();
    const int pad = 10;

    // Distribute vertical space
    const int circH  = std::min(W - 2*pad, H / 2);      // square circumplex
    const int barH   = 72;
    const int tableH = 80;
    const int statH  = 44;

    int y = pad;

    // Status row
    QRectF statCard(pad, y, W - 2*pad, statH);
    {
        p.save();
        p.setPen(QPen(kCardBorder, 1));
        p.setBrush(kCard);
        p.drawRoundedRect(statCard, 10, 10);
        p.restore();
    }
    drawStatusRow(p, statCard);
    y += statH + pad;

    // Circumplex
    QRectF circCard(pad, y, W - 2*pad, circH);
    {
        p.save();
        p.setPen(QPen(kCardBorder, 1));
        p.setBrush(kCard);
        p.drawRoundedRect(circCard, 14, 14);
        p.restore();
    }
    drawCircumplex(p, circCard);
    y += circH + pad;

    // Band power bars
    QRectF barCard(pad, y, W - 2*pad, barH);
    {
        p.save();
        p.setPen(QPen(kCardBorder, 1));
        p.setBrush(kCard);
        p.drawRoundedRect(barCard, 10, 10);
        p.restore();
    }
    drawMetricBars(p, barCard);
    y += barH + pad;

    // Per-channel band table
    QRectF tableCard(pad, y, W - 2*pad, tableH);
    {
        p.save();
        p.setPen(QPen(kCardBorder, 1));
        p.setBrush(kCard);
        p.drawRoundedRect(tableCard, 10, 10);
        p.restore();
    }
    drawBandTable(p, tableCard);
}

void AffectivePanel::drawStatusRow(QPainter& p, QRectF rect)
{
    p.save();

    // Title
    QFont tf("Segoe UI", 9, QFont::Bold);
    tf.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    p.setFont(tf);
    p.setPen(kAccentCyan);
    p.drawText(rect.adjusted(14, 6, 0, 0), Qt::AlignLeft | Qt::AlignTop, "AFFECTIVE COMPUTING");

    // Calibration status pill
    QString pillText;
    QColor  pillCol;
    if (m_calibrating)
    {
        int remaining = kCalibDuration - m_calibSeconds;
        pillText = QString("CALIBRATING  %1 s").arg(remaining);
        pillCol  = kWarn;
    }
    else if (m_calibrated)
    {
        pillText = "CALIBRATED";
        pillCol  = kFocusGreen;
    }
    else
    {
        pillText = "RAW  (no calibration)";
        pillCol  = kSubText;
    }

    QFont pf("Segoe UI", 7, QFont::Bold);
    pf.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    QFontMetrics fm(pf);
    int pw2 = fm.horizontalAdvance(pillText) + 16;
    QRectF pill(rect.right() - pw2 - 10, rect.top() + 9, pw2, 18);
    p.setPen(Qt::NoPen);
    p.setBrush(pillCol.darker(280));
    p.drawRoundedRect(pill, 4, 4);
    p.setPen(pillCol);
    p.setFont(pf);
    p.drawText(pill, Qt::AlignCenter, pillText);

    p.restore();
}

void AffectivePanel::drawCircumplex(QPainter& p, QRectF rect)
{
    p.save();

    const double margin = 26.0;
    QRectF plot(rect.left() + margin, rect.top() + margin,
                rect.width() - 2*margin, rect.height() - 2*margin);

    // Centre
    QPointF O(plot.center());
    double  rx = plot.width()  * 0.5;
    double  ry = plot.height() * 0.5;

    // Quadrant fills
    auto fillQuad = [&](double vSign, double aSign, QColor col) {
        QRectF q;
        if (vSign >= 0 && aSign >= 0) q = QRectF(O.x(), plot.top(),   rx, ry);
        if (vSign >= 0 && aSign <  0) q = QRectF(O.x(), O.y(),        rx, ry);
        if (vSign <  0 && aSign >= 0) q = QRectF(plot.left(), plot.top(), rx, ry);
        if (vSign <  0 && aSign <  0) q = QRectF(plot.left(), O.y(),   rx, ry);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawRect(q);
    };
    p.setClipRect(plot);
    fillQuad( 1,  1, kHappy);
    fillQuad( 1, -1, kRelaxed);
    fillQuad(-1,  1, kAngry);
    fillQuad(-1, -1, kSad);
    p.setClipping(false);

    // Clip to rounded rect border
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect.adjusted(2,2,-2,-2), 12, 12);
    p.setClipPath(clipPath);

    // Concentric circles
    for (int ring = 1; ring <= 4; ++ring)
    {
        double fraction = ring / 4.0;
        QRectF r(O.x() - rx*fraction, O.y() - ry*fraction,
                 rx*fraction*2,        ry*fraction*2);
        p.setPen(QPen(kGridLine, 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(r);
    }

    // Axes
    p.setPen(QPen(QColor(55, 65, 95), 1, Qt::DashLine));
    p.drawLine(QPointF(plot.left(), O.y()), QPointF(plot.right(), O.y()));
    p.drawLine(QPointF(O.x(), plot.top()),  QPointF(O.x(), plot.bottom()));

    // Axis labels
    QFont af("Segoe UI", 7, QFont::Bold);
    af.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    p.setFont(af);
    p.setPen(QColor(80, 95, 130));

    auto axLbl = [&](QPointF pos, const QString& txt, Qt::Alignment al) {
        QRectF r(pos.x()-40, pos.y()-8, 80, 16);
        p.drawText(r, al | Qt::AlignVCenter, txt);
    };
    axLbl(QPointF(plot.right() - 2, O.y() - 10), "POSITIVE →", Qt::AlignRight);
    axLbl(QPointF(plot.left()  + 2, O.y() - 10), "← NEGATIVE", Qt::AlignLeft);
    axLbl(QPointF(O.x(), plot.top()  + 2),        "HIGH AROUSAL", Qt::AlignHCenter);
    axLbl(QPointF(O.x(), plot.bottom() - 2),       "LOW AROUSAL",  Qt::AlignHCenter);

    // Quadrant corner labels
    QFont qlf("Segoe UI", 7);
    p.setFont(qlf);
    auto ql = [&](QPointF pos, const QString& txt, Qt::Alignment al, QColor col) {
        p.setPen(col.lighter(120));
        QRectF r(pos.x()-50, pos.y()-8, 100, 16);
        p.drawText(r, al | Qt::AlignVCenter, txt);
    };
    ql(QPointF(plot.right()-4, plot.top()+10),    "EXCITED",   Qt::AlignRight, kHappy.lighter(150));
    ql(QPointF(plot.right()-4, plot.bottom()-10),  "RELAXED",   Qt::AlignRight, kRelaxed.lighter(180));
    ql(QPointF(plot.left()+4,  plot.top()+10),    "TENSE",     Qt::AlignLeft,  kAngry.lighter(150));
    ql(QPointF(plot.left()+4,  plot.bottom()-10),  "DEPRESSED", Qt::AlignLeft,  kSad.lighter(180));

    p.setClipping(false);

    // Trail
    if (m_trail.size() >= 2)
    {
        p.setClipPath(clipPath);
        const int N = static_cast<int>(m_trail.size());
        auto toScreen = [&](double v, double a) -> QPointF {
            return QPointF(O.x() + v * rx, O.y() - a * ry);
        };

        QPainterPath trail;
        trail.moveTo(toScreen(m_trail[0].v, m_trail[0].a));
        for (int i = 1; i < N; ++i)
            trail.lineTo(toScreen(m_trail[i].v, m_trail[i].a));

        QColor dotColor = affectColor(m_normValence, m_normArousal);
        QPen trailPen(dotColor, 1.5);
        trailPen.setColor(QColor(dotColor.red(), dotColor.green(), dotColor.blue(), 80));
        p.setPen(trailPen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(trail);
        p.setClipping(false);
    }

    // Current position dot
    {
        QPointF dot(O.x() + m_dispValence * rx, O.y() - m_dispArousal * ry);
        QColor  col = affectColor(m_dispValence, m_dispArousal);

        // Glow
        QRadialGradient glow(dot, 18);
        glow.setColorAt(0.0, QColor(col.red(), col.green(), col.blue(), 120));
        glow.setColorAt(1.0, QColor(col.red(), col.green(), col.blue(),   0));
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(dot, 18, 18);

        // Outer ring
        p.setPen(QPen(col.lighter(130), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(dot, 8, 8);

        // Fill
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(dot, 5, 5);
        p.setBrush(col);
        p.drawEllipse(dot, 3.5, 3.5);
    }

    // Crosshair dashes
    {
        QPointF dot(O.x() + m_dispValence * rx, O.y() - m_dispArousal * ry);
        QColor  col = affectColor(m_dispValence, m_dispArousal);
        QPen cp(QColor(col.red(), col.green(), col.blue(), 60), 1, Qt::DashLine);
        p.setPen(cp);
        p.drawLine(QPointF(plot.left(), dot.y()), QPointF(dot.x() - 10, dot.y()));
        p.drawLine(QPointF(dot.x() + 10, dot.y()), QPointF(plot.right(), dot.y()));
        p.drawLine(QPointF(dot.x(), plot.top()),   QPointF(dot.x(), dot.y() - 10));
        p.drawLine(QPointF(dot.x(), dot.y() + 10), QPointF(dot.x(), plot.bottom()));
    }

    // Coordinate readout
    {
        QFont rf("Segoe UI", 7, QFont::Bold);
        p.setFont(rf);
        QColor col = affectColor(m_dispValence, m_dispArousal);

        QString coords = QString("V: %1  A: %2")
            .arg(m_dispValence, 0, 'f', 2)
            .arg(m_dispArousal, 0, 'f', 2);

        QString quad = quadrantLabel(m_dispValence, m_dispArousal);

        QRectF cr(rect.left() + 10, rect.bottom() - 26, rect.width() - 20, 18);
        p.setPen(col.lighter(120));
        p.drawText(cr, Qt::AlignHCenter | Qt::AlignVCenter,
                   quad + "   (" + coords + ")");
    }

    p.restore();
}

void AffectivePanel::drawMetricBars(QPainter& p, QRectF rect)
{
    p.save();

    drawSectionLabel(p, rect.adjusted(12, 7, 0, 0), "VALENCE  &  AROUSAL  METRICS");

    const int lx  = static_cast<int>(rect.left() + 12);
    const int bw  = static_cast<int>(rect.width() - 24);
    const int barW = static_cast<int>(bw * 0.65);
    const int labelW = bw - barW - 4;

    // Bipolar bar: centre = 0, left = negative, right = positive
    auto drawBiBar = [&](int y, const QString& name, double normVal, QColor posCol, QColor negCol)
    {
        QFont lf("Segoe UI", 8, QFont::Bold);
        p.setFont(lf);
        p.setPen(normVal >= 0 ? posCol : negCol);
        p.drawText(QRectF(lx, y, labelW, 20), Qt::AlignLeft | Qt::AlignVCenter, name);

        // Value text
        QFont vf("Segoe UI", 7);
        p.setFont(vf);
        p.setPen(kSubText);
        QString vs = QString("%1%2").arg(normVal >= 0 ? "+" : "").arg(normVal, 0, 'f', 2);
        p.drawText(QRectF(lx + labelW, y, barW, 20), Qt::AlignRight | Qt::AlignVCenter, vs);

        int bx = lx + labelW + 4;
        QRectF bg(bx, y + 4, barW - 32, 14);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(28, 34, 52));
        p.drawRoundedRect(bg, 3, 3);

        // Centre line
        double cx = bg.left() + bg.width() * 0.5;
        p.setPen(QPen(QColor(55, 65, 95), 1));
        p.drawLine(QPointF(cx, bg.top()-1), QPointF(cx, bg.bottom()+1));

        // Fill from centre
        double half = bg.width() * 0.5;
        double fill = std::abs(normVal) * half;
        QColor col  = normVal >= 0 ? posCol : negCol;
        QLinearGradient grad(0, 0, bg.width(), 0);
        grad.setColorAt(0, col.darker(160));
        grad.setColorAt(1, col);
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        if (normVal >= 0)
            p.drawRoundedRect(QRectF(cx, bg.top(), fill, bg.height()), 3, 3);
        else
            p.drawRoundedRect(QRectF(cx - fill, bg.top(), fill, bg.height()), 3, 3);
    };

    // Arousal: unipolar 0→1 bar
    auto drawUniBar = [&](int y, const QString& name, double normVal, QColor col)
    {
        QFont lf("Segoe UI", 8, QFont::Bold);
        p.setFont(lf);
        p.setPen(col);
        p.drawText(QRectF(lx, y, labelW, 20), Qt::AlignLeft | Qt::AlignVCenter, name);

        QFont vf("Segoe UI", 7);
        p.setFont(vf);
        p.setPen(kSubText);
        QString vs = QString("%1%2").arg(normVal >= 0 ? "+" : "").arg(normVal, 0, 'f', 2);
        p.drawText(QRectF(lx + labelW, y, barW, 20), Qt::AlignRight | Qt::AlignVCenter, vs);

        int bx = lx + labelW + 4;
        QRectF bg(bx, y + 4, barW - 32, 14);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(28, 34, 52));
        p.drawRoundedRect(bg, 3, 3);

        double frac = (normVal + 1.0) * 0.5;   // −1→0%, +1→100%
        QLinearGradient grad(bg.left(), 0, bg.right(), 0);
        grad.setColorAt(0, col.darker(180));
        grad.setColorAt(1, col);
        p.setBrush(grad);
        double fw = std::max(6.0, bg.width() * frac);
        p.drawRoundedRect(QRectF(bg.left(), bg.top(), fw, bg.height()), 3, 3);
    };

    int y0 = static_cast<int>(rect.top()) + 22;
    drawBiBar(y0,      "Valence  (FAA)",  m_dispValence, kFocusGreen, kAlert);
    drawUniBar(y0 + 26, "Arousal  (β/α)", m_dispArousal,
               affectColor(m_dispValence, m_dispArousal));

    p.restore();
}

void AffectivePanel::drawBandTable(QPainter& p, QRectF rect)
{
    p.save();

    drawSectionLabel(p, rect.adjusted(12, 7, 0, 0), "PER-CHANNEL  α  /  β  POWER");

    static const char* names[8] = { "Fp1", "Fp2", "F3", "F4", "T3", "T4", "P3", "P4" };
    static const QColor chCols[8] = {
        {0,255,255},{255,255,0},{0,255,0},{255,128,0},
        {255,0,255},{100,180,255},{255,80,80},{180,255,128}
    };

    const double maxA = *std::max_element(m_alpha, m_alpha+8) + 1e-30;
    const double maxB = *std::max_element(m_beta,  m_beta +8) + 1e-30;

    const int lx = static_cast<int>(rect.left() + 10);
    const int tw = static_cast<int>(rect.width() - 20);
    const int cw = tw / 8;
    const int y0 = static_cast<int>(rect.top()) + 22;

    QFont nf("Segoe UI", 6, QFont::Bold);

    for (int ch = 0; ch < 8; ++ch)
    {
        int cx = lx + ch * cw;

        // Channel name
        p.setFont(nf);
        p.setPen(chCols[ch]);
        p.drawText(QRectF(cx, y0, cw, 12), Qt::AlignHCenter | Qt::AlignVCenter, names[ch]);

        // Alpha mini-bar
        double aN = m_alpha[ch] / maxA;
        QRectF aBg(cx + 2, y0 + 14, cw - 4, 6);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(28, 34, 52));
        p.drawRoundedRect(aBg, 2, 2);
        p.setBrush(QColor(80, 140, 255));
        p.drawRoundedRect(QRectF(aBg.left(), aBg.top(), std::max(2.0, aBg.width()*aN), aBg.height()), 2, 2);

        // Beta mini-bar
        double bN = m_beta[ch] / maxB;
        QRectF bBg(cx + 2, y0 + 22, cw - 4, 6);
        p.setBrush(QColor(28, 34, 52));
        p.drawRoundedRect(bBg, 2, 2);
        p.setBrush(QColor(255, 100, 180));
        p.drawRoundedRect(QRectF(bBg.left(), bBg.top(), std::max(2.0, bBg.width()*bN), bBg.height()), 2, 2);
    }

    // Legend
    QFont lf("Segoe UI", 6);
    p.setFont(lf);
    int ly = y0 + 32;
    p.setPen(QColor(80, 140, 255));
    p.drawText(QRectF(lx, ly, 60, 10), Qt::AlignLeft, "α  Alpha");
    p.setPen(QColor(255, 100, 180));
    p.drawText(QRectF(lx + 60, ly, 60, 10), Qt::AlignLeft, "β  Beta");

    p.restore();
}
