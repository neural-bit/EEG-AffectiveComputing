#pragma once

#include <QtWidgets/QMainWindow>
#include <QTimer>
#include <QComboBox>
#include <QAction>
#include <QToolBar>
#include <QLabel>
#include <QSerialPortInfo>
#include <QSplitter>

#include <thread>
#include <atomic>
#include <array>
#include <vector>
#include <memory>

#include "OpenBCIReader.h"
#include "ScopeWidget.h"
#include "Filtering.h"
#include "WelchPSD.h"
#include "AffectivePanel.h"

class EmotionDetection : public QMainWindow
{
    Q_OBJECT

public:
    explicit EmotionDetection(QWidget* parent = nullptr);
    ~EmotionDetection() override;

private slots:
    void onConnect();
    void onStartStream();
    void onStopStream();
    void onDisplayTimer();
    void onComboScale(const QString& value);
    void onCalibrate();

private:
    void buildToolbar();
    void buildCentralWidget();
    void streamingThread();
    void processData(double* data);

    // UI
    QComboBox* m_comboPort = nullptr;
    QAction* m_actConnect = nullptr;
    QAction* m_actStart = nullptr;
    QAction* m_actStop = nullptr;
    QAction* m_actCalib = nullptr;
    QComboBox* m_comboScale = nullptr;
    QLabel* m_lblStatus = nullptr;

    ScopeWidget* m_scope = nullptr;
    AffectivePanel* m_affPanel = nullptr;

    QTimer* m_displayTimer = nullptr;

    // Serial reader
    OpenBCIReader* m_reader = nullptr;
    bool              m_isConnected = false;
    std::atomic<bool> m_readerRunning{ false };
    std::thread       m_readerThread;

    // Ring buffer 
    static constexpr int RING_SIZE = 2048;

    struct Sample {
        std::array<double, OpenBCIReader::NUM_CHANNELS> ch{};
    };

    std::array<Sample, RING_SIZE> m_ring;
    std::atomic<int> m_writeIdx{ 0 };
    int              m_readIdx{ 0 };

    // DSP
    Filtering* m_filter[OpenBCIReader::NUM_CHANNELS]{};
    std::unique_ptr<WelchPSD> m_welch[OpenBCIReader::NUM_CHANNELS];

    // Latest per-channel band powers (updated on display timer thread)
    double m_alphaPow[OpenBCIReader::NUM_CHANNELS]{};
    double m_betaPow[OpenBCIReader::NUM_CHANNELS]{};

    // Scale
    int m_scale = 50;
};