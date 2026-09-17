#include "mainwindow.h"

#include <QComboBox>
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
        return QColor("#48D597");
    if (level < 0.50)
        return QColor("#39C5E8");
    if (level < 0.75)
        return QColor("#E8C84A");
    if (level < 1.00)
        return QColor("#F39A45");
    return QColor("#F05B68");
}

int WaveformListWidget::channelAtY(int y) const
{
    if (!m_model || y < 0)
        return -1;
    const double heightPerChannel = rowHeight();
    if (heightPerChannel <= 0.0)
        return -1;
    const int channel = static_cast<int>(y / heightPerChannel);
    return channel < m_model->channelCount() ? channel : -1;
}

void WaveformListWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(event->rect(), QColor("#07111D"));
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
        painter.fillRect(rowRect, QColor(channel % 2 ? "#08131F" : "#0A1725"));
        painter.setPen(QPen(QColor("#15283A"), 1.0));
        painter.drawLine(rowRect.bottomLeft(), rowRect.bottomRight());

        const QRectF labelArea = rowRect.adjusted(16.0, 0.0, -width() + 104.0, 0.0);
        painter.setPen(QColor("#C9D7E5"));
        painter.setFont(QFont("Segoe UI", 7, QFont::DemiBold));
        painter.drawText(labelArea,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString("CH-%1").arg(channel + 1, 3, 10, QLatin1Char('0')));

        const QRectF plot(112.0,
                          rowRect.top() + 1.0,
                          qMax(2.0, width() - 128.0),
                          qMax(2.0, heightPerChannel - 2.0));
        painter.setPen(QPen(QColor("#1C3044"), 1.0));
        for (int division = 0; division <= 8; ++division)
        {
            const double x = plot.left() + plot.width() * division / 8.0;
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        painter.setPen(QPen(QColor("#36506A"), 1.0));
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
    setWindowTitle("微震阵列实时监测 · B 版");
    resize(1440, 900);
    setMinimumSize(1080, 680);

    auto *central = new QWidget(this);
    central->setObjectName("appRoot");
    setCentralWidget(central);
    auto *pageLayout = new QVBoxLayout(central);
    pageLayout->setContentsMargins(20, 16, 20, 20);
    pageLayout->setSpacing(12);

    auto *headerLayout = new QHBoxLayout;
    auto *headingLayout = new QVBoxLayout;
    headingLayout->setSpacing(2);
    auto *eyebrow = new QLabel("MICROSEISMIC ARRAY  /  100-CHANNEL LIVE MONITOR");
    eyebrow->setObjectName("eyebrow");
    auto *heading = new QLabel("100 路实时波形监测");
    heading->setObjectName("pageTitle");
    auto *subtitle = new QLabel("统一采集时钟 · 五级振幅着色 · 5 秒滚动窗口");
    subtitle->setObjectName("subtitle");
    headingLayout->addWidget(eyebrow);
    headingLayout->addWidget(heading);
    headingLayout->addWidget(subtitle);
    headerLayout->addLayout(headingLayout);
    headerLayout->addStretch();
    auto *onlineCountLabel = new QLabel("● 100 / 100 在线");
    onlineCountLabel->setObjectName("onlineBadge");
    headerLayout->addWidget(onlineCountLabel, 0, Qt::AlignBottom);
    pageLayout->addLayout(headerLayout);

    auto *controlPanel = new QWidget;
    controlPanel->setObjectName("panel");
    auto *controlLayout = new QHBoxLayout(controlPanel);
    controlLayout->setContentsMargins(14, 10, 14, 10);
    controlLayout->setSpacing(10);
    auto *sampleRateCaption = new QLabel("采样率");
    sampleRateCaption->setObjectName("caption");
    controlLayout->addWidget(sampleRateCaption);
    sampleRateBox = new QComboBox;
    sampleRateBox->addItems({"1000 Hz", "500 Hz"});
    sampleRateBox->setFixedWidth(108);
    controlLayout->addWidget(sampleRateBox);
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
    controlLayout->addStretch();
    for (const QString &legend : {QString("● 低"), QString("● 较低"), QString("● 中高"), QString("● 较高"), QString("● 高")})
    {
        auto *label = new QLabel(legend);
        label->setObjectName("legend");
        controlLayout->addWidget(label);
    }
    auto *dataCaption = new QLabel("累计采样点");
    dataCaption->setObjectName("caption");
    controlLayout->addSpacing(8);
    controlLayout->addWidget(dataCaption);
    dataCountLabel = new QLabel("0");
    dataCountLabel->setObjectName("dataCount");
    controlLayout->addWidget(dataCountLabel);
    pageLayout->addWidget(controlPanel);

    auto *waveformPanel = new QWidget;
    waveformPanel->setObjectName("panel");
    auto *waveformLayout = new QVBoxLayout(waveformPanel);
    waveformLayout->setContentsMargins(10, 10, 10, 10);
    waveformLayout->setSpacing(8);
    auto *waveformHeader = new QHBoxLayout;
    auto *waveformTitle = new QLabel("实时通道波形");
    waveformTitle->setObjectName("sectionTitle");
    auto *listBadge = new QLabel("100 CHANNELS  ·  50 / VIEW");
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
        QWidget#appRoot { background: #07111D; color: #D7E3EF; }
        QWidget#panel { background: #0D1A29; border: 1px solid #1B2E42; border-radius: 10px; }
        QLabel { color: #D7E3EF; font-family: "Segoe UI", "Microsoft YaHei UI"; }
        QLabel#eyebrow { color: #6FA7D8; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
        QLabel#pageTitle { color: #EAF2F8; font-size: 25px; font-weight: 700; }
        QLabel#subtitle, QLabel#caption { color: #71869B; }
        QLabel#subtitle { font-size: 12px; }
        QLabel#caption { font-size: 10px; font-weight: 600; }
        QLabel#sectionTitle { color: #E3EDF6; font-size: 15px; font-weight: 700; }
        QLabel#onlineBadge { color: #48D597; background: #102638; border: 1px solid #244057; border-radius: 13px; padding: 6px 12px; font-weight: 600; }
        QLabel#subtleBadge { color: #82A9C9; background: #122437; border-radius: 10px; padding: 4px 9px; font-size: 9px; font-weight: 700; }
        QLabel#statusValue { color: #7F93A7; font-weight: 600; }
        QLabel#dataCount { color: #EAF2F8; min-width: 92px; font-size: 15px; font-weight: 700; }
        QLabel#legend { color: #8AA0B5; font-size: 10px; font-weight: 600; }
        QComboBox { color: #D7E3EF; background: #101F30; border: 1px solid #24384D; border-radius: 5px; padding: 7px 9px; }
        QComboBox::drop-down { border: 0; width: 22px; }
        QComboBox QAbstractItemView { color: #D7E3EF; background: #101F30; selection-background-color: #315D83; }
        QPushButton { color: #C4D2DF; background: #132538; border: 1px solid #294158; border-radius: 5px; padding: 9px; font-weight: 600; }
        QPushButton:hover { background: #183149; }
        QPushButton:disabled { color: #52677A; background: #0D1925; border-color: #192B3C; }
        QPushButton#primaryButton { color: #F3F8FC; background: #3D78A8; border-color: #4B88B8; }
        QPushButton#primaryButton:hover { background: #4A88B8; }
        QFrame#divider { color: #294158; }
        QScrollArea#monitorScroll { background: #07111D; border: 1px solid #1B2E42; border-radius: 6px; }
        QScrollBar:vertical { background: #091521; width: 11px; margin: 0; }
        QScrollBar::handle:vertical { background: #36516A; border-radius: 5px; min-height: 36px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");

    const QList<QLabel *> legendLabels = controlPanel->findChildren<QLabel *>("legend");
    const QStringList legendColors = {"#48D597", "#39C5E8", "#E8C84A", "#F39A45", "#F05B68"};
    for (int index = 0; index < legendLabels.size(); ++index)
        legendLabels[index]->setStyleSheet(QString("color: %1;").arg(legendColors[index]));

    timer = new QTimer(this);
    timer->setInterval(20);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startAcquisition);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopAcquisition);
    connect(timer, &QTimer::timeout, this, &MainWindow::generateData);
    connect(sampleRateBox, &QComboBox::currentIndexChanged, this, &MainWindow::changeSampleRate);
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
    statusLabel->setStyleSheet("color: #48D597;");
}

void MainWindow::stopAcquisition()
{
    timer->stop();
    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    statusLabel->setText("●  已暂停");
    statusLabel->setStyleSheet("color: #F39A45;");
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
            double value = signalGenerator.sample(channel, t, sampleRate);
            value += eventScheduler.impulse(channel, t);
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
