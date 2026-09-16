#include "mainwindow.h"

#include <QSignalSpy>
#include <QTest>

class WaveformWidgetsTest : public QObject
{
    Q_OBJECT

private slots:
    void mapsPointToChannel();
    void ignoresPointsOutsideGrid();
    void emitsSelectedChannelOnClick();
};

void WaveformWidgetsTest::mapsPointToChannel()
{
    WaveformModel model(100, 800);
    OverviewWidget overview(&model);
    overview.resize(1000, 1000);

    QCOMPARE(overview.channelAt(QPoint(50, 50)), 0);
    QCOMPARE(overview.channelAt(QPoint(950, 950)), 99);
}

void WaveformWidgetsTest::ignoresPointsOutsideGrid()
{
    WaveformModel model(100, 800);
    OverviewWidget overview(&model);
    overview.resize(1000, 1000);

    QCOMPARE(overview.channelAt(QPoint(-1, 20)), -1);
    QCOMPARE(overview.channelAt(QPoint(1000, 20)), -1);
}

void WaveformWidgetsTest::emitsSelectedChannelOnClick()
{
    WaveformModel model(100, 800);
    OverviewWidget overview(&model);
    overview.resize(1000, 1000);
    QSignalSpy spy(&overview, &OverviewWidget::channelSelected);

    QTest::mouseClick(&overview, Qt::LeftButton, Qt::NoModifier, QPoint(250, 350));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toInt(), 32);
}

QTEST_MAIN(WaveformWidgetsTest)

#include "test_waveformwidgets.moc"
