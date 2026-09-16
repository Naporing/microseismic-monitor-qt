#include "mainwindow.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <cmath>

namespace {

constexpr double PI = 3.14159265358979323846;

void drawCompactWaveform(QPainter &painter,
                         const QVector<double> &samples,
                         const QRectF &area,
                         const QColor &color)
{
    if (samples.size() < 2 || area.width() < 2.0 || area.height() < 2.0)
        return;

    painter.save();
    painter.setClipRect(area);
    painter.setPen(QPen(color, 1.0));
    const int columns = qMax(1, qMin(static_cast<int>(area.width()), samples.size()));
    const double centerY = area.center().y();
    const double scaleY = area.height() * 0.42;

    for (int column = 0; column < columns; ++column)
    {
        const int begin = column * samples.size() / columns;
        const int end = qMax(begin + 1, (column + 1) * samples.size() / columns);
        double minimum = samples[begin];
        double maximum = samples[begin];
        for (int index = begin + 1; index < end; ++index)
        {
            minimum = qMin(minimum, samples[index]);
            maximum = qMax(maximum, samples[index]);
        }

        const double x = area.left() + (column + 0.5) * area.width() / columns;
        painter.drawLine(QPointF(x, centerY - maximum * scaleY),
                         QPointF(x, centerY - minimum * scaleY));
    }
    painter.restore();
}

void drawDetailWaveform(QPainter &painter,
                        const QVector<double> &samples,
                        const QRectF &area,
                        const QColor &color)
{
    if (samples.size() < 2)
        return;

    QPolygonF points;
    const int pointCount = qMin(samples.size(), qMax(2, static_cast<int>(area.width() * 1.5)));
    points.reserve(pointCount);
    for (int point = 0; point < pointCount; ++point)
    {
        const int index = point * (samples.size() - 1) / (pointCount - 1);
        const double x = area.left() + point * area.width() / (pointCount - 1);
        const double y = area.center().y() - samples[index] * area.height() * 0.42;
        points.append(QPointF(x, y));
    }

    painter.save();
    painter.setClipRect(area);
    painter.setPen(QPen(color, 1.6));
    painter.drawPolyline(points);
    painter.restore();
}

QLabel *makeCaption(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName("caption");
    return label;
}

QFrame *makeDivider()
{
    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setObjectName("divider");
    return line;
}

} // namespace

OverviewWidget::OverviewWidget(WaveformModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    setMinimumSize(700, 650);
    setCursor(Qt::PointingHandCursor);
}

int OverviewWidget::channelAt(const QPoint &point) const
{
    if (!rect().contains(point) || width() <= 0 || height() <= 0)
        return -1;

    const int column = qMin(9, point.x() * 10 / width());
    const int row = qMin(9, point.y() * 10 / height());
    const int channel = row * 10 + column;
    return m_model && channel < m_model->channelCount() ? channel : -1;
}

void OverviewWidget::setSelectedChannel(int channel)
{
    if (!m_model || channel < 0 || channel >= m_model->channelCount())
        return;

    m_selectedChannel = channel;
    update();
}

void OverviewWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#080D18"));
    if (!m_model)
        return;

    const double cellWidth = width() / 10.0;
    const double cellHeight = height() / 10.0;
    const int channelCount = qMin(100, m_model->channelCount());
    for (int channel = 0; channel < channelCount; ++channel)
    {
        const int row = channel / 10;
        const int column = channel % 10;
        const QRectF cell(column * cellWidth + 2.5,
                          row * cellHeight + 2.5,
                          cellWidth - 5.0,
                          cellHeight - 5.0);
        const QVector<double> &samples = m_model->samples(channel);
        const bool selected = channel == m_selectedChannel;
        const bool alarm = m_model->peak(channel) > 0.78;
        const QColor signalColor = samples.isEmpty()
                                       ? QColor("#536174")
                                       : alarm ? QColor("#FF8A5B") : QColor("#31D7A5");

        painter.setPen(QPen(selected ? QColor("#4DA3FF") : QColor("#1B2A3D"),
                            selected ? 2.0 : 1.0));
        painter.setBrush(QColor(selected ? "#112842" : "#0E1726"));
        painter.drawRoundedRect(cell, 4.0, 4.0);

        painter.setPen(QColor("#B7C5D8"));
        painter.setFont(QFont("Segoe UI", 7, QFont::DemiBold));
        painter.drawText(cell.adjusted(6, 3, -5, 0),
                         Qt::AlignLeft | Qt::AlignTop,
                         QString("CH-%1").arg(channel + 1, 3, 10, QLatin1Char('0')));
        painter.setPen(Qt::NoPen);
        painter.setBrush(signalColor);
        painter.drawEllipse(QPointF(cell.right() - 8.0, cell.top() + 8.0), 2.5, 2.5);

        const QRectF plot = cell.adjusted(5.0, 18.0, -5.0, -5.0);
        painter.setPen(QPen(QColor("#24354A"), 0.8));
        painter.drawLine(QPointF(plot.left(), plot.center().y()),
                         QPointF(plot.right(), plot.center().y()));
        drawCompactWaveform(painter, samples, plot, signalColor);
    }
}

void OverviewWidget::mousePressEvent(QMouseEvent *event)
{
    const int channel = channelAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && channel >= 0)
    {
        setSelectedChannel(channel);
        emit channelSelected(channel);
    }
    QWidget::mousePressEvent(event);
}

DetailWaveformWidget::DetailWaveformWidget(WaveformModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    setMinimumHeight(280);
}

void DetailWaveformWidget::setChannel(int channel)
{
    if (!m_model || channel < 0 || channel >= m_model->channelCount())
        return;

    m_channel = channel;
    update();
}

void DetailWaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#0A1220"));

    const QRectF plot = QRectF(rect()).adjusted(42.0, 20.0, -14.0, -34.0);
    painter.setPen(QPen(QColor("#213148"), 1.0));
    for (int division = 0; division <= 8; ++division)
    {
        const double x = plot.left() + plot.width() * division / 8.0;
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    for (int division = 0; division <= 6; ++division)
    {
        const double y = plot.top() + plot.height() * division / 6.0;
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    painter.setPen(QPen(QColor("#50647D"), 1.0));
    painter.drawLine(QPointF(plot.left(), plot.center().y()),
                     QPointF(plot.right(), plot.center().y()));
    painter.setPen(QColor("#7890AA"));
    painter.setFont(QFont("Segoe UI", 8));
    painter.drawText(QRectF(4, plot.top() - 8, 34, 20), Qt::AlignRight, "+1.0");
    painter.drawText(QRectF(4, plot.center().y() - 10, 34, 20), Qt::AlignRight, "0");
    painter.drawText(QRectF(4, plot.bottom() - 10, 34, 20), Qt::AlignRight, "-1.0");
    painter.drawText(QRectF(plot.left(), plot.bottom() + 8, plot.width(), 20),
                     Qt::AlignCenter,
                     "最近 1 秒");

    if (m_model)
    {
        const bool alarm = m_model->peak(m_channel) > 0.78;
        drawDetailWaveform(painter,
                           m_model->samples(m_channel),
                           plot,
                           alarm ? QColor("#FF8A5B") : QColor("#39E6B0"));
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , waveformModel(100, 5000)
{
    setWindowTitle("微震阵列实时监测 · 100 通道总览");
    resize(1520, 900);
    setMinimumSize(1280, 760);

    auto *central = new QWidget(this);
    central->setObjectName("appRoot");
    setCentralWidget(central);
    auto *pageLayout = new QVBoxLayout(central);
    pageLayout->setContentsMargins(24, 18, 24, 22);
    pageLayout->setSpacing(16);

    auto *headerLayout = new QHBoxLayout;
    auto *headingLayout = new QVBoxLayout;
    headingLayout->setSpacing(2);
    auto *eyebrow = new QLabel("MICROSEISMIC ARRAY  /  LIVE OVERVIEW");
    eyebrow->setObjectName("eyebrow");
    auto *heading = new QLabel("微震阵列实时监测");
    heading->setObjectName("pageTitle");
    auto *subtitle = new QLabel("100 路同步采集 · 时域波形总览与通道诊断");
    subtitle->setObjectName("subtitle");
    headingLayout->addWidget(eyebrow);
    headingLayout->addWidget(heading);
    headingLayout->addWidget(subtitle);
    headerLayout->addLayout(headingLayout);
    headerLayout->addStretch();
    onlineCountLabel = new QLabel("● 100 / 100 在线");
    onlineCountLabel->setObjectName("onlineBadge");
    headerLayout->addWidget(onlineCountLabel, 0, Qt::AlignBottom);
    pageLayout->addLayout(headerLayout);

    auto *bodyLayout = new QHBoxLayout;
    bodyLayout->setSpacing(14);
    pageLayout->addLayout(bodyLayout, 1);

    auto *controlPanel = new QWidget;
    controlPanel->setObjectName("panel");
    controlPanel->setFixedWidth(220);
    auto *controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(18, 20, 18, 18);
    controlLayout->setSpacing(11);
    auto *controlTitle = new QLabel("采集控制");
    controlTitle->setObjectName("sectionTitle");
    controlLayout->addWidget(controlTitle);
    controlLayout->addWidget(makeCaption("采样率"));
    sampleRateBox = new QComboBox;
    sampleRateBox->addItems({"1000 Hz", "500 Hz"});
    controlLayout->addWidget(sampleRateBox);
    controlLayout->addWidget(makeCaption("显示时窗"));
    auto *displayWindow = new QLabel("最近 1 秒");
    displayWindow->setObjectName("readOnlyField");
    controlLayout->addWidget(displayWindow);
    startButton = new QPushButton("开始采集");
    startButton->setObjectName("primaryButton");
    stopButton = new QPushButton("停止采集");
    stopButton->setEnabled(false);
    controlLayout->addSpacing(4);
    controlLayout->addWidget(startButton);
    controlLayout->addWidget(stopButton);
    controlLayout->addWidget(makeDivider());
    controlLayout->addWidget(makeCaption("运行状态"));
    statusLabel = new QLabel("●  待机");
    statusLabel->setObjectName("statusValue");
    controlLayout->addWidget(statusLabel);
    controlLayout->addWidget(makeCaption("接入通道"));
    auto *channelCountLabel = new QLabel("100 路");
    channelCountLabel->setObjectName("metricValue");
    controlLayout->addWidget(channelCountLabel);
    controlLayout->addWidget(makeCaption("累计采样点"));
    dataCountLabel = new QLabel("0");
    dataCountLabel->setObjectName("metricValue");
    controlLayout->addWidget(dataCountLabel);
    controlLayout->addStretch();
    auto *hint = new QLabel("点击任一缩略波形\n可在右侧查看详情");
    hint->setObjectName("hint");
    controlLayout->addWidget(hint);
    bodyLayout->addWidget(controlPanel);

    auto *overviewPanel = new QWidget;
    overviewPanel->setObjectName("panel");
    auto *overviewLayout = new QVBoxLayout(overviewPanel);
    overviewLayout->setContentsMargins(14, 14, 14, 14);
    overviewLayout->setSpacing(10);
    auto *overviewHeader = new QHBoxLayout;
    auto *overviewTitle = new QLabel("全通道波形总览");
    overviewTitle->setObjectName("sectionTitle");
    auto *gridBadge = new QLabel("10 × 10 MATRIX");
    gridBadge->setObjectName("subtleBadge");
    overviewHeader->addWidget(overviewTitle);
    overviewHeader->addStretch();
    overviewHeader->addWidget(gridBadge);
    overviewLayout->addLayout(overviewHeader);
    overview = new OverviewWidget(&waveformModel);
    overview->setObjectName("overviewWidget");
    overviewLayout->addWidget(overview, 1);
    bodyLayout->addWidget(overviewPanel, 1);

    auto *detailPanel = new QWidget;
    detailPanel->setObjectName("panel");
    detailPanel->setFixedWidth(350);
    auto *detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(18, 18, 18, 18);
    detailLayout->setSpacing(10);
    detailLayout->addWidget(makeCaption("SELECTED CHANNEL"));
    detailChannelLabel = new QLabel("CH-001");
    detailChannelLabel->setObjectName("detailChannelLabel");
    detailLayout->addWidget(detailChannelLabel);
    detailStatusLabel = new QLabel("●  等待数据");
    detailStatusLabel->setObjectName("detailStatus");
    detailLayout->addWidget(detailStatusLabel);
    detailLayout->addSpacing(4);
    detailWaveform = new DetailWaveformWidget(&waveformModel);
    detailWaveform->setObjectName("detailWaveformWidget");
    detailLayout->addWidget(detailWaveform, 1);
    detailLayout->addWidget(makeDivider());
    detailLayout->addWidget(makeCaption("通道指标"));
    detailSampleRateLabel = new QLabel;
    detailPeakLabel = new QLabel;
    detailRmsLabel = new QLabel;
    for (QLabel *label : {detailSampleRateLabel, detailPeakLabel, detailRmsLabel})
    {
        label->setObjectName("detailMetric");
        detailLayout->addWidget(label);
    }
    detailLayout->addStretch();
    bodyLayout->addWidget(detailPanel);

    setStyleSheet(R"(
        QWidget#appRoot { background: #070B13; color: #DCE6F2; }
        QWidget#panel { background: #0D1522; border: 1px solid #1B293B; border-radius: 10px; }
        QLabel { color: #DCE6F2; font-family: "Segoe UI", "Microsoft YaHei UI"; }
        QLabel#eyebrow { color: #4DA3FF; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
        QLabel#pageTitle { color: #F2F7FC; font-size: 25px; font-weight: 700; }
        QLabel#subtitle, QLabel#caption, QLabel#hint { color: #73869E; }
        QLabel#subtitle { font-size: 12px; }
        QLabel#caption { font-size: 10px; font-weight: 600; }
        QLabel#sectionTitle { color: #EDF5FF; font-size: 15px; font-weight: 700; }
        QLabel#onlineBadge { color: #53E0AE; background: #0E2A25; border: 1px solid #19483D; border-radius: 13px; padding: 6px 12px; font-weight: 600; }
        QLabel#subtleBadge { color: #78A9DA; background: #111F30; border-radius: 10px; padding: 4px 9px; font-size: 9px; font-weight: 700; }
        QLabel#readOnlyField { color: #AFC0D3; background: #101B2B; border: 1px solid #22344A; border-radius: 5px; padding: 8px; }
        QLabel#statusValue { color: #7F93AA; font-weight: 600; }
        QLabel#metricValue { color: #F2F7FC; font-size: 20px; font-weight: 700; }
        QLabel#hint { font-size: 10px; line-height: 1.4; }
        QLabel#detailChannelLabel { color: #FFFFFF; font-size: 28px; font-weight: 700; }
        QLabel#detailStatus { color: #7F93AA; font-weight: 600; }
        QLabel#detailMetric { color: #B8C7D8; background: #101B2B; border-radius: 5px; padding: 9px 11px; }
        QComboBox { color: #DCE6F2; background: #101B2B; border: 1px solid #22344A; border-radius: 5px; padding: 7px 9px; }
        QComboBox::drop-down { border: 0; width: 22px; }
        QComboBox QAbstractItemView { color: #DCE6F2; background: #101B2B; selection-background-color: #1E5D91; }
        QPushButton { color: #C4D1DF; background: #152235; border: 1px solid #263A51; border-radius: 5px; padding: 9px; font-weight: 600; }
        QPushButton:hover { background: #1B2D44; }
        QPushButton:disabled { color: #536174; background: #101722; border-color: #192435; }
        QPushButton#primaryButton { color: #071812; background: #39D7A5; border-color: #39D7A5; }
        QPushButton#primaryButton:hover { background: #55E3B7; }
        QFrame#divider { color: #1C2A3C; }
    )");

    timer = new QTimer(this);
    timer->setInterval(20);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startAcquisition);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopAcquisition);
    connect(timer, &QTimer::timeout, this, &MainWindow::generateData);
    connect(sampleRateBox, &QComboBox::currentIndexChanged, this, &MainWindow::changeSampleRate);
    connect(overview, &OverviewWidget::channelSelected, this, &MainWindow::selectChannel);
    updateDetailLabels();
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
    statusLabel->setStyleSheet("color: #53E0AE;");
}

void MainWindow::stopAcquisition()
{
    timer->stop();
    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    statusLabel->setText("●  已暂停");
    statusLabel->setStyleSheet("color: #FFBA6B;");
}

void MainWindow::generateData()
{
    const int pointsPerUpdate = sampleRate / 50;
    for (int channel = 0; channel < waveformModel.channelCount(); ++channel)
    {
        QVector<double> batch;
        batch.reserve(pointsPerUpdate);
        const double frequency = 7.0 + (channel % 10) * 2.6 + (channel / 10) * 0.32;
        const double amplitude = 0.22 + (channel % 7) * 0.055;
        const double phase = channel * 0.31;
        for (int point = 0; point < pointsPerUpdate; ++point)
        {
            const double t = static_cast<double>(samplesPerChannel + point) / sampleRate;
            double value = amplitude * qSin(2.0 * PI * frequency * t + phase);
            value += 0.06 * qSin(2.0 * PI * (frequency * 0.17) * t + phase * 0.5);
            value += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.10;

            const bool impulseChannel = channel == 17 || channel == 46 || channel == 83;
            const double cycle = std::fmod(t + channel * 0.07, 6.0);
            if (impulseChannel && cycle > 4.45 && cycle < 4.52)
                value += 0.55 * qSin(2.0 * PI * 90.0 * t);
            batch.append(value);
        }
        waveformModel.appendSamples(channel, batch);
    }

    samplesPerChannel += pointsPerUpdate;
    totalDataCount += static_cast<long long>(pointsPerUpdate) * waveformModel.channelCount();
    dataCountLabel->setText(QLocale().toString(totalDataCount));
    updateDetailLabels();

    if (++updateCount % 2 == 0)
    {
        overview->update();
        detailWaveform->update();
    }
}

void MainWindow::changeSampleRate(int index)
{
    sampleRate = index == 0 ? 1000 : 500;
    updateDetailLabels();
}

void MainWindow::selectChannel(int channel)
{
    if (channel < 0 || channel >= waveformModel.channelCount())
        return;

    selectedChannel = channel;
    overview->setSelectedChannel(channel);
    detailWaveform->setChannel(channel);
    updateDetailLabels();
}

void MainWindow::updateDetailLabels()
{
    const double peak = waveformModel.peak(selectedChannel);
    const bool hasData = !waveformModel.samples(selectedChannel).isEmpty();
    const bool alarm = peak > 0.78;
    detailChannelLabel->setText(
        QString("CH-%1").arg(selectedChannel + 1, 3, 10, QLatin1Char('0')));
    detailSampleRateLabel->setText(QString("采样率    %1 Hz").arg(sampleRate));
    detailPeakLabel->setText(QString("峰值       %1").arg(peak, 0, 'f', 3));
    detailRmsLabel->setText(QString("RMS        %1").arg(waveformModel.rms(selectedChannel), 0, 'f', 3));

    if (!hasData)
    {
        detailStatusLabel->setText("●  等待数据");
        detailStatusLabel->setStyleSheet("color: #7F93AA;");
    }
    else if (alarm)
    {
        detailStatusLabel->setText("●  振幅告警");
        detailStatusLabel->setStyleSheet("color: #FF8A5B;");
    }
    else
    {
        detailStatusLabel->setText("●  信号正常");
        detailStatusLabel->setStyleSheet("color: #53E0AE;");
    }
}
