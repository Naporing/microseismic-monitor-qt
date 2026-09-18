#include "mainwindow.h"

#include <QColor>
#include <QImage>
#include <QSignalSpy>
#include <QTest>

class WaveformWidgetsTest : public QObject
{
    Q_OBJECT

private slots:
    void mapsVerticalPositionToChannel();
    void ignoresPositionsOutsideRows();
    void mapsAmplitudeToFiveColors_data();
    void mapsAmplitudeToFiveColors();
    void preservesHighSpikeWhenDownsampling();
    void keepsPartialBufferAtRightEdge();
    void selectsAndActivatesClickedChannel();
    void paintsSelectedChannelDistinctly();
    void changesVisibleRowCountForDrawer();
    void paintsSpectrumAxisLabels();
};

void WaveformWidgetsTest::mapsVerticalPositionToChannel()
{
    WaveformModel model(100, 800);
    WaveformListWidget list(&model);
    list.setViewportHeight(650);

    QCOMPARE(list.height(), 1300);
    QCOMPARE(list.rowHeight(), 13.0);
    QCOMPARE(list.channelAtY(0), 0);
    QCOMPARE(list.channelAtY(649), 49);
    QCOMPARE(list.channelAtY(650), 50);
    QCOMPARE(list.channelAtY(1299), 99);
}

void WaveformWidgetsTest::ignoresPositionsOutsideRows()
{
    WaveformModel model(100, 800);
    WaveformListWidget list(&model);
    list.setViewportHeight(650);

    QCOMPARE(list.channelAtY(-1), -1);
    QCOMPARE(list.channelAtY(1300), -1);
}

void WaveformWidgetsTest::mapsAmplitudeToFiveColors_data()
{
    QTest::addColumn<double>("amplitude");
    QTest::addColumn<QColor>("expected");

    QTest::newRow("zero") << 0.0 << QColor("#16805B");
    QTest::newRow("green upper") << 0.249 << QColor("#16805B");
    QTest::newRow("cyan lower") << 0.25 << QColor("#087EA4");
    QTest::newRow("cyan upper") << 0.499 << QColor("#087EA4");
    QTest::newRow("yellow lower") << 0.50 << QColor("#B78016");
    QTest::newRow("yellow upper") << 0.749 << QColor("#B78016");
    QTest::newRow("orange lower") << 0.75 << QColor("#D96C18");
    QTest::newRow("orange upper") << 0.999 << QColor("#D96C18");
    QTest::newRow("red lower") << 1.00 << QColor("#C9363E");
    QTest::newRow("absolute amplitude") << -1.20 << QColor("#C9363E");
}

void WaveformWidgetsTest::mapsAmplitudeToFiveColors()
{
    QFETCH(double, amplitude);
    QFETCH(QColor, expected);

    QCOMPARE(WaveformListWidget::colorForAmplitude(amplitude), expected);
}

void WaveformWidgetsTest::preservesHighSpikeWhenDownsampling()
{
    WaveformModel model(100, 5000);
    QVector<double> samples(5000, 0.0);
    samples[3] = 1.2;
    model.appendSamples(0, samples);
    WaveformListWidget list(&model);
    list.setViewportHeight(650);
    list.resize(760, list.height());

    QImage image(760, 13, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    list.render(&image, QPoint(), QRegion(0, 0, image.width(), image.height()));

    bool foundRed = false;
    for (int y = 0; y < image.height() && !foundRed; ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor color = image.pixelColor(x, y);
            if (color.red() > 185 && color.green() < 90 && color.blue() < 100)
            {
                foundRed = true;
                break;
            }
        }
    }
    QVERIFY(foundRed);
}

void WaveformWidgetsTest::keepsPartialBufferAtRightEdge()
{
    WaveformModel model(100, 5000);
    model.appendSamples(0, QVector<double>(100, 0.1));
    WaveformListWidget list(&model);
    list.setViewportHeight(650);
    list.resize(760, list.height());

    QImage image(760, 13, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    list.render(&image, QPoint(), QRegion(0, 0, image.width(), image.height()));

    int firstGreenX = image.width();
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor color = image.pixelColor(x, y);
            if (color.green() > 110 && color.red() < 80 && color.blue() < 130)
                firstGreenX = qMin(firstGreenX, x);
        }
    }
    QVERIFY(firstGreenX > 700);
}

void WaveformWidgetsTest::selectsAndActivatesClickedChannel()
{
    WaveformModel model(100, 800);
    WaveformListWidget list(&model);
    list.setViewportHeight(650);
    list.resize(760, list.height());
    QSignalSpy selectedSpy(&list, &WaveformListWidget::channelSelected);
    QSignalSpy activatedSpy(&list, &WaveformListWidget::channelActivated);

    QTest::mouseClick(&list, Qt::LeftButton, Qt::NoModifier, QPoint(200, 36 * 13 + 6));
    QCOMPARE(list.selectedChannel(), 36);
    QCOMPARE(selectedSpy.count(), 1);
    QCOMPARE(selectedSpy.first().first().toInt(), 36);

    QTest::mouseDClick(&list, Qt::LeftButton, Qt::NoModifier, QPoint(200, 42 * 13 + 6));
    QCOMPARE(list.selectedChannel(), 42);
    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(activatedSpy.first().first().toInt(), 42);
}

void WaveformWidgetsTest::paintsSelectedChannelDistinctly()
{
    WaveformModel model(100, 800);
    WaveformListWidget list(&model);
    list.setViewportHeight(650);
    list.resize(760, list.height());
    list.setSelectedChannel(3);

    QImage image(760, list.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    list.render(&image);

    QCOMPARE(image.pixelColor(104, 3 * 13 + 6), QColor("#E2F1F6"));
}

void WaveformWidgetsTest::changesVisibleRowCountForDrawer()
{
    WaveformModel model(100, 800);
    WaveformListWidget list(&model);

    list.setVisibleRows(34);
    list.setViewportHeight(680);
    QCOMPARE(list.visibleRows(), 34);
    QCOMPARE(list.height(), 2000);
    QCOMPARE(list.channelAtY(679), 33);
    QCOMPARE(list.channelAtY(680), 34);

    list.setVisibleRows(50);
    QCOMPARE(list.height(), 1360);
    QCOMPARE(list.channelAtY(679), 49);
}

void WaveformWidgetsTest::paintsSpectrumAxisLabels()
{
    WaveformModel model(100, 800);
    ChannelAnalysisPlot plot(&model);
    plot.resize(900, 300);

    QImage image(plot.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    plot.render(&image);

    int frequencyLabelPixels = 0;
    for (int y = 281; y < image.height(); ++y)
    {
        for (int x = 45; x < image.width() - 5; ++x)
        {
            const QColor color = image.pixelColor(x, y);
            if (color.red() < 140 && color.green() < 150 && color.blue() < 160)
                ++frequencyLabelPixels;
        }
    }

    int decibelLabelPixels = 0;
    for (int y = 160; y < 281; ++y)
    {
        for (int x = 0; x < 48; ++x)
        {
            const QColor color = image.pixelColor(x, y);
            if (color.red() < 140 && color.green() < 150 && color.blue() < 160)
                ++decibelLabelPixels;
        }
    }

    QVERIFY(frequencyLabelPixels > 15);
    QVERIFY(decibelLabelPixels > 15);
}

QTEST_MAIN(WaveformWidgetsTest)

#include "test_waveformwidgets.moc"
