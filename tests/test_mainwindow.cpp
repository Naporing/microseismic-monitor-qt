#include "mainwindow.h"

#include <QLabel>
#include <QTest>

class MainWindowTest : public QObject
{
    Q_OBJECT

private slots:
    void containsOverviewAndDetailPanels();
    void oneTickUpdatesEveryChannel();
    void selectingChannelUpdatesDetailTitle();
};

void MainWindowTest::containsOverviewAndDetailPanels()
{
    MainWindow window;

    QVERIFY(window.findChild<OverviewWidget *>("overviewWidget"));
    QVERIFY(window.findChild<DetailWaveformWidget *>("detailWaveformWidget"));
    QCOMPARE(window.model()->channelCount(), 100);
}

void MainWindowTest::oneTickUpdatesEveryChannel()
{
    MainWindow window;

    QVERIFY(QMetaObject::invokeMethod(&window, "generateData", Qt::DirectConnection));

    for (int channel = 0; channel < 100; ++channel)
        QCOMPARE(window.model()->samples(channel).size(), 20);
}

void MainWindowTest::selectingChannelUpdatesDetailTitle()
{
    MainWindow window;
    auto *overview = window.findChild<OverviewWidget *>("overviewWidget");
    auto *title = window.findChild<QLabel *>("detailChannelLabel");

    QVERIFY(overview);
    QVERIFY(title);
    QVERIFY(QMetaObject::invokeMethod(overview,
                                      "channelSelected",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 99)));
    QCOMPARE(title->text(), QString("CH-100"));
}

QTEST_MAIN(MainWindowTest)

#include "test_mainwindow.moc"
