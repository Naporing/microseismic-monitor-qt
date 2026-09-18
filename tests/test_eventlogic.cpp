#include "eventlogic.h"

#include <QSet>
#include <QTest>

#include <algorithm>

class EventLogicTest : public QObject
{
    Q_OBJECT

private slots:
    void backgroundIsRepeatableButNotPeriodic();
    void channelsHaveDistinctBoundedBackgrounds();
    void sineSignalIsContinuousAndAccurate_data();
    void sineSignalIsContinuousAndAccurate();
    void sineSignalIsRepeatableAndChannelSpecific();
    void sineChannelGainsStayNearTenPercent();
    void sineSignalRejectsInvalidInput();
    void schedulerIsRepeatableAndSpatial();
    void impulsePropagatesWithDelayAndAttenuation();
    void eventContainsPWaveSWaveAndVariedCoda();
    void eventSustainsTwoDetectionBatches();
    void detectorConfirmsAndRecovers();
};

void EventLogicTest::backgroundIsRepeatableButNotPeriodic()
{
    SeismicSignalGenerator first(100, 16092026);
    SeismicSignalGenerator second(100, 16092026);
    QVector<double> firstWindow;
    QVector<double> secondWindow;

    for (int sample = 0; sample < 400; ++sample)
    {
        const double time = static_cast<double>(sample) / 1000.0;
        const double firstValue = first.sample(0, time, 1000);
        QCOMPARE(firstValue, second.sample(0, time, 1000));
        if (sample < 200)
            firstWindow.append(firstValue);
        else
            secondWindow.append(firstValue);
    }

    QVERIFY(firstWindow != secondWindow);
}

void EventLogicTest::channelsHaveDistinctBoundedBackgrounds()
{
    SeismicSignalGenerator generator(100, 16092026);
    QVector<double> channel0;
    QVector<double> channel1;
    double peak = 0.0;

    for (int sample = 0; sample < 2000; ++sample)
    {
        const double time = static_cast<double>(sample) / 1000.0;
        const double first = generator.sample(0, time, 1000);
        const double second = generator.sample(1, time, 1000);
        channel0.append(first);
        channel1.append(second);
        peak = qMax(peak, qMax(qAbs(first), qAbs(second)));
    }

    QVERIFY(channel0 != channel1);
    QVERIFY(peak > 0.05);
    QVERIFY(peak < 0.50);
}

void EventLogicTest::sineSignalIsContinuousAndAccurate_data()
{
    QTest::addColumn<double>("frequencyHz");

    QTest::newRow("20 Hz") << 20.0;
    QTest::newRow("30 Hz") << 30.0;
    QTest::newRow("80 Hz") << 80.0;
    QTest::newRow("custom 137.5 Hz") << 137.5;
}

void EventLogicTest::sineSignalIsContinuousAndAccurate()
{
    QFETCH(double, frequencyHz);
    constexpr int sampleRate = 1000;
    constexpr int sampleCount = sampleRate * 5;
    SeismicSignalGenerator generator(100, 16092026);

    int risingCrossings = 0;
    double previous = generator.sineSample(0, 0.0, sampleRate, frequencyHz);
    QVector<double> halfSecondEnergy(10, 0.0);
    for (int sample = 1; sample < sampleCount; ++sample)
    {
        const double time = static_cast<double>(sample) / sampleRate;
        const double value = generator.sineSample(0, time, sampleRate, frequencyHz);
        if (previous <= 0.0 && value > 0.0)
            ++risingCrossings;
        halfSecondEnergy[sample / 500] += value * value;
        previous = value;
    }

    const double measuredFrequency = risingCrossings / 5.0;
    QVERIFY(qAbs(measuredFrequency - frequencyHz) < 0.5);
    for (double energy : halfSecondEnergy)
        QVERIFY(energy > 5.0);
}

void EventLogicTest::sineSignalIsRepeatableAndChannelSpecific()
{
    SeismicSignalGenerator first(2, 16092026);
    SeismicSignalGenerator second(2, 16092026);
    QVector<double> channel0;
    QVector<double> channel1;

    for (int sample = 0; sample < 1000; ++sample)
    {
        const double time = static_cast<double>(sample) / 1000.0;
        const double first0 = first.sineSample(0, time, 1000, 30.0);
        const double first1 = first.sineSample(1, time, 1000, 30.0);
        QCOMPARE(first0, second.sineSample(0, time, 1000, 30.0));
        QCOMPARE(first1, second.sineSample(1, time, 1000, 30.0));
        channel0.append(first0);
        channel1.append(first1);
    }

    QVERIFY(channel0 != channel1);
}

void EventLogicTest::sineChannelGainsStayNearTenPercent()
{
    SeismicSignalGenerator generator(100, 16092026);
    QVector<double> energies(100, 0.0);
    for (int sample = 0; sample < 2000; ++sample)
    {
        const double time = sample / 1000.0;
        for (int channel = 0; channel < 100; ++channel)
        {
            const double value = generator.sineSample(channel, time, 1000, 30.0);
            energies[channel] += value * value;
        }
    }

    const auto bounds = std::minmax_element(energies.cbegin(), energies.cend());
    QVERIFY(*bounds.first > 0.0);
    QVERIFY(qSqrt(*bounds.second / *bounds.first) < 1.25);
}

void EventLogicTest::sineSignalRejectsInvalidInput()
{
    SeismicSignalGenerator generator(2, 16092026);

    QCOMPARE(generator.sineSample(-1, 0.0, 1000, 30.0), 0.0);
    QCOMPARE(generator.sineSample(2, 0.0, 1000, 30.0), 0.0);
    QCOMPARE(generator.sineSample(0, 0.0, 0, 30.0), 0.0);
    QCOMPARE(generator.sineSample(0, 0.0, 1000, 0.0), 0.0);
    QCOMPARE(generator.sineSample(0, 0.0, 1000, 201.0), 0.0);
}

void EventLogicTest::schedulerIsRepeatableAndSpatial()
{
    DemoEventScheduler first(100, 20260916);
    DemoEventScheduler second(100, 20260916);

    QSet<int> observedIntervals;
    double previousStart = 0.0;
    for (int eventIndex = 0; eventIndex < 100; ++eventIndex)
    {
        const QVector<int> targets = first.targetsForEvent(eventIndex);
        QCOMPARE(targets, second.targetsForEvent(eventIndex));
        QCOMPARE(first.eventStartTime(eventIndex), second.eventStartTime(eventIndex));
        QVERIFY(targets.size() >= 4);
        QVERIFY(targets.size() <= 9);
        QCOMPARE(QSet<int>(targets.begin(), targets.end()).size(), targets.size());
        const int source = first.sourceChannelForEvent(eventIndex);
        QVERIFY(targets.contains(source));
        const int sourceRow = source / 10;
        const int sourceColumn = source % 10;
        for (int channel : targets)
        {
            QVERIFY(qAbs(channel / 10 - sourceRow) <= 1);
            QVERIFY(qAbs(channel % 10 - sourceColumn) <= 1);
        }

        const double start = first.eventStartTime(eventIndex);
        QVERIFY(start > eventIndex * 5.0 + 0.7);
        QVERIFY(start < eventIndex * 5.0 + 3.3);
        if (eventIndex > 0)
            observedIntervals.insert(qRound((start - previousStart) * 1000.0));
        previousStart = start;
    }
    QVERIFY(observedIntervals.size() > 10);
}

void EventLogicTest::impulsePropagatesWithDelayAndAttenuation()
{
    DemoEventScheduler scheduler(100, 20260916);
    const int eventIndex = 0;
    const QVector<int> targets = scheduler.targetsForEvent(eventIndex);
    const int source = scheduler.sourceChannelForEvent(eventIndex);
    int farTarget = source;
    for (int channel : targets)
    {
        if (scheduler.arrivalTime(channel, eventIndex)
            > scheduler.arrivalTime(farTarget, eventIndex))
            farTarget = channel;
    }

    QVERIFY(farTarget != source);
    const double sourceArrival = scheduler.arrivalTime(source, eventIndex);
    const double farArrival = scheduler.arrivalTime(farTarget, eventIndex);
    QVERIFY(farArrival > sourceArrival);
    QCOMPARE(scheduler.impulse(farTarget, farArrival - 0.001), 0.0);

    double sourcePeak = 0.0;
    double farPeak = 0.0;
    for (int sample = 0; sample < 350; ++sample)
    {
        const double offset = sample / 1000.0;
        sourcePeak = qMax(sourcePeak,
                          qAbs(scheduler.impulse(source, sourceArrival + offset)));
        farPeak = qMax(farPeak,
                       qAbs(scheduler.impulse(farTarget, farArrival + offset)));
    }
    QVERIFY(sourcePeak > 1.0);
    QVERIFY(farPeak > 0.5);
    QVERIFY(sourcePeak > farPeak);
    double codaPeak = 0.0;
    for (int sample = 600; sample < 1000; ++sample)
        codaPeak = qMax(codaPeak,
                        qAbs(scheduler.impulse(source,
                                               sourceArrival + sample / 1000.0)));
    QVERIFY(codaPeak > 0.03);
    QCOMPARE(scheduler.impulse(source, sourceArrival + 1.6), 0.0);

    int nonTarget = 0;
    while (targets.contains(nonTarget))
        ++nonTarget;
    QCOMPARE(scheduler.impulse(nonTarget, scheduler.eventStartTime(eventIndex) + 0.1), 0.0);
}

void EventLogicTest::eventContainsPWaveSWaveAndVariedCoda()
{
    DemoEventScheduler scheduler(100, 20260916);
    QVector<QVector<double>> eventShapes;

    for (int eventIndex = 0; eventIndex < 2; ++eventIndex)
    {
        const int source = scheduler.sourceChannelForEvent(eventIndex);
        const double arrival = scheduler.arrivalTime(source, eventIndex);
        QVector<double> shape;
        double pPeak = 0.0;
        double sPeak = 0.0;
        double earlyCodaPeak = 0.0;
        double lateCodaPeak = 0.0;
        double maximumTransitionStep = 0.0;
        double previousValue = scheduler.impulse(source, arrival + 0.30);
        for (int sample = 0; sample < 1400; ++sample)
        {
            const double offset = sample / 1000.0;
            const double value = scheduler.impulse(source, arrival + offset);
            shape.append(value);
            if (offset < 0.13)
                pPeak = qMax(pPeak, qAbs(value));
            else if (offset < 0.45)
                sPeak = qMax(sPeak, qAbs(value));
            else if (offset < 0.80)
                earlyCodaPeak = qMax(earlyCodaPeak, qAbs(value));
            else if (offset < 1.20)
                lateCodaPeak = qMax(lateCodaPeak, qAbs(value));
            if (offset >= 0.30 && offset <= 0.45)
            {
                maximumTransitionStep = qMax(maximumTransitionStep,
                                             qAbs(value - previousValue));
                previousValue = value;
            }
        }

        QVERIFY(pPeak > 0.10);
        QVERIFY(sPeak > pPeak);
        QVERIFY(earlyCodaPeak > 0.05);
        QVERIFY(lateCodaPeak > 0.01);
        QVERIFY(earlyCodaPeak > lateCodaPeak);
        QVERIFY(maximumTransitionStep < 0.30);
        eventShapes.append(shape);
    }

    QVERIFY(eventShapes[0] != eventShapes[1]);
}

void EventLogicTest::eventSustainsTwoDetectionBatches()
{
    DemoEventScheduler scheduler(100, 20260916);
    const QVector<int> targets = scheduler.targetsForEvent(0);

    for (int sampleRate : {500, 1000})
    {
        const int samplesPerBatch = sampleRate / 50;
        for (int channel : targets)
        {
            int consecutiveBatches = 0;
            int maximumConsecutiveBatches = 0;
            for (int batch = 0; batch < 25; ++batch)
            {
                double peak = 0.0;
                for (int sample = 0; sample < samplesPerBatch; ++sample)
                {
                    const double time = scheduler.eventStartTime(0)
                                        + (batch * samplesPerBatch + sample)
                                              / static_cast<double>(sampleRate);
                    peak = qMax(peak, qAbs(scheduler.impulse(channel, time)));
                }
                consecutiveBatches = peak > 0.78 ? consecutiveBatches + 1 : 0;
                maximumConsecutiveBatches = qMax(maximumConsecutiveBatches,
                                                  consecutiveBatches);
            }
            QVERIFY2(maximumConsecutiveBatches >= 2,
                     qPrintable(QString("channel %1 at %2 Hz only sustained %3 batches")
                                    .arg(channel)
                                    .arg(sampleRate)
                                    .arg(maximumConsecutiveBatches)));
        }
    }
}

void EventLogicTest::detectorConfirmsAndRecovers()
{
    ChannelEventDetector detector(2, 0.78, 2, 50);

    detector.update(0, {0.80});
    QVERIFY(!detector.eventDetected(0));
    detector.update(0, {0.81});
    QVERIFY(detector.eventDetected(0));

    for (int batch = 0; batch < 49; ++batch)
        detector.update(0, {0.20});
    QVERIFY(detector.eventDetected(0));

    detector.update(0, {0.20});
    QVERIFY(!detector.eventDetected(0));
    QVERIFY(!detector.eventDetected(1));
}

QTEST_GUILESS_MAIN(EventLogicTest)

#include "test_eventlogic.moc"
