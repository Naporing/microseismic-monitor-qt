#include "mainwindow.h"
#include "appversion.h"
#include "updatemanager.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

namespace {

constexpr int DISPLAY_WINDOW_SECONDS = 5;

class MonitorScrollArea : public QScrollArea
{
public:
    using QScrollArea::QScrollArea;

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QScrollArea::resizeEvent(event);
        if (auto *list = qobject_cast<WaveformListWidget *>(widget()))
            list->setViewportHeight(viewport()->height());
    }
};

} // namespace

WaveformListWidget::WaveformListWidget(WaveformModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    setMinimumWidth(760);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setViewportHeight(650);
}

double WaveformListWidget::rowHeight() const
{
    return m_model && m_model->channelCount() > 0
               ? static_cast<double>(height()) / m_model->channelCount()
               : 0.0;
}

void WaveformListWidget::setViewportHeight(int height)
{
    if (!m_model || height <= 0)
        return;

    m_viewportHeight = height;
    setFixedHeight(qCeil(height * m_model->channelCount()
                         / static_cast<double>(m_visibleRows)));
}

int WaveformListWidget::visibleRows() const
{
    return m_visibleRows;
}

void WaveformListWidget::setVisibleRows(int rows)
{
    if (!m_model || m_model->channelCount() <= 0)
        return;

    const int boundedRows = qBound(1, rows, m_model->channelCount());
    if (boundedRows == m_visibleRows)
        return;
    m_visibleRows = boundedRows;
    setViewportHeight(m_viewportHeight);
}

QColor WaveformListWidget::colorForAmplitude(double amplitude)
{
    const double level = qAbs(amplitude);
    if (level < 0.25)
        return QColor("#16805B");
    if (level < 0.50)
        return QColor("#087EA4");
    if (level < 0.75)
        return QColor("#B78016");
    if (level < 1.00)
        return QColor("#D96C18");
    return QColor("#C9363E");
}

int WaveformListWidget::channelAtY(int y) const
{
    if (!m_model || y < 0 || y >= height())
        return -1;
    if (height() <= 0 || m_model->channelCount() <= 0)
        return -1;
    return static_cast<int>(static_cast<qint64>(y) * m_model->channelCount()
                            / height());
}

int WaveformListWidget::selectedChannel() const
{
    return m_selectedChannel;
}

void WaveformListWidget::setSelectedChannel(int channel)
{
    if (!m_model || channel < -1 || channel >= m_model->channelCount()
        || channel == m_selectedChannel)
        return;

    m_selectedChannel = channel;
    update();
}

void WaveformListWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(event->rect(), QColor("#F7F9FA"));
    if (!m_model)
        return;

    const double heightPerChannel = rowHeight();
    if (heightPerChannel <= 0.0)
        return;
    const int firstChannel = qMax(0,
                                  static_cast<int>(event->rect().top() / heightPerChannel));
    const int lastChannel = qMin(m_model->channelCount() - 1,
                                 static_cast<int>(event->rect().bottom() / heightPerChannel));
    for (int channel = firstChannel; channel <= lastChannel; ++channel)
    {
        const QRectF rowRect(0.0,
                             channel * heightPerChannel,
                             width(),
                             heightPerChannel);
        const bool selected = channel == m_selectedChannel;
        painter.fillRect(rowRect,
                         selected
                             ? QColor("#E2F1F6")
                             : QColor(channel % 2 ? "#F3F6F8" : "#FBFCFD"));
        if (selected)
        {
            painter.setPen(QPen(QColor("#2B7896"), 1.0));
            painter.drawRect(rowRect.adjusted(0.5, 0.5, -0.5, -0.5));
        }
        painter.setPen(QPen(QColor("#DDE4E9"), 1.0));
        painter.drawLine(rowRect.bottomLeft(), rowRect.bottomRight());

        if (channel > 0 && channel % 10 == 0)
        {
            painter.setPen(QPen(QColor("#9DADB8"), 1.0));
            painter.drawLine(rowRect.topLeft(), rowRect.topRight());
        }

        if (m_model->eventDetected(channel))
        {
            painter.fillRect(QRectF(4.0, rowRect.top() + 2.0, 5.0,
                                    qMax(3.0, rowRect.height() - 4.0)),
                             QColor("#C9363E"));
            painter.setPen(QPen(QColor("#7F1D24"), 1.0));
            painter.drawRect(QRectF(3.5, rowRect.top() + 1.5, 6.0,
                                    qMax(4.0, rowRect.height() - 3.0)));
        }

        const QRectF labelArea = rowRect.adjusted(15.0, 0.0, -width() + 100.0, 0.0);
        painter.setPen(QColor("#344451"));
        painter.setFont(QFont("Cascadia Mono", 7, QFont::DemiBold));
        painter.drawText(labelArea,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString("CH-%1").arg(channel + 1, 3, 10, QLatin1Char('0')));

        const QRectF plot(108.0,
                          rowRect.top() + 1.0,
                          qMax(2.0, width() - 122.0),
                          qMax(2.0, heightPerChannel - 2.0));
        painter.setPen(QPen(QColor("#E1E7EB"), 1.0));
        for (int division = 0; division <= 10; ++division)
        {
            const double x = plot.left() + plot.width() * division / 10.0;
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        painter.setPen(QPen(QColor("#AAB8C3"), 1.0));
        painter.drawLine(QPointF(plot.left(), plot.center().y()),
                         QPointF(plot.right(), plot.center().y()));

        const QVector<double> &samples = m_model->samples(channel);
        if (samples.size() < 2)
            continue;

        painter.save();
        painter.setClipRect(plot);
        const int columns = qMax(2, static_cast<int>(plot.width()));
        const int capacity = m_model->maxPoints();
        const int availableStart = capacity - samples.size();
        QPointF previousPoint;
        double previousPeak = 0.0;
        bool hasPreviousPoint = false;
        for (int column = 0; column < columns; ++column)
        {
            const int windowBegin = column * capacity / columns;
            const int windowEnd = qMax(windowBegin + 1,
                                       (column + 1) * capacity / columns);
            const int bucketBegin = qMax(windowBegin, availableStart);
            const int bucketEnd = qMin(windowEnd, capacity);
            if (bucketBegin >= bucketEnd)
                continue;

            const int firstSample = bucketBegin - availableStart;
            const int lastSample = bucketEnd - availableStart;
            double minimum = samples[firstSample];
            double maximum = samples[firstSample];
            double peak = qAbs(samples[firstSample]);
            for (int index = firstSample + 1; index < lastSample; ++index)
            {
                minimum = qMin(minimum, samples[index]);
                maximum = qMax(maximum, samples[index]);
                peak = qMax(peak, qAbs(samples[index]));
            }

            const double x = plot.left() + (column + 0.5) * plot.width() / columns;
            painter.setPen(QPen(colorForAmplitude(peak), 1.35));
            painter.drawLine(QPointF(x, plot.center().y() - maximum * plot.height() * 0.43),
                             QPointF(x, plot.center().y() - minimum * plot.height() * 0.43));

            const double value = samples[lastSample - 1];
            const QPointF currentPoint(
                x,
                plot.center().y() - value * plot.height() * 0.43);
            if (hasPreviousPoint)
            {
                painter.setPen(QPen(colorForAmplitude(qMax(previousPeak, peak)), 1.35));
                painter.drawLine(previousPoint, currentPoint);
            }
            previousPoint = currentPoint;
            previousPeak = peak;
            hasPreviousPoint = true;
        }
        painter.restore();
    }
}

void WaveformListWidget::mousePressEvent(QMouseEvent *event)
{
    const int channel = channelAtY(qFloor(event->position().y()));
    if (event->button() == Qt::LeftButton && channel >= 0)
    {
        setSelectedChannel(channel);
        emit channelSelected(channel);
    }
    QWidget::mousePressEvent(event);
}

void WaveformListWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    const int channel = channelAtY(qFloor(event->position().y()));
    if (event->button() == Qt::LeftButton && channel >= 0)
    {
        setSelectedChannel(channel);
        emit channelSelected(channel);
        emit channelActivated(channel);
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

ChannelAnalysisPlot::ChannelAnalysisPlot(const WaveformModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ChannelAnalysisPlot::setChannel(int channel)
{
    if (m_channel == channel)
        return;
    m_channel = channel;
    update();
}

void ChannelAnalysisPlot::setSpectrum(const SpectrumResult &spectrum)
{
    m_spectrum = spectrum;
    update();
}

void ChannelAnalysisPlot::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#FBFCFD"));

    const double plotHeight = qMax(42.0, (height() - 70.0) / 2.0);
    const QRectF timePlot(52.0, 22.0, qMax(40.0, width() - 68.0), plotHeight);
    const QRectF spectrumPlot(52.0,
                              timePlot.bottom() + 28.0,
                              qMax(40.0, width() - 68.0),
                              plotHeight);
    auto drawGrid = [&painter](const QRectF &plot) {
        painter.fillRect(plot, QColor("#F7F9FA"));
        painter.setPen(QPen(QColor("#DFE6EA"), 1.0));
        for (int division = 0; division <= 10; ++division)
        {
            const double x = plot.left() + plot.width() * division / 10.0;
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        for (int division = 0; division <= 4; ++division)
        {
            const double y = plot.top() + plot.height() * division / 4.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        painter.setPen(QPen(QColor("#A8B7C1"), 1.0));
        painter.drawRect(plot);
    };

    painter.setFont(QFont("Segoe UI Variable", 9, QFont::DemiBold));
    painter.setPen(QColor("#344451"));
    painter.drawText(QRectF(52.0, 2.0, timePlot.width(), 18.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     "时域波形  ·  最近 5.0 s");
    painter.drawText(QRectF(52.0, timePlot.bottom() + 8.0, spectrumPlot.width(), 18.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     "FFT 频谱  ·  0–200 Hz / -60–0 dB");
    drawGrid(timePlot);
    drawGrid(spectrumPlot);

    painter.setFont(QFont("Cascadia Mono", 8));
    painter.setPen(QColor("#5D6F7C"));
    for (int tick = 0; tick <= 4; ++tick)
    {
        const double x = spectrumPlot.left() + spectrumPlot.width() * tick / 4.0;
        painter.drawText(QRectF(x - 24.0,
                                spectrumPlot.bottom() + 2.0,
                                48.0,
                                16.0),
                         Qt::AlignHCenter | Qt::AlignTop,
                         QString::number(tick * 50));
    }
    for (int tick = 0; tick <= 2; ++tick)
    {
        const double y = spectrumPlot.bottom() - spectrumPlot.height() * tick / 2.0;
        painter.drawText(QRectF(0.0, y - 8.0, 46.0, 16.0),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(-60 + tick * 30));
    }

    const QVector<double> &samples = m_model ? m_model->samples(m_channel)
                                              : QVector<double>();
    if (samples.size() >= 2)
    {
        painter.save();
        painter.setClipRect(timePlot);
        const int columns = qMax(2, static_cast<int>(timePlot.width()));
        QPointF previous;
        double previousPeak = 0.0;
        bool hasPrevious = false;
        for (int column = 0; column < columns; ++column)
        {
            const int begin = column * samples.size() / columns;
            const int end = qMax(begin + 1,
                                 (column + 1) * samples.size() / columns);
            double minimum = samples[begin];
            double maximum = samples[begin];
            double peak = qAbs(samples[begin]);
            for (int index = begin + 1; index < end; ++index)
            {
                minimum = qMin(minimum, samples[index]);
                maximum = qMax(maximum, samples[index]);
                peak = qMax(peak, qAbs(samples[index]));
            }
            const double x = timePlot.left()
                             + (column + 0.5) * timePlot.width() / columns;
            const double scale = timePlot.height() * 0.42;
            painter.setPen(QPen(WaveformListWidget::colorForAmplitude(peak), 1.2));
            painter.drawLine(QPointF(x, timePlot.center().y() - maximum * scale),
                             QPointF(x, timePlot.center().y() - minimum * scale));
            const QPointF current(x,
                                  timePlot.center().y()
                                      - samples[end - 1] * scale);
            if (hasPrevious)
            {
                painter.setPen(QPen(WaveformListWidget::colorForAmplitude(
                                        qMax(previousPeak, peak)),
                                    1.2));
                painter.drawLine(previous, current);
            }
            previous = current;
            previousPeak = peak;
            hasPrevious = true;
        }
        painter.restore();
    }

    if (m_spectrum.isValid())
    {
        QPainterPath spectrumPath;
        for (int index = 0; index < m_spectrum.frequencies.size(); ++index)
        {
            const double x = spectrumPlot.left()
                             + m_spectrum.frequencies[index] / 200.0
                                   * spectrumPlot.width();
            const double y = spectrumPlot.bottom()
                             - (m_spectrum.magnitudesDb[index] + 60.0) / 60.0
                                   * spectrumPlot.height();
            if (index == 0)
                spectrumPath.moveTo(x, y);
            else
                spectrumPath.lineTo(x, y);
        }
        painter.setClipRect(spectrumPlot);
        painter.setPen(QPen(QColor("#176B87"), 1.6));
        painter.drawPath(spectrumPath);
        painter.setClipping(false);
    }
    else
    {
        painter.setPen(QColor("#687B88"));
        painter.setFont(QFont("Segoe UI Variable", 9));
        painter.drawText(spectrumPlot,
                         Qt::AlignCenter,
                         "正在积累频谱数据（至少 256 点）");
    }
}

MainWindow::MainWindow(QWidget *parent, UpdateManager *updates, InstallerLauncher launcher)
    : QMainWindow(parent)
    , updateManager(updates ? updates : new UpdateManager(this))
    , installerLauncher(launcher ? std::move(launcher) : InstallerLauncher([](const QString &path, const QStringList &args) {
          return QProcess::startDetached(path, args);
      }))
    , waveformModel(100, 1000 * DISPLAY_WINDOW_SECONDS)
    , eventScheduler(100, 20260916)
    , eventDetector(100)
    , signalGenerator(100, 16092026)
{
    setWindowTitle("微震阵列实时监测 · 科研仪器台");
    resize(1440, 900);
    setMinimumSize(1080, 680);

    auto *central = new QWidget(this);
    central->setObjectName("appRoot");
    central->setProperty("designVariant", "B");
    setCentralWidget(central);
    auto *pageLayout = new QVBoxLayout(central);
    pageLayout->setContentsMargins(16, 12, 16, 16);
    pageLayout->setSpacing(8);

    auto *instrumentHeader = new QWidget;
    instrumentHeader->setObjectName("instrumentHeader");
    auto *headerLayout = new QHBoxLayout(instrumentHeader);
    headerLayout->setContentsMargins(16, 8, 14, 8);
    auto *headingLayout = new QVBoxLayout;
    headingLayout->setSpacing(0);
    auto *eyebrow = new QLabel("MICROSEISMIC LAB  /  100-CHANNEL RECORDER");
    eyebrow->setObjectName("eyebrow");
    auto *heading = new QLabel("阵列信号采集仪");
    heading->setObjectName("pageTitle");
    auto *subtitle = new QLabel("实时记录  ·  统一时钟  ·  5.0 秒观测窗");
    subtitle->setObjectName("subtitle");
    headingLayout->addWidget(eyebrow);
    headingLayout->addWidget(heading);
    headingLayout->addWidget(subtitle);
    headerLayout->addLayout(headingLayout);
    headerLayout->addStretch();
    auto *currentVersionLabel = new QLabel("版本 v" APP_VERSION);
    currentVersionLabel->setObjectName("currentVersionLabel");
    headerLayout->addWidget(currentVersionLabel);
    checkUpdateButton = new QPushButton("检查更新");
    checkUpdateButton->setObjectName("checkUpdateButton");
    headerLayout->addWidget(checkUpdateButton);
    headerLayout->addSpacing(12);
    auto *onlineCountLabel = new QLabel("●  设备在线  100 / 100");
    onlineCountLabel->setObjectName("onlineBadge");
    headerLayout->addWidget(onlineCountLabel, 0, Qt::AlignVCenter);
    pageLayout->addWidget(instrumentHeader);

    auto *controlPanel = new QWidget;
    controlPanel->setObjectName("readoutStrip");
    auto *controlLayout = new QHBoxLayout(controlPanel);
    controlLayout->setContentsMargins(12, 7, 12, 7);
    controlLayout->setSpacing(8);
    auto *sampleRateCaption = new QLabel("采样率");
    sampleRateCaption->setObjectName("caption");
    controlLayout->addWidget(sampleRateCaption);
    sampleRateBox = new QComboBox;
    sampleRateBox->setObjectName("sampleRateBox");
    sampleRateBox->addItems({"1000 Hz", "500 Hz"});
    sampleRateBox->setFixedWidth(108);
    controlLayout->addWidget(sampleRateBox);
    auto *modeCaption = new QLabel("信号");
    modeCaption->setObjectName("caption");
    controlLayout->addWidget(modeCaption);
    signalModeBox = new QComboBox;
    signalModeBox->setObjectName("signalModeBox");
    signalModeBox->addItems({"地震模拟", "正弦测试"});
    signalModeBox->setFixedWidth(104);
    controlLayout->addWidget(signalModeBox);
    frequencyPresetBox = new QComboBox;
    frequencyPresetBox->setObjectName("frequencyPresetBox");
    frequencyPresetBox->addItems({"20 Hz", "30 Hz", "80 Hz", "自定义"});
    frequencyPresetBox->setFixedWidth(86);
    frequencyPresetBox->setEnabled(false);
    controlLayout->addWidget(frequencyPresetBox);
    startButton = new QPushButton("开始采集");
    startButton->setObjectName("primaryButton");
    stopButton = new QPushButton("停止采集");
    stopButton->setEnabled(false);
    controlLayout->addWidget(startButton);
    controlLayout->addWidget(stopButton);
    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::VLine);
    separator->setObjectName("divider");
    controlLayout->addWidget(separator);
    statusLabel = new QLabel("●  待机");
    statusLabel->setObjectName("statusValue");
    controlLayout->addWidget(statusLabel);
    auto *windowReadout = new QLabel("窗口  5.0 s");
    windowReadout->setObjectName("readout");
    controlLayout->addWidget(windowReadout);
    controlLayout->addStretch();
    auto *amplitudeScale = new QWidget;
    amplitudeScale->setObjectName("amplitudeScale");
    auto *scaleLayout = new QHBoxLayout(amplitudeScale);
    scaleLayout->setContentsMargins(7, 3, 7, 3);
    scaleLayout->setSpacing(8);
    auto *scaleCaption = new QLabel("振幅级别");
    scaleCaption->setObjectName("caption");
    scaleLayout->addWidget(scaleCaption);
    for (const QString &legend : {QString("■ 低"), QString("■ 较低"), QString("■ 中等"), QString("■ 较高"), QString("■ 高")})
    {
        auto *label = new QLabel(legend);
        label->setObjectName("legend");
        scaleLayout->addWidget(label);
    }
    controlLayout->addWidget(amplitudeScale);
    auto *dataCaption = new QLabel("累计采样点");
    dataCaption->setObjectName("caption");
    controlLayout->addSpacing(8);
    controlLayout->addWidget(dataCaption);
    dataCountLabel = new QLabel("0");
    dataCountLabel->setObjectName("dataCount");
    controlLayout->addWidget(dataCountLabel);
    pageLayout->addWidget(controlPanel);

    auto *waveformPanel = new QWidget;
    waveformPanel->setObjectName("instrumentStage");
    auto *waveformLayout = new QVBoxLayout(waveformPanel);
    waveformLayout->setContentsMargins(9, 8, 9, 9);
    waveformLayout->setSpacing(6);
    auto *waveformHeader = new QHBoxLayout;
    auto *waveformTitle = new QLabel("实时通道记录");
    waveformTitle->setObjectName("sectionTitle");
    visibleChannelBadge = new QLabel("通道编号   ·   时间轴 5.0 s   ·   当前显示 50 路");
    visibleChannelBadge->setObjectName("visibleChannelBadge");
    waveformHeader->addWidget(waveformTitle);
    waveformHeader->addStretch();
    waveformHeader->addWidget(visibleChannelBadge);
    waveformLayout->addLayout(waveformHeader);

    auto *scrollArea = new MonitorScrollArea;
    scrollArea->setObjectName("monitorScroll");
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    waveformList = new WaveformListWidget(&waveformModel);
    waveformList->setObjectName("waveformListWidget");
    scrollArea->setWidget(waveformList);
    waveformLayout->addWidget(scrollArea, 1);

    monitorSplitter = new QSplitter(Qt::Vertical);
    monitorSplitter->setObjectName("monitorSplitter");
    monitorSplitter->setChildrenCollapsible(false);
    monitorSplitter->setHandleWidth(6);
    monitorSplitter->addWidget(waveformPanel);

    channelDetailPanel = new QWidget;
    channelDetailPanel->setObjectName("channelDetailPanel");
    channelDetailPanel->setMinimumHeight(220);
    auto *detailLayout = new QVBoxLayout(channelDetailPanel);
    detailLayout->setContentsMargins(10, 8, 10, 8);
    detailLayout->setSpacing(6);
    auto *detailHeader = new QHBoxLayout;
    detailChannelLabel = new QLabel("CH---  单通道分析");
    detailChannelLabel->setObjectName("detailChannelLabel");
    detailHeader->addWidget(detailChannelLabel);
    detailHeader->addSpacing(12);
    detailPeakLabel = new QLabel("峰值  --");
    detailPeakLabel->setObjectName("detailMetric");
    detailHeader->addWidget(detailPeakLabel);
    detailRmsLabel = new QLabel("RMS  --");
    detailRmsLabel->setObjectName("detailMetric");
    detailHeader->addWidget(detailRmsLabel);
    detailFrequencyLabel = new QLabel("主频  --");
    detailFrequencyLabel->setObjectName("detailFrequencyLabel");
    detailHeader->addWidget(detailFrequencyLabel);
    detailHeader->addStretch();
    auto *detailCloseButton = new QPushButton("收起详情");
    detailCloseButton->setObjectName("detailCloseButton");
    detailHeader->addWidget(detailCloseButton);
    detailLayout->addLayout(detailHeader);
    channelAnalysisPlot = new ChannelAnalysisPlot(&waveformModel);
    channelAnalysisPlot->setObjectName("channelAnalysisPlot");
    detailLayout->addWidget(channelAnalysisPlot, 1);
    monitorSplitter->addWidget(channelDetailPanel);
    monitorSplitter->setStretchFactor(0, 1);
    monitorSplitter->setStretchFactor(1, 0);
    channelDetailPanel->hide();
    pageLayout->addWidget(monitorSplitter, 1);

    setStyleSheet(R"(
        QWidget#appRoot { background: #E9EEF2; color: #17212B; }
        QWidget#instrumentHeader { background: #F8FAFB; border: 1px solid #C9D3DB; border-radius: 3px; }
        QWidget#readoutStrip { background: #F4F7F9; border: 1px solid #C7D2DA; border-radius: 3px; }
        QWidget#instrumentStage { background: #F8FAFB; border: 1px solid #C7D2DA; border-radius: 3px; }
        QWidget#channelDetailPanel { background: #F8FAFB; border: 1px solid #AFC1CC; border-radius: 3px; }
        QWidget#amplitudeScale { background: #FFFFFF; border: 1px solid #D2DAE0; border-radius: 2px; }
        QLabel { color: #22303B; font-family: "Segoe UI Variable", "Microsoft YaHei UI"; }
        QLabel#eyebrow { color: #607687; font-family: "Cascadia Mono"; font-size: 9px; font-weight: 700; letter-spacing: 1px; }
        QLabel#pageTitle { color: #17212B; font-size: 20px; font-weight: 700; }
        QLabel#subtitle { color: #5D6F7C; font-size: 11px; }
        QLabel#currentVersionLabel { color: #607687; font-size: 11px; padding: 0 6px; }
        QLabel#caption { color: #5D6F7C; font-size: 10px; font-weight: 650; }
        QLabel#sectionTitle { color: #24333E; font-size: 13px; font-weight: 700; }
        QLabel#detailChannelLabel { color: #1D3544; font-family: "Cascadia Mono"; font-size: 12px; font-weight: 750; }
        QLabel#detailMetric, QLabel#detailFrequencyLabel { color: #4E6472; font-family: "Cascadia Mono"; font-size: 10px; font-weight: 650; }
        QLabel#onlineBadge { color: #106846; background: #E4F3EB; border: 1px solid #A8D4BF; border-radius: 3px; padding: 6px 10px; font-weight: 700; }
        QLabel#visibleChannelBadge { color: #60717E; font-family: "Cascadia Mono"; font-size: 9px; font-weight: 650; }
        QLabel#statusValue { color: #566A78; font-weight: 700; }
        QLabel#readout { color: #314552; border-left: 1px solid #CAD4DB; padding: 3px 10px; font-family: "Cascadia Mono"; font-weight: 650; }
        QLabel#dataCount { color: #17212B; min-width: 92px; font-family: "Cascadia Mono"; font-size: 14px; font-weight: 700; }
        QLabel#legend { font-family: "Cascadia Mono"; font-size: 9px; font-weight: 700; }
        QComboBox, QDoubleSpinBox { color: #253642; background: #FFFFFF; border: 1px solid #AEBBC5; border-radius: 3px; padding: 6px 8px; }
        QComboBox::drop-down { border: 0; width: 22px; }
        QComboBox QAbstractItemView { color: #253642; background: #FFFFFF; selection-background-color: #CBE3ED; }
        QPushButton { color: #314552; background: #FFFFFF; border: 1px solid #AEBBC5; border-radius: 3px; padding: 7px 11px; font-weight: 650; }
        QPushButton:hover { background: #EDF4F7; border-color: #718C9D; }
        QPushButton:focus { border: 2px solid #176B87; }
        QPushButton:disabled { color: #9AA8B2; background: #EDF1F3; border-color: #D5DDE2; }
        QPushButton#primaryButton { color: #FFFFFF; background: #176B87; border-color: #12566D; }
        QPushButton#primaryButton:hover { background: #1E7D9B; }
        QFrame#divider { color: #BAC6CE; }
        QScrollArea#monitorScroll { background: #F7F9FA; border: 1px solid #C7D2DA; border-radius: 2px; }
        QScrollBar:vertical { background: #E7ECEF; width: 10px; margin: 0; }
        QScrollBar::handle:vertical { background: #98A8B4; border-radius: 2px; min-height: 36px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QSplitter#monitorSplitter::handle { background: #D5DEE4; border: 1px solid #C0CDD5; }
    )");

    const QList<QLabel *> legendLabels = controlPanel->findChildren<QLabel *>("legend");
    const QStringList legendColors = {"#16805B", "#087EA4", "#B78016", "#D96C18", "#C9363E"};
    for (int index = 0; index < legendLabels.size(); ++index)
        legendLabels[index]->setStyleSheet(QString("color: %1;").arg(legendColors[index]));

    timer = new QTimer(this);
    timer->setInterval(20);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startAcquisition);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopAcquisition);
    connect(timer, &QTimer::timeout, this, &MainWindow::generateData);
    connect(sampleRateBox, &QComboBox::currentIndexChanged, this, &MainWindow::changeSampleRate);
    connect(signalModeBox, &QComboBox::currentIndexChanged, this, &MainWindow::changeSignalMode);
    connect(frequencyPresetBox, &QComboBox::activated, this, &MainWindow::changeFrequencyPreset);
    connect(waveformList, &WaveformListWidget::channelSelected, this, &MainWindow::selectChannel);
    connect(waveformList, &WaveformListWidget::channelActivated, this, &MainWindow::openChannelDetail);
    connect(detailCloseButton, &QPushButton::clicked, this, &MainWindow::closeChannelDetail);
    connectUpdates();
}

MainWindow::~MainWindow() = default;

void MainWindow::showUpdateMessage(const QString &message)
{
    auto *dialog = new QMessageBox(QMessageBox::Information, "软件更新", message, QMessageBox::Ok, this);
    dialog->setObjectName("updateMessage");
    dialog->setTextFormat(Qt::PlainText);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    dialog->show();
}

void MainWindow::closeDownloadProgress()
{
    if (updateProgress)
    {
        updateProgress->hide();
        updateProgress->deleteLater();
        updateProgress = nullptr;
    }
}

void MainWindow::showRelease(const ReleaseInfo &release)
{
    if (updatePromptOpen)
        return;
    updatePromptOpen = true;
    checkUpdateButton->setEnabled(false);
    auto *dialog = new QDialog(this);
    dialog->setObjectName("updateAvailableDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("发现新版本 " + release.tagName);
    dialog->resize(480, 320);
    auto *layout = new QVBoxLayout(dialog);
    auto *description = new QLabel(QString("当前版本 v%1 → %2\n下载完成后将自动安装并重启软件。").arg(APP_VERSION, release.tagName));
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *notes = new QPlainTextEdit;
    notes->setReadOnly(true);
    notes->setPlainText(release.notes.isEmpty() ? "此版本暂无发布说明。" : release.notes);
    layout->addWidget(notes, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No);
    buttons->button(QDialogButtonBox::Yes)->setText("立即更新");
    buttons->button(QDialogButtonBox::No)->setText("稍后");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(dialog, &QDialog::finished, this, [this, release](int result) {
        updatePromptOpen = false;
        checkUpdateButton->setEnabled(true);
        if (result != QDialog::Accepted)
            return;
        updateProgress = new QProgressDialog("准备下载更新…", "取消下载", 0, 0, this);
        updateProgress->setObjectName("updateProgressDialog");
        updateProgress->setWindowTitle("软件更新");
        updateProgress->setWindowModality(Qt::NonModal);
        updateProgress->setAutoClose(false);
        updateProgress->setAutoReset(false);
        updateProgress->setMinimumDuration(0);
        connect(updateProgress, &QProgressDialog::canceled, this, [this] {
            updateManager->cancel();
            closeDownloadProgress();
        });
        updateProgress->show();
        updateManager->downloadUpdate(release);
    });
    dialog->show();
}

void MainWindow::connectUpdates()
{
    connect(checkUpdateButton, &QPushButton::clicked, this, [this] { updateManager->checkForUpdates(); });
    connect(updateManager, &UpdateManager::checkingChanged, this, [this](bool checking) {
        checkUpdateButton->setText(checking ? "检查中…" : "检查更新");
    });
    connect(updateManager, &UpdateManager::busyChanged, this, [this](bool busy) {
        checkUpdateButton->setEnabled(!busy && !updatePromptOpen);
        if (!busy)
            checkUpdateButton->setText("检查更新");
    });
    connect(updateManager, &UpdateManager::upToDate, this, [this] {
        checkUpdateButton->setText("已是最新版");
    });
    connect(updateManager, &UpdateManager::updateAvailable, this, &MainWindow::showRelease);
    connect(updateManager, &UpdateManager::failed, this, [this](const QString &message, bool silent) {
        closeDownloadProgress();
        if (!silent)
            showUpdateMessage(message);
    });
    connect(updateManager, &UpdateManager::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (!updateProgress)
            return;
        updateProgress->setLabelText(QString("正在下载更新：%1 MB%2")
            .arg(received / 1048576.0, 0, 'f', 1)
            .arg(total > 0 ? QString(" / %1 MB").arg(total / 1048576.0, 0, 'f', 1) : QString()));
        updateProgress->setRange(0, total > 0 ? 100 : 0);
        if (total > 0)
            updateProgress->setValue(int(qMin(100LL, received * 100 / total)));
    });
    connect(updateManager, &UpdateManager::readyToInstall, this, [this](const QString &path) {
        if (updateProgress)
        {
            updateProgress->setLabelText("校验通过，正在启动安装器…");
            updateProgress->setCancelButton(nullptr);
        }
        const QStringList arguments = {"/VERYSILENT", "/SUPPRESSMSGBOXES", "/SP-",
                                       "/CLOSEAPPLICATIONS", "/NORESTARTAPPLICATIONS",
                                       "/NORESTART", "/UPDATE=1", "/LOG"};
        if (!installerLauncher(path, arguments))
        {
            updateManager->cancel();
            closeDownloadProgress();
            showUpdateMessage("无法启动更新安装器，当前版本仍可继续使用。请稍后重试。");
            return;
        }
        updateManager->preserveInstaller();
        stopAcquisition();
        closeDownloadProgress();
        emit restartRequested();
    });
}

const WaveformModel *MainWindow::model() const
{
    return &waveformModel;
}

void MainWindow::startAcquisition()
{
    timer->start();
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    statusLabel->setText("●  采集中");
    statusLabel->setStyleSheet("color: #16805B;");
}

void MainWindow::stopAcquisition()
{
    timer->stop();
    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    statusLabel->setText("●  已暂停");
    statusLabel->setStyleSheet("color: #A84F0A;");
}

void MainWindow::generateData()
{
    const int pointsPerUpdate = sampleRate / 50;
    const double batchStart = elapsedSeconds;
    for (int channel = 0; channel < waveformModel.channelCount(); ++channel)
    {
        QVector<double> batch;
        batch.reserve(pointsPerUpdate);
        for (int point = 0; point < pointsPerUpdate; ++point)
        {
            const double t = batchStart + static_cast<double>(point) / sampleRate;
            double value = 0.0;
            if (signalMode == SignalMode::Sine)
            {
                value = signalGenerator.sineSample(channel,
                                                   t,
                                                   sampleRate,
                                                   sineFrequencyHz);
            }
            else
            {
                value = signalGenerator.sample(channel, t, sampleRate);
                value += eventScheduler.impulse(channel, t);
            }
            batch.append(value);
        }
        waveformModel.appendSamples(channel, batch);
        eventDetector.update(channel, batch);
        waveformModel.setEventDetected(channel, eventDetector.eventDetected(channel));
    }

    elapsedSeconds += static_cast<double>(pointsPerUpdate) / sampleRate;
    totalDataCount += static_cast<long long>(pointsPerUpdate) * waveformModel.channelCount();
    dataCountLabel->setText(QLocale().toString(totalDataCount));
    ++updateCount;
    if (updateCount % 2 == 0)
        waveformList->update();
    if (channelDetailPanel->isVisible() && updateCount % 5 == 0)
        refreshChannelDetail();
}

void MainWindow::changeSampleRate(int index)
{
    sampleRate = index == 0 ? 1000 : 500;
    waveformModel.clear();
    waveformModel.setMaxPoints(sampleRate * DISPLAY_WINDOW_SECONDS);
    eventDetector.reset();
    waveformList->update();
    if (channelDetailPanel->isVisible())
        refreshChannelDetail();
}

void MainWindow::changeSignalMode(int index)
{
    signalMode = index == 1 ? SignalMode::Sine : SignalMode::Seismic;
    const bool sineMode = signalMode == SignalMode::Sine;
    frequencyPresetBox->setEnabled(sineMode);
    resetSimulation();
}

void MainWindow::changeFrequencyPreset(int index)
{
    if (index < 0 || index > 3)
        return;

    if (index == 3)
    {
        bool accepted = false;
        const double frequencyHz = QInputDialog::getDouble(
            this,
            "自定义正弦频率",
            "频率（Hz）",
            sineFrequencyHz,
            1.0,
            200.0,
            1,
            &accepted,
            Qt::WindowFlags(),
            0.5);
        if (!accepted)
        {
            const QSignalBlocker blocker(frequencyPresetBox);
            frequencyPresetBox->setCurrentIndex(lastFrequencyPresetIndex);
            return;
        }

        sineFrequencyHz = frequencyHz;
        frequencyPresetBox->setItemText(
            3,
            QString("自定义 %1 Hz").arg(frequencyHz, 0, 'f', 1));
    }
    else
    {
        static constexpr double frequencies[] = {20.0, 30.0, 80.0};
        sineFrequencyHz = frequencies[index];
        frequencyPresetBox->setItemText(3, "自定义");
    }

    lastFrequencyPresetIndex = index;
    if (signalMode == SignalMode::Sine)
        resetSimulation();
}

void MainWindow::resetSimulation()
{
    waveformModel.clear();
    eventDetector.reset();
    signalGenerator.reset();
    elapsedSeconds = 0.0;
    waveformList->update();
    if (channelDetailPanel->isVisible())
        refreshChannelDetail();
}

void MainWindow::selectChannel(int channel)
{
    if (channel < 0 || channel >= waveformModel.channelCount())
        return;
    detailChannel = channel;
    if (channelDetailPanel->isVisible())
        refreshChannelDetail();
}

void MainWindow::openChannelDetail(int channel)
{
    selectChannel(channel);
    if (detailChannel < 0)
        return;

    channelDetailPanel->show();
    waveformList->setVisibleRows(34);
    visibleChannelBadge->setText("通道编号   ·   时间轴 5.0 s   ·   当前显示 34 路");
    const int totalHeight = qMax(1, monitorSplitter->height());
    monitorSplitter->setSizes({qRound(totalHeight * 0.68),
                               qRound(totalHeight * 0.32)});
    refreshChannelDetail();
}

void MainWindow::closeChannelDetail()
{
    channelDetailPanel->hide();
    waveformList->setVisibleRows(50);
    visibleChannelBadge->setText("通道编号   ·   时间轴 5.0 s   ·   当前显示 50 路");
}

void MainWindow::refreshChannelDetail()
{
    if (detailChannel < 0 || detailChannel >= waveformModel.channelCount())
        return;

    detailSpectrum = SpectrumAnalyzer::analyze(waveformModel.samples(detailChannel),
                                               sampleRate);
    channelAnalysisPlot->setChannel(detailChannel);
    channelAnalysisPlot->setSpectrum(detailSpectrum);
    detailChannelLabel->setText(QString("CH-%1  单通道分析")
                                    .arg(detailChannel + 1,
                                         3,
                                         10,
                                         QLatin1Char('0')));
    detailPeakLabel->setText(QString("峰值  %1")
                                 .arg(waveformModel.peak(detailChannel),
                                      0,
                                      'f',
                                      3));
    detailRmsLabel->setText(QString("RMS  %1")
                                .arg(waveformModel.rms(detailChannel),
                                     0,
                                     'f',
                                     3));
    detailFrequencyLabel->setText(
        detailSpectrum.isValid()
            ? QString("主频  %1 Hz").arg(detailSpectrum.peakFrequencyHz, 0, 'f', 1)
            : QString("主频  --"));
}
