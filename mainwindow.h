#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QVector>

#include "waveformmodel.h"

class QTimer;
class QPushButton;
class QComboBox;
class QLabel;
class QPaintEvent;
class QMouseEvent;


class OverviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewWidget(WaveformModel *model, QWidget *parent = nullptr);

    int channelAt(const QPoint &point) const;
    void setSelectedChannel(int channel);

signals:
    void channelSelected(int channel);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    WaveformModel *m_model;
    int m_selectedChannel = 0;
};


class DetailWaveformWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DetailWaveformWidget(WaveformModel *model, QWidget *parent = nullptr);

    void setChannel(int channel);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    WaveformModel *m_model;
    int m_channel = 0;
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
    void selectChannel(int channel);

private:
    void updateDetailLabels();

    WaveformModel waveformModel;
    OverviewWidget *overview;
    DetailWaveformWidget *detailWaveform;

    QTimer *timer;

    QPushButton *startButton;
    QPushButton *stopButton;

    QComboBox *sampleRateBox;

    QLabel *statusLabel;
    QLabel *dataCountLabel;
    QLabel *onlineCountLabel;
    QLabel *detailChannelLabel;
    QLabel *detailStatusLabel;
    QLabel *detailSampleRateLabel;
    QLabel *detailPeakLabel;
    QLabel *detailRmsLabel;

    int sampleRate = 1000;
    int selectedChannel = 0;
    int updateCount = 0;
    long long totalDataCount = 0;
    long long samplesPerChannel = 0;
};

#endif
