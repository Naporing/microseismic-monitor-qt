#include "mainwindow.h"

#include <QColor>
#include <QImage>
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

    QTest::newRow("zero") << 0.0 << QColor("#48D597");
    QTest::newRow("green upper") << 0.249 << QColor("#48D597");
    QTest::newRow("cyan lower") << 0.25 << QColor("#39C5E8");
    QTest::newRow("cyan upper") << 0.499 << QColor("#39C5E8");
    QTest::newRow("yellow lower") << 0.50 << QColor("#E8C84A");
    QTest::newRow("yellow upper") << 0.749 << QColor("#E8C84A");
    QTest::newRow("orange lower") << 0.75 << QColor("#F39A45");
    QTest::newRow("orange upper") << 0.999 << QColor("#F39A45");
    QTest::newRow("red lower") << 1.00 << QColor("#F05B68");
    QTest::newRow("absolute amplitude") << -1.20 << QColor("#F05B68");
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
            if (color.red() > 220 && color.green() < 130 && color.blue() < 150)
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
            if (color.green() > 150 && color.red() < 120 && color.blue() < 170)
                firstGreenX = qMin(firstGreenX, x);
        }
    }
    QVERIFY(firstGreenX > 700);
}

QTEST_MAIN(WaveformWidgetsTest)

#include "test_waveformwidgets.moc"
