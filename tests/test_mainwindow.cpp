#include "mainwindow.h"
#include "appversion.h"
#include "updatemanager.h"
#include "fakenetwork.h"

#include <QApplication>
#include <QComboBox>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTest>
#include <QTimer>
#include <QVariant>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QSignalSpy>
#include <QtMath>

namespace {

bool activateComboItem(QComboBox *comboBox, int index)
{
    comboBox->setCurrentIndex(index);
    return QMetaObject::invokeMethod(comboBox,
                                     "activated",
                                     Qt::DirectConnection,
                                     Q_ARG(int, index));
}

}

class MainWindowTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesUpdateControls();
    void handlesUpdateDialogsAndSilentErrors();
    void exitsOnlyAfterInstallerLaunchSucceeds();
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
    void cancelingCustomFrequencyKeepsPreviousSelection();
    void detectsScheduledEventFromSamples();
    void detectsScheduledEventAtFiveHundredHertz();
    void exposesScientificInstrumentDesign();
    void usesScientificStatusColors();
    void opensSwitchesAndClosesDetailDrawer();
    void reportsSelectedChannelSpectrum();
};

void MainWindowTest::exposesUpdateControls()
{
    FakeNetwork network;
    UpdateManager manager(nullptr, &network);
    MainWindow window(nullptr, &manager);
    auto *version = window.findChild<QLabel *>("currentVersionLabel");
    auto *button = window.findChild<QPushButton *>("checkUpdateButton");
    QVERIFY(version);
    QVERIFY(button);
    QCOMPARE(version->text(), QString("版本 v" APP_VERSION));
    QCOMPARE(button->text(), QString("检查更新"));
    button->click();
    QVERIFY(!button->isEnabled());
    QCOMPARE(button->text(), QString("检查中…"));
    manager.cancel();
    QVERIFY(button->isEnabled());
}

void MainWindowTest::handlesUpdateDialogsAndSilentErrors()
{
    FakeNetwork network;
    UpdateManager manager(nullptr, &network);
    MainWindow window(nullptr, &manager);
    window.show();
    manager.failed("静默错误", true);
    QVERIFY(!window.findChild<QMessageBox *>("updateMessage"));
    manager.failed("手动错误", false);
    auto *message = window.findChild<QMessageBox *>("updateMessage");
    QVERIFY(message);
    QCOMPARE(message->text(), QString("手动错误"));
    message->close();
    ReleaseInfo release;
    release.tagName = "v1.2.0";
    release.notes = "改进波形显示\n<script>这是纯文本</script>";
    manager.updateAvailable(release);
    auto *dialog = window.findChild<QDialog *>("updateAvailableDialog");
    QVERIFY(dialog);
    QVERIFY(dialog->windowTitle().contains("v1.2.0"));
    QCOMPARE(dialog->findChild<QPlainTextEdit *>()->toPlainText(), release.notes);
    auto *buttons = dialog->findChild<QDialogButtonBox *>();
    QCOMPARE(buttons->button(QDialogButtonBox::Yes)->text(), QString("立即更新"));
    QCOMPARE(buttons->button(QDialogButtonBox::No)->text(), QString("稍后"));
    buttons->button(QDialogButtonBox::No)->click();
    QVERIFY(window.findChild<QPushButton *>("checkUpdateButton")->isEnabled());
}

void MainWindowTest::exitsOnlyAfterInstallerLaunchSucceeds()
{
    for (bool success : {false, true})
    {
        FakeNetwork network;
        UpdateManager manager(nullptr, &network);
        QString launchedPath;
        QStringList launchedArguments;
        MainWindow window(nullptr, &manager, [&](const QString &path, const QStringList &args) {
            launchedPath = path;
            launchedArguments = args;
            return success;
        });
        QSignalSpy restart(&window, &MainWindow::restartRequested);
        QVERIFY(QMetaObject::invokeMethod(&window, "startAcquisition"));
        manager.readyToInstall("C:/fake/verified-setup.exe");
        QTRY_COMPARE(launchedPath, QString("C:/fake/verified-setup.exe"));
        QVERIFY(launchedArguments.contains("/VERYSILENT"));
        QVERIFY(launchedArguments.contains("/UPDATE=1"));
        QCOMPARE(restart.size(), success ? 1 : 0);
        QCOMPARE(window.findChild<QTimer *>()->isActive(), !success);
        QCOMPARE(window.findChild<QMessageBox *>("updateMessage") != nullptr, !success);
    }
}

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
    auto *customFrequency = window.findChild<QWidget *>("customFrequencySpin");

    QVERIFY(modeBox);
    QVERIFY(frequencyBox);
    QVERIFY(!customFrequency);
    QCOMPARE(modeBox->count(), 2);
    QCOMPARE(modeBox->itemText(0), QString("地震模拟"));
    QCOMPARE(modeBox->itemText(1), QString("正弦测试"));
    QCOMPARE(frequencyBox->count(), 4);
    QCOMPARE(frequencyBox->itemText(0), QString("20 Hz"));
    QCOMPARE(frequencyBox->itemText(1), QString("30 Hz"));
    QCOMPARE(frequencyBox->itemText(2), QString("80 Hz"));
    QCOMPARE(frequencyBox->itemText(3), QString("自定义"));
    QVERIFY(!frequencyBox->isEnabled());

    modeBox->setCurrentIndex(1);
    QVERIFY(frequencyBox->isEnabled());
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
    QVERIFY(activateComboItem(frequencyBox, 1));
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
    QVERIFY(modeBox);
    QVERIFY(frequencyBox);

    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));
    modeBox->setCurrentIndex(1);
    bool dialogOpened = false;
    QTimer::singleShot(0, [&dialogOpened]() {
        auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        dialogOpened = true;
        dialog->setDoubleValue(137.5);
        dialog->accept();
    });
    QVERIFY(activateComboItem(frequencyBox, 3));
    QVERIFY(dialogOpened);
    QCOMPARE(frequencyBox->itemText(3), QString("自定义 137.5 Hz"));
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

void MainWindowTest::cancelingCustomFrequencyKeepsPreviousSelection()
{
    MainWindow window;
    auto *modeBox = window.findChild<QComboBox *>("signalModeBox");
    auto *frequencyBox = window.findChild<QComboBox *>("frequencyPresetBox");
    QVERIFY(modeBox);
    QVERIFY(frequencyBox);

    modeBox->setCurrentIndex(1);
    QVERIFY(activateComboItem(frequencyBox, 1));
    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));
    const QVector<double> samplesBeforeDialog = window.model()->samples(0);

    bool dialogOpened = false;
    QTimer::singleShot(0, [&dialogOpened]() {
        auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        dialogOpened = true;
        dialog->reject();
    });
    QVERIFY(activateComboItem(frequencyBox, 3));

    QVERIFY(dialogOpened);
    QCOMPARE(frequencyBox->currentIndex(), 1);
    QCOMPARE(frequencyBox->itemText(3), QString("自定义"));
    QCOMPARE(window.model()->samples(0), samplesBeforeDialog);
}

void MainWindowTest::detectsScheduledEventFromSamples()
{
    MainWindow window;
    const DemoEventScheduler scheduler(100, 20260916);
    const QVector<int> eventChannels = scheduler.targetsForEvent(0);
    int nonEventChannel = 0;
    while (eventChannels.contains(nonEventChannel))
        ++nonEventChannel;

    const int eventTicks = qCeil((scheduler.eventStartTime(0) + 0.75) * 50.0);
    for (int tick = 0; tick < eventTicks; ++tick)
        QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));

    for (int channel : eventChannels)
    {
        QVERIFY2(window.model()->eventDetected(channel),
                 qPrintable(QString("channel %1 peak %2 was not detected")
                                .arg(channel)
                                .arg(window.model()->peak(channel))));
    }
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
    const int eventTicks = qCeil((scheduler.eventStartTime(0) + 0.75) * 50.0);
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

void MainWindowTest::opensSwitchesAndClosesDetailDrawer()
{
    MainWindow window;
    window.show();
    QTest::qWait(20);
    auto *splitter = window.findChild<QSplitter *>("monitorSplitter");
    auto *panel = window.findChild<QWidget *>("channelDetailPanel");
    auto *list = window.findChild<WaveformListWidget *>("waveformListWidget");
    auto *channelLabel = window.findChild<QLabel *>("detailChannelLabel");
    auto *frequencyLabel = window.findChild<QLabel *>("detailFrequencyLabel");
    auto *visibleChannelBadge = window.findChild<QLabel *>("visibleChannelBadge");
    auto *closeButton = window.findChild<QPushButton *>("detailCloseButton");

    QVERIFY(splitter);
    QVERIFY(panel);
    QVERIFY(list);
    QVERIFY(channelLabel);
    QVERIFY(frequencyLabel);
    QVERIFY(visibleChannelBadge);
    QVERIFY(closeButton);
    QVERIFY(panel->isHidden());
    const int timerCount = window.findChildren<QTimer *>().size();

    const int channel36Y = qFloor((36.5) * list->rowHeight());
    QTest::mouseDClick(list,
                       Qt::LeftButton,
                       Qt::NoModifier,
                       QPoint(200, channel36Y));
    QTRY_VERIFY(panel->isVisible());
    QCOMPARE(list->selectedChannel(), 36);
    QCOMPARE(list->visibleRows(), 34);
    QVERIFY(channelLabel->text().contains("CH-037"));
    QVERIFY(frequencyLabel->text().contains("--"));
    QVERIFY(visibleChannelBadge->text().contains("34 路"));

    const int channel8Y = qFloor((8.5) * list->rowHeight());
    QTest::mouseClick(list,
                      Qt::LeftButton,
                      Qt::NoModifier,
                      QPoint(200, channel8Y));
    QCOMPARE(list->selectedChannel(), 8);
    QVERIFY(channelLabel->text().contains("CH-009"));

    QTest::mouseClick(closeButton, Qt::LeftButton);
    QVERIFY(panel->isHidden());
    QCOMPARE(list->visibleRows(), 50);
    QVERIFY(visibleChannelBadge->text().contains("50 路"));
    QCOMPARE(window.findChildren<QTimer *>().size(), timerCount);
}

void MainWindowTest::reportsSelectedChannelSpectrum()
{
    MainWindow window;
    auto *modeBox = window.findChild<QComboBox *>("signalModeBox");
    auto *frequencyBox = window.findChild<QComboBox *>("frequencyPresetBox");
    auto *list = window.findChild<WaveformListWidget *>("waveformListWidget");
    auto *panel = window.findChild<QWidget *>("channelDetailPanel");
    auto *frequencyLabel = window.findChild<QLabel *>("detailFrequencyLabel");
    auto *plot = window.findChild<QWidget *>("channelAnalysisPlot");
    QVERIFY(modeBox);
    QVERIFY(frequencyBox);
    QVERIFY(list);
    QVERIFY(panel);
    QVERIFY(frequencyLabel);
    QVERIFY(plot);

    modeBox->setCurrentIndex(1);
    QVERIFY(activateComboItem(frequencyBox, 1));
    for (int tick = 0; tick < 251; ++tick)
        QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));

    window.show();
    QTest::qWait(20);
    QTest::mouseDClick(list, Qt::LeftButton, Qt::NoModifier, QPoint(200, 6));
    QTRY_VERIFY(panel->isVisible());
    QTRY_VERIFY(frequencyLabel->text().contains("30"));
    QVERIFY(plot->height() >= 180);
    QVERIFY(plot->width() >= 700);

    QImage image(plot->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    plot->render(&image);
    int timeTracePixels = 0;
    int spectrumTracePixels = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor color = image.pixelColor(x, y);
            if (color.red() < 80 && color.green() > 70 && color.blue() > 90)
            {
                if (y < image.height() / 2)
                    ++timeTracePixels;
                else
                    ++spectrumTracePixels;
            }
        }
    }
    QVERIFY(timeTracePixels > 10);
    QVERIFY(spectrumTracePixels > 10);
}

QTEST_MAIN(MainWindowTest)

#include "test_mainwindow.moc"
