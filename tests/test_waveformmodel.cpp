#include "waveformmodel.h"

#include <QTest>
#include <QtMath>

class WaveformModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initializesOneHundredChannels();
    void capsEachChannelBuffer();
    void calculatesPeakAndRms();
    void ignoresInvalidChannels();
};

void WaveformModelTest::initializesOneHundredChannels()
{
    WaveformModel model(100, 800);

    QCOMPARE(model.channelCount(), 100);
    QCOMPARE(model.samples(0).size(), 0);
}

void WaveformModelTest::capsEachChannelBuffer()
{
    WaveformModel model(100, 3);

    model.appendSamples(0, {1.0, 2.0, 3.0, 4.0});

    QCOMPARE(model.samples(0), QVector<double>({2.0, 3.0, 4.0}));
}

void WaveformModelTest::calculatesPeakAndRms()
{
    WaveformModel model(100, 8);

    model.appendSamples(7, {3.0, 4.0});

    QCOMPARE(model.peak(7), 4.0);
    QVERIFY(qAbs(model.rms(7) - qSqrt(12.5)) < 0.0001);
}

void WaveformModelTest::ignoresInvalidChannels()
{
    WaveformModel model(100, 8);

    model.appendSamples(-1, {1.0});
    model.appendSamples(100, {1.0});

    QVERIFY(model.samples(-1).isEmpty());
    QVERIFY(model.samples(100).isEmpty());
}

QTEST_GUILESS_MAIN(WaveformModelTest)

#include "test_waveformmodel.moc"
