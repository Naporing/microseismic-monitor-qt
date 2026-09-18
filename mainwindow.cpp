#include "mainwindow.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
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

    setFixedHeight(qCeil(height * m_model->channelCount() / 50.0));
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
        painter.fillRect(rowRect, QColor(channel % 2 ? "#F3F6F8" : "#FBFCFD"));
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
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
    customFrequencySpin = new QDoubleSpinBox;
    customFrequencySpin->setObjectName("customFrequencySpin");
    customFrequencySpin->setRange(1.0, 200.0);
    customFrequencySpin->setDecimals(1);
    customFrequencySpin->setSingleStep(0.5);
    customFrequencySpin->setSuffix(" Hz");
    customFrequencySpin->setValue(20.0);
    customFrequencySpin->setFixedWidth(88);
    customFrequencySpin->setEnabled(false);
    controlLayout->addWidget(customFrequencySpin);
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
    auto *listBadge = new QLabel("通道编号   ·   时间轴 5.0 s   ·   当前显示 50 路");
    listBadge->setObjectName("subtleBadge");
    waveformHeader->addWidget(waveformTitle);
    waveformHeader->addStretch();
    waveformHeader->addWidget(listBadge);
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
    pageLayout->addWidget(waveformPanel, 1);

    setStyleSheet(R"(
        QWidget#appRoot { background: #E9EEF2; color: #17212B; }
        QWidget#instrumentHeader { background: #F8FAFB; border: 1px solid #C9D3DB; border-radius: 3px; }
        QWidget#readoutStrip { background: #F4F7F9; border: 1px solid #C7D2DA; border-radius: 3px; }
        QWidget#instrumentStage { background: #F8FAFB; border: 1px solid #C7D2DA; border-radius: 3px; }
        QWidget#amplitudeScale { background: #FFFFFF; border: 1px solid #D2DAE0; border-radius: 2px; }
        QLabel { color: #22303B; font-family: "Segoe UI Variable", "Microsoft YaHei UI"; }
        QLabel#eyebrow { color: #607687; font-family: "Cascadia Mono"; font-size: 9px; font-weight: 700; letter-spacing: 1px; }
        QLabel#pageTitle { color: #17212B; font-size: 20px; font-weight: 700; }
        QLabel#subtitle { color: #5D6F7C; font-size: 11px; }
        QLabel#caption { color: #5D6F7C; font-size: 10px; font-weight: 650; }
        QLabel#sectionTitle { color: #24333E; font-size: 13px; font-weight: 700; }
        QLabel#onlineBadge { color: #106846; background: #E4F3EB; border: 1px solid #A8D4BF; border-radius: 3px; padding: 6px 10px; font-weight: 700; }
        QLabel#subtleBadge { color: #60717E; font-family: "Cascadia Mono"; font-size: 9px; font-weight: 650; }
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
    connect(frequencyPresetBox, &QComboBox::currentIndexChanged, this, &MainWindow::changeFrequencyPreset);
    connect(customFrequencySpin, &QDoubleSpinBox::valueChanged, this, &MainWindow::changeCustomFrequency);
}

MainWindow::~MainWindow() = default;

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
    if (++updateCount % 2 == 0)
        waveformList->update();
}

void MainWindow::changeSampleRate(int index)
{
    sampleRate = index == 0 ? 1000 : 500;
    waveformModel.clear();
    waveformModel.setMaxPoints(sampleRate * DISPLAY_WINDOW_SECONDS);
    eventDetector.reset();
    waveformList->update();
}

void MainWindow::changeSignalMode(int index)
{
    signalMode = index == 1 ? SignalMode::Sine : SignalMode::Seismic;
    const bool sineMode = signalMode == SignalMode::Sine;
    frequencyPresetBox->setEnabled(sineMode);
    customFrequencySpin->setEnabled(sineMode
                                    && frequencyPresetBox->currentIndex() == 3);
    resetSimulation();
}

void MainWindow::changeFrequencyPreset(int index)
{
    const bool custom = index == 3;
    customFrequencySpin->setEnabled(signalMode == SignalMode::Sine && custom);
    if (!custom)
    {
        static constexpr double frequencies[] = {20.0, 30.0, 80.0};
        if (index >= 0 && index < 3)
            sineFrequencyHz = frequencies[index];
    }
    else
    {
        sineFrequencyHz = customFrequencySpin->value();
    }

    if (signalMode == SignalMode::Sine)
        resetSimulation();
}

void MainWindow::changeCustomFrequency(double frequencyHz)
{
    if (signalMode != SignalMode::Sine
        || frequencyPresetBox->currentIndex() != 3)
        return;

    sineFrequencyHz = frequencyHz;
    resetSimulation();
}

void MainWindow::resetSimulation()
{
    waveformModel.clear();
    eventDetector.reset();
    signalGenerator.reset();
    elapsedSeconds = 0.0;
    waveformList->update();
}
