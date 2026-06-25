#include "EmotionDetection.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QMessageBox>
#include <QToolBar>
#include <QStatusBar>

#include <algorithm>

// DSP constants
static constexpr int    WELCH_WIN_S = 500;   // 2 s window  @ 250 Hz
static constexpr int    WELCH_HOP_S = 25;    // 100 ms hop  @ 250 Hz

static constexpr double ALPHA_LO = 8.0, ALPHA_HI = 13.0;
static constexpr double BETA_LO = 13.0, BETA_HI = 21.0;

static constexpr double OPENBCI_GAIN = 1'000'000.0;

EmotionDetection::EmotionDetection(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("EEG Emotion Detection");
    resize(1600, 880);
    showMaximized();

    m_lblStatus = new QLabel("  Not connected");
    statusBar()->addPermanentWidget(m_lblStatus);

    for (int ch = 0; ch < OpenBCIReader::NUM_CHANNELS; ++ch)
    {
        m_filter[ch] = new Filtering(static_cast<int>(SR));
        m_filter[ch]->setBandPassFrequencyCorners(1.0, 40.0);

        m_welch[ch] = std::make_unique<WelchPSD>(
            static_cast<int>(SR), WELCH_WIN_S, WELCH_HOP_S);
    }

    buildToolbar();
    buildCentralWidget();

    m_displayTimer = new QTimer(this);
    m_displayTimer->setInterval(16);
    connect(m_displayTimer, &QTimer::timeout, this, &EmotionDetection::onDisplayTimer);
}

EmotionDetection::~EmotionDetection()
{
    if (m_readerRunning)
    {
        m_readerRunning = false;
        if (m_reader) m_reader->stopStreaming();
        m_displayTimer->stop();
        if (m_readerThread.joinable()) m_readerThread.join();
    }

    for (int ch = 0; ch < OpenBCIReader::NUM_CHANNELS; ++ch)
        delete m_filter[ch];

    delete m_reader;
}

void EmotionDetection::buildToolbar()
{
    QToolBar* tb = addToolBar("Main");
    tb->setMovable(false);
    tb->setIconSize(QSize(18, 18));
    tb->setStyleSheet(
        "QToolBar { background:#0d0f18; border-bottom:1px solid #1e2440; spacing:6px; padding:4px; }"
        "QToolButton { color:#c8d0e8; background:transparent; border:1px solid #2a3255;"
        "  border-radius:5px; padding:4px 10px; font:bold 9pt 'Segoe UI'; }"
        "QToolButton:hover   { background:#1e2440; border-color:#3a4870; }"
        "QToolButton:pressed { background:#151b30; }"
        "QToolButton:disabled{ color:#404860; border-color:#1e2440; }"
        "QComboBox { color:#c8d0e8; background:#111420; border:1px solid #2a3255;"
        "  border-radius:5px; padding:3px 8px; min-width:80px; font:9pt 'Segoe UI'; }"
        "QComboBox::drop-down { border:0; }"
        "QComboBox QAbstractItemView { background:#111420; color:#c8d0e8;"
        "  selection-background-color:#1e2c55; }"
        "QLabel { color:#7080a0; font:8pt 'Segoe UI'; padding:0 4px; }"
    );

    tb->addWidget(new QLabel("PORT"));
    m_comboPort = new QComboBox;
    for (const auto& pi : QSerialPortInfo::availablePorts())
        m_comboPort->addItem(pi.portName());
    tb->addWidget(m_comboPort);
    tb->addSeparator();

    m_actConnect = new QAction("CONNECT", this);
    connect(m_actConnect, &QAction::triggered, this, &EmotionDetection::onConnect);
    tb->addAction(m_actConnect);
    tb->addSeparator();

    m_actStart = new QAction("▶  START", this);
    m_actStart->setEnabled(false);
    connect(m_actStart, &QAction::triggered, this, &EmotionDetection::onStartStream);
    tb->addAction(m_actStart);

    m_actStop = new QAction("■  STOP", this);
    m_actStop->setEnabled(false);
    connect(m_actStop, &QAction::triggered, this, &EmotionDetection::onStopStream);
    tb->addAction(m_actStop);
    tb->addSeparator();

    m_actCalib = new QAction("⊙  CALIBRATE", this);
    m_actCalib->setEnabled(false);
    m_actCalib->setToolTip("Record 60 s baseline for Valence / Arousal normalisation");
    connect(m_actCalib, &QAction::triggered, this, &EmotionDetection::onCalibrate);
    tb->addAction(m_actCalib);
    tb->addSeparator();

    tb->addWidget(new QLabel("SCALE"));
    m_comboScale = new QComboBox;
    for (const QString& s : { "10","20","50","100","200","500","1000" })
        m_comboScale->addItem(s);
    m_comboScale->setCurrentText("50");
    connect(m_comboScale, &QComboBox::currentTextChanged,
        this, &EmotionDetection::onComboScale);
    tb->addWidget(m_comboScale);
    tb->addWidget(new QLabel("µV/div"));
}

void EmotionDetection::buildCentralWidget()
{
    QWidget* central = new QWidget;
    central->setStyleSheet("background:#0a0c14;");
    setCentralWidget(central);

    QHBoxLayout* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // EEG scope
    QWidget* leftPanel = new QWidget;
    leftPanel->setStyleSheet("background:#0a0c14;");
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 4, 8);
    leftLayout->setSpacing(6);

    QLabel* scopeLabel = new QLabel(
        "EEG  —  Fp1 · Fp2 . F3 · F4 · T3 · T4 · P3 · P4");
    scopeLabel->setStyleSheet(
        "color:#3a4870; font:bold 8pt 'Segoe UI'; letter-spacing:2px; padding:2px 0;");
    leftLayout->addWidget(scopeLabel);

    m_scope = new ScopeWidget(leftPanel);
    m_scope->setUvPerDiv(50.0);
    leftLayout->addWidget(m_scope, 1);   // scope fills all remaining height

    // Affective panel
    m_affPanel = new AffectivePanel;
    m_affPanel->setFixedWidth(380);
    m_affPanel->setStyleSheet("background:#0a0c14;");

    // Splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    splitter->setStyleSheet("QSplitter::handle { background:#1e2440; width:2px; }");
    splitter->addWidget(leftPanel);
    splitter->addWidget(m_affPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    rootLayout->addWidget(splitter);
}

void EmotionDetection::onConnect()
{
    if (m_isConnected)
    {
        if (m_readerRunning)
        {
            QMessageBox::warning(this, "Busy", "Stop streaming first.");
            return;
        }
        delete m_reader;
        m_reader = nullptr;
        m_isConnected = false;
        m_actConnect->setText("CONNECT");
        m_actStart->setEnabled(false);
        m_lblStatus->setText("  Disconnected");
    }
    else
    {
        QString port = m_comboPort->currentText();
        if (port.isEmpty())
        {
            QMessageBox::information(this, "Port", "No COM ports detected.");
            return;
        }
        try 
        {
            m_reader = new OpenBCIReader(port.toStdString());
        }
        catch (const std::exception& e) 
        {
            QMessageBox::critical(this, "Connection Error", e.what());
            return;
        }
        m_isConnected = true;
        m_actConnect->setText("DISCONNECT");
        m_actStart->setEnabled(true);
        m_lblStatus->setText("  Connected · " + port);
    }
}

void EmotionDetection::onStartStream()
{
    if (!m_isConnected || m_readerRunning) return;

    m_writeIdx.store(0, std::memory_order_relaxed);
    m_readIdx = 0;

    for (int ch = 0; ch < OpenBCIReader::NUM_CHANNELS; ++ch)
    {
        m_welch[ch] = std::make_unique<WelchPSD>(
            static_cast<int>(SR), WELCH_WIN_S, WELCH_HOP_S);
        m_filter[ch]->setBandPassFrequencyCorners(1.0, 40.0);
    }

    m_readerRunning = true;
    m_reader->startStreaming();
    m_displayTimer->start();
    m_affPanel->startSession();
    m_readerThread = std::thread(&EmotionDetection::streamingThread, this);

    m_actStart->setEnabled(false);
    m_actStop->setEnabled(true);
    m_actCalib->setEnabled(true);
    m_lblStatus->setText("  Streaming…");
}

void EmotionDetection::onStopStream()
{
    if (!m_readerRunning) return;

    m_readerRunning = false;
    if (m_reader) m_reader->stopStreaming();
    m_displayTimer->stop();
    m_affPanel->stopSession();

    if (m_readerThread.joinable()) m_readerThread.join();

    m_actStart->setEnabled(true);
    m_actStop->setEnabled(false);
    m_actCalib->setEnabled(false);
    m_lblStatus->setText("  Stopped");
}

void EmotionDetection::onCalibrate()
{
    if (!m_readerRunning) return;

    if (m_affPanel->isCalibrating())
    {
        m_affPanel->stopCalibration();
        m_actCalib->setText("⊙  CALIBRATE");
        m_lblStatus->setText("  Calibration complete");
    }
    else
    {
        m_affPanel->startCalibration();
        m_actCalib->setText("◉  STOP CALIB");
        m_lblStatus->setText("  Calibrating baseline (60 s) — sit calmly…");
    }
}

void EmotionDetection::onComboScale(const QString& value)
{
    m_scale = value.toInt();
    m_scope->setUvPerDiv(m_scale);
}

void EmotionDetection::streamingThread()
{
    double data[OpenBCIReader::NUM_CHANNELS]{};
    while (m_readerRunning.load())
    {
        if (m_reader->readData(data))
            processData(data);
    }
}

void EmotionDetection::processData(double* data)
{
    // Scale raw ADC counts to µV
    for (int ch = 0; ch < OpenBCIReader::NUM_CHANNELS; ++ch)
        data[ch] *= OPENBCI_GAIN;

    // Bandpass filter 1–40 Hz
    for (int ch = 0; ch < OpenBCIReader::NUM_CHANNELS; ++ch)
    {
        double in[2] = { data[ch], data[ch] };
        double out[2] = { 0.0, 0.0 };
        m_filter[ch]->processBP(in, out, 2);
        data[ch] = out[0];
    }

    // Feed all channels into Welch estimators
    for (int ch = 0; ch < OpenBCIReader::NUM_CHANNELS; ++ch)
        m_welch[ch]->pushSample(data[ch]);

    // Write to display ring buffer
    int idx = m_writeIdx.load(std::memory_order_relaxed);
    for (int ch = 0; ch < OpenBCIReader::NUM_CHANNELS; ++ch)
        m_ring[idx].ch[ch] = data[ch];
    m_writeIdx.store((idx + 1) & (RING_SIZE - 1), std::memory_order_release);
}

void EmotionDetection::onDisplayTimer()
{
    // Drain ring buffer into scope
    const int writeIdx = m_writeIdx.load(std::memory_order_acquire);
    while (m_readIdx != writeIdx)
    {
        m_scope->addSample(m_ring[m_readIdx].ch.data(), OpenBCIReader::NUM_CHANNELS);
        m_readIdx = (m_readIdx + 1) & (RING_SIZE - 1);
    }

    // Compute band powers for all 8 channels
    for (int ch = 0; ch < OpenBCIReader::NUM_CHANNELS; ++ch)
    {
        m_alphaPow[ch] = m_welch[ch]->bandPower(ALPHA_LO, ALPHA_HI);
        m_betaPow[ch] = m_welch[ch]->bandPower(BETA_LO, BETA_HI);
    }

    // Feed the affective panel
    m_affPanel->updateAffect(m_alphaPow, m_betaPow);
}