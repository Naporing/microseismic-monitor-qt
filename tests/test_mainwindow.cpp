#include "mainwindow.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QScrollArea>
#include <QTest>
#include <QTimer>
#include <QVariant>
#include <QtMath>

class MainWindowTest : public QObject
{
    Q_OBJECT

private slots:
    void containsWaveformListAndOneTimer();
    void keepsFiftyChannelsInViewport();
    void oneTickUpdatesEveryChannel();
    void usesRealisticSeededGenerator();
    void keepsFiveSecondsOfSamples();
    void changingSampleRateClearsAndResizesBuffers();
    void changingSampleRateKeepsNoiseStateContinuous();
    void exposesSimulationModeControls();
    void routesSinePresetToAllChannels();
    void routesCustomSineFrequency();
    void detectsScheduledEventFromSamples();
    void detectsScheduledEventAtFiveHundredHertz();
    void exposesScientificInstrumentDesign();
    void usesScientificStatusColors();
};

void MainWindowTest::containsWaveformListAndOneTimer()
{
    MainWindow window;

    QVERIFY(window.findChild<WaveformListWidget *>("waveformListWidget"));
    QVERIFY(!window.findChild<QWidget *>("detailWaveformWidget"));
    QCOMPARE(window.findChildren<QTimer *>().size(), 1);
    QCOMPARE(window.model()->channelCount(), 100);
}

void MainWindowTest::keepsFiftyChannelsInViewport()
{
    MainWindow window;
    window.show();

    auto *scrollArea = window.findChild<QScrollArea *>("monitorScroll");
    auto *list = window.findChild<WaveformListWidget *>("waveformListWidget");
    QVERIFY(scrollArea);
    QVERIFY(list);

    for (const QSize &windowSize : {QSize(1080, 680), QSize(1440, 901), QSize(1600, 1000)})
    {
        window.resize(windowSize);
        QTest::qWait(20);

        const int viewportHeight = scrollArea->viewport()->height();
        QCOMPARE(list->height(), viewportHeight * 2);
        QCOMPARE(list->channelAtY(viewportHeight - 1), 49);
        QCOMPARE(list->channelAtY(viewportHeight), 50);
    }
}

void MainWindowTest::oneTickUpdatesEveryChannel()
{
    MainWindow window;

    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));

    for (int channel = 0; channel < 100; ++channel)
        QCOMPARE(window.model()->samples(channel).size(), 20);
}

void MainWindowTest::usesRealisticSeededGenerator()
{
    MainWindow window;
    SeismicSignalGenerator expectedGenerator(100, 16092026);
    DemoEventScheduler expectedEvents(100, 20260916);
    QVector<double> expected;

    for (int point = 0; point < 20; ++point)
    {
        const double time = static_cast<double>(point) / 1000.0;
        expected.append(expectedGenerator.sample(0, time, 1000)
                        + expectedEvents.impulse(0, time));
    }

    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));
    QCOMPARE(window.model()->samples(0), expected);
}

void MainWindowTest::keepsFiveSecondsOfSamples()
{
    MainWindow window;

    for (int tick = 0; tick < 251; ++tick)
        QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));

    QCOMPARE(window.model()->maxPoints(), 5000);
    QCOMPARE(window.model()->samples(0).size(), 5000);
}

void MainWindowTest::changingSampleRateClearsAndResizesBuffers()
{
    MainWindow window;
    auto *sampleRateBox = window.findChild<QComboBox *>("sampleRateBox");

    QVERIFY(sampleRateBox);
    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));
    QVERIFY(!window.model()->samples(0).isEmpty());

    sampleRateBox->setCurrentIndex(1);

    QCOMPARE(window.model()->maxPoints(), 2500);
    QVERIFY(window.model()->samples(0).isEmpty());
}

void MainWindowTest::changingSampleRateKeepsNoiseStateContinuous()
{
    MainWindow window;
    auto *sampleRateBox = window.findChild<QComboBox *>("sampleRateBox");
    SeismicSignalGenerator expectedGenerator(100, 16092026);
    DemoEventScheduler expectedEvents(100, 20260916);

    for (int point = 0; point < 20; ++point)
        expectedGenerator.sample(0, static_cast<double>(point) / 1000.0, 1000);

    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));
    sampleRateBox->setCurrentIndex(1);
    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));

    QVector<double> expected;
    for (int point = 0; point < 10; ++point)
    {
        const double time = 0.02 + static_cast<double>(point) / 500.0;
        expected.append(expectedGenerator.sample(0, time, 500)
                        + expectedEvents.impulse(0, time));
    }
    QCOMPARE(window.model()->samples(0), expected);
}

void MainWindowTest::exposesSimulationModeControls()
{
    MainWindow window;
    auto *modeBox = window.findChild<QComboBox *>("signalModeBox");
    auto *frequencyBox = window.findChild<QComboBox *>("frequencyPresetBox");
    auto *customFrequency = window.findChild<QDoubleSpinBox *>("customFrequencySpin");

    QVERIFY(modeBox);
    QVERIFY(frequencyBox);
    QVERIFY(customFrequency);
    QCOMPARE(modeBox->count(), 2);
    QCOMPARE(modeBox->itemText(0), QString("地震模拟"));
    QCOMPARE(modeBox->itemText(1), QString("正弦测试"));
    QCOMPARE(frequencyBox->count(), 4);
    QCOMPARE(frequencyBox->itemText(0), QString("20 Hz"));
    QCOMPARE(frequencyBox->itemText(1), QString("30 Hz"));
    QCOMPARE(frequencyBox->itemText(2), QString("80 Hz"));
    QCOMPARE(frequencyBox->itemText(3), QString("自定义"));
    QCOMPARE(customFrequency->minimum(), 1.0);
    QCOMPARE(customFrequency->maximum(), 200.0);
    QVERIFY(!frequencyBox->isEnabled());
    QVERIFY(!customFrequency->isEnabled());

    modeBox->setCurrentIndex(1);
    QVERIFY(frequencyBox->isEnabled());
    QVERIFY(!customFrequency->isEnabled());
    frequencyBox->setCurrentIndex(3);
    QVERIFY(customFrequency->isEnabled());
    QCOMPARE(window.findChildren<QTimer *>().size(), 1);
}

void MainWindowTest::routesSinePresetToAllChannels()
{
    MainWindow window;
    auto *modeBox = window.findChild<QComboBox *>("signalModeBox");
    auto *frequencyBox = window.findChild<QComboBox *>("frequencyPresetBox");
    QVERIFY(modeBox);
    QVERIFY(frequencyBox);

    modeBox->setCurrentIndex(1);
    frequencyBox->setCurrentIndex(1);
    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));

    SeismicSignalGenerator expectedGenerator(100, 16092026);
    QVector<double> expected;
    for (int point = 0; point < 20; ++point)
        expected.append(expectedGenerator.sineSample(0,
                                                     point / 1000.0,
                                                     1000,
                                                     30.0));
    QCOMPARE(window.model()->samples(0), expected);
    for (int channel = 0; channel < 100; ++channel)
        QCOMPARE(window.model()->samples(channel).size(), 20);
}

void MainWindowTest::routesCustomSineFrequency()
{
    MainWindow window;
    auto *modeBox = window.findChild<QComboBox *>("signalModeBox");
    auto *frequencyBox = window.findChild<QComboBox *>("frequencyPresetBox");
    auto *customFrequency = window.findChild<QDoubleSpinBox *>("customFrequencySpin");
    QVERIFY(modeBox);
    QVERIFY(frequencyBox);
    QVERIFY(customFrequency);

    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));
    modeBox->setCurrentIndex(1);
    frequencyBox->setCurrentIndex(3);
    customFrequency->setValue(137.5);
    QVERIFY(window.model()->samples(0).isEmpty());
    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));

    SeismicSignalGenerator expectedGenerator(100, 16092026);
    QVector<double> expected;
    for (int point = 0; point < 20; ++point)
        expected.append(expectedGenerator.sineSample(0,
                                                     point / 1000.0,
                                                     1000,
                                                     137.5));
    QCOMPARE(window.model()->samples(0), expected);
}

void MainWindowTest::detectsScheduledEventFromSamples()
{
    MainWindow window;
    const DemoEventScheduler scheduler(100, 20260916);
    const QVector<int> eventChannels = scheduler.targetsForEvent(0);
    int nonEventChannel = 0;
    while (eventChannels.contains(nonEventChannel))
        ++nonEventChannel;

    const int eventTicks = qCeil((scheduler.eventStartTime(0) + 0.55) * 50.0);
    for (int tick = 0; tick < eventTicks; ++tick)
        QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));

    for (int channel : eventChannels)
        QVERIFY(window.model()->eventDetected(channel));
    QVERIFY(!window.model()->eventDetected(nonEventChannel));

    for (int tick = 0; tick < 100; ++tick)
        QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));
    for (int channel : eventChannels)
        QVERIFY(!window.model()->eventDetected(channel));
}

void MainWindowTest::detectsScheduledEventAtFiveHundredHertz()
{
    MainWindow window;
    auto *sampleRateBox = window.findChild<QComboBox *>("sampleRateBox");
    const DemoEventScheduler scheduler(100, 20260916);
    const int eventChannel = scheduler.targetsForEvent(0).first();

    QVERIFY(sampleRateBox);
    sampleRateBox->setCurrentIndex(1);
    const int eventTicks = qCeil((scheduler.eventStartTime(0) + 0.55) * 50.0);
    for (int tick = 0; tick < eventTicks; ++tick)
        QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));

    QCOMPARE(window.model()->maxPoints(), 2500);
    QVERIFY(window.model()->eventDetected(eventChannel));
}

void MainWindowTest::exposesScientificInstrumentDesign()
{
    MainWindow window;

    QCOMPARE(window.centralWidget()->property("designVariant").toString(), QString("B"));
    QCOMPARE(window.windowTitle(), QString("微震阵列实时监测 · 科研仪器台"));
    QVERIFY(window.findChild<QWidget *>("instrumentHeader"));
    QVERIFY(window.findChild<QWidget *>("readoutStrip"));
    QVERIFY(window.findChild<QWidget *>("amplitudeScale"));
    QVERIFY(window.styleSheet().contains("QLabel#subtitle { color: #5D6F7C;"));
}

void MainWindowTest::usesScientificStatusColors()
{
    MainWindow window;
    auto *status = window.findChild<QLabel *>("statusValue");
    QVERIFY(status);

    QVERIFY(QMetaObject::invokeMethod(&window, "startAcquisition", Qt::DirectConnection));
    QVERIFY(status->styleSheet().contains("#16805B"));
    QVERIFY(QMetaObject::invokeMethod(&window, "stopAcquisition", Qt::DirectConnection));
    QVERIFY(status->styleSheet().contains("#A84F0A"));
}

QTEST_MAIN(MainWindowTest)

#include "test_mainwindow.moc"
