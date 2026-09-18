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
class QMouseEvent;
class QColor;


class WaveformListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformListWidget(WaveformModel *model, QWidget *parent = nullptr);

    double rowHeight() const;
    void setViewportHeight(int height);
    int visibleRows() const;
    void setVisibleRows(int rows);
    static QColor colorForAmplitude(double amplitude);
    int channelAtY(int y) const;
    int selectedChannel() const;
    void setSelectedChannel(int channel);

signals:
    void channelSelected(int channel);
    void channelActivated(int channel);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    WaveformModel *m_model;
    int m_visibleRows = 50;
    int m_viewportHeight = 650;
    int m_selectedChannel = -1;
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
