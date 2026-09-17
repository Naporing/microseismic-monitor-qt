#include "eventlogic.h"

#include <QSet>
#include <QTest>

class EventLogicTest : public QObject
{
    Q_OBJECT

private slots:
    void backgroundIsRepeatableButNotPeriodic();
    void channelsHaveDistinctBoundedBackgrounds();
    void schedulerIsRepeatableAndSpatial();
    void impulsePropagatesWithDelayAndAttenuation();
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
    QVERIFY(qAbs(scheduler.impulse(source, sourceArrival + 0.319)) < 0.03);
    QCOMPARE(scheduler.impulse(source, sourceArrival + 0.320), 0.0);

    int nonTarget = 0;
    while (targets.contains(nonTarget))
        ++nonTarget;
    QCOMPARE(scheduler.impulse(nonTarget, scheduler.eventStartTime(eventIndex) + 0.1), 0.0);
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
