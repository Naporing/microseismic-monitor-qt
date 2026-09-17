#include "waveformmodel.h"

#include <QTest>
#include <QtMath>

class WaveformModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initializesOneHundredChannels();
    void capsEachChannelBuffer();
    void keepsChronologicalOrderAcrossRepeatedWraps();
    void calculatesPeakAndRms();
    void ignoresInvalidChannels();
    void resizesAndClearsChannelBuffers();
    void storesAndClearsEventState();
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

void WaveformModelTest::keepsChronologicalOrderAcrossRepeatedWraps()
{
    WaveformModel model(2, 5);

    model.appendSamples(0, {1.0, 2.0});
    model.appendSamples(0, {3.0, 4.0});
    model.appendSamples(0, {5.0, 6.0});
    QCOMPARE(model.samples(0), QVector<double>({2.0, 3.0, 4.0, 5.0, 6.0}));

    model.appendSamples(0, {7.0, 8.0, 9.0});
    QCOMPARE(model.samples(0), QVector<double>({5.0, 6.0, 7.0, 8.0, 9.0}));
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

void WaveformModelTest::resizesAndClearsChannelBuffers()
{
    WaveformModel model(2, 4);
    model.appendSamples(0, {1.0, 2.0, 3.0, 4.0});

    model.setMaxPoints(2);
    QCOMPARE(model.maxPoints(), 2);
    QCOMPARE(model.samples(0), QVector<double>({3.0, 4.0}));

    model.clear();
    QVERIFY(model.samples(0).isEmpty());
}

void WaveformModelTest::storesAndClearsEventState()
{
    WaveformModel model(2, 4);

    model.setEventDetected(1, true);
    QVERIFY(model.eventDetected(1));
    QVERIFY(!model.eventDetected(0));

    model.clear();
    QVERIFY(!model.eventDetected(1));
}

QTEST_GUILESS_MAIN(WaveformModelTest)

#include "test_waveformmodel.moc"
