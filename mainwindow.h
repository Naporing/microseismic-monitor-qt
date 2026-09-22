#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QVector>
#include <QPointer>
#include <functional>

#include "eventlogic.h"
#include "spectrumanalyzer.h"
#include "waveformmodel.h"

class QTimer;
class QPushButton;
class QComboBox;
class QLabel;
class QPaintEvent;
class QMouseEvent;
class QColor;
class QSplitter;
class UpdateManager;
class QProgressDialog;
struct ReleaseInfo;


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


class ChannelAnalysisPlot : public QWidget
{
    Q_OBJECT

public:
    explicit ChannelAnalysisPlot(const WaveformModel *model,
                                 QWidget *parent = nullptr);

    void setChannel(int channel);
    void setSpectrum(const SpectrumResult &spectrum);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    const WaveformModel *m_model;
    int m_channel = -1;
    SpectrumResult m_spectrum;
};


// ========================
// 主窗口
// ========================
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    using InstallerLauncher = std::function<bool(const QString &, const QStringList &)>;
    explicit MainWindow(QWidget *parent = nullptr,
                        UpdateManager *updates = nullptr,
                        InstallerLauncher launcher = {});
    ~MainWindow();

    const WaveformModel *model() const;

signals:
    void restartRequested();

private slots:
    void startAcquisition();
    void stopAcquisition();
    void generateData();
    void changeSampleRate(int index);
    void changeSignalMode(int index);
    void changeFrequencyPreset(int index);
    void selectChannel(int channel);
    void openChannelDetail(int channel);
    void closeChannelDetail();

private:
    enum class SignalMode
    {
        Seismic,
        Sine
    };

    void resetSimulation();
    void refreshChannelDetail();
    void connectUpdates();
    void showRelease(const ReleaseInfo &release);
    void showUpdateMessage(const QString &message);
    void closeDownloadProgress();

    UpdateManager *updateManager;
    InstallerLauncher installerLauncher;
    QPushButton *checkUpdateButton;
    QPointer<QProgressDialog> updateProgress;
    bool updatePromptOpen = false;

    WaveformModel waveformModel;
    DemoEventScheduler eventScheduler;
    ChannelEventDetector eventDetector;
    SeismicSignalGenerator signalGenerator;
    WaveformListWidget *waveformList;
    QSplitter *monitorSplitter;
    QWidget *channelDetailPanel;
    ChannelAnalysisPlot *channelAnalysisPlot;

    QTimer *timer;

    QPushButton *startButton;
    QPushButton *stopButton;

    QComboBox *sampleRateBox;
    QComboBox *signalModeBox;
    QComboBox *frequencyPresetBox;

    QLabel *statusLabel;
    QLabel *dataCountLabel;
    QLabel *visibleChannelBadge;
    QLabel *detailChannelLabel;
    QLabel *detailPeakLabel;
    QLabel *detailRmsLabel;
    QLabel *detailFrequencyLabel;

    int sampleRate = 1000;
    SignalMode signalMode = SignalMode::Seismic;
    double sineFrequencyHz = 20.0;
    int lastFrequencyPresetIndex = 0;
    int detailChannel = -1;
    SpectrumResult detailSpectrum;
    int updateCount = 0;
    long long totalDataCount = 0;
    double elapsedSeconds = 0.0;
};

#endif
