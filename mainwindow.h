#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QVector>

#include "eventlogic.h"
#include "waveformmodel.h"

class QTimer;
class QPushButton;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPaintEvent;
class QColor;


class WaveformListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformListWidget(WaveformModel *model, QWidget *parent = nullptr);

    double rowHeight() const;
    void setViewportHeight(int height);
    static QColor colorForAmplitude(double amplitude);
    int channelAtY(int y) const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    WaveformModel *m_model;
};


// ========================
// 主窗口
// ========================
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    const WaveformModel *model() const;

private slots:
    void startAcquisition();
    void stopAcquisition();
    void generateData();
    void changeSampleRate(int index);
    void changeSignalMode(int index);
    void changeFrequencyPreset(int index);
    void changeCustomFrequency(double frequencyHz);

private:
    enum class SignalMode
    {
        Seismic,
        Sine
    };

    void resetSimulation();

    WaveformModel waveformModel;
    DemoEventScheduler eventScheduler;
    ChannelEventDetector eventDetector;
    SeismicSignalGenerator signalGenerator;
    WaveformListWidget *waveformList;

    QTimer *timer;

    QPushButton *startButton;
    QPushButton *stopButton;

    QComboBox *sampleRateBox;
    QComboBox *signalModeBox;
    QComboBox *frequencyPresetBox;
    QDoubleSpinBox *customFrequencySpin;

    QLabel *statusLabel;
    QLabel *dataCountLabel;

    int sampleRate = 1000;
    SignalMode signalMode = SignalMode::Seismic;
    double sineFrequencyHz = 20.0;
    int updateCount = 0;
    long long totalDataCount = 0;
    double elapsedSeconds = 0.0;
};

#endif
