#include "spectrumanalyzer.h"

#include <QTest>
#include <QtMath>

namespace {

constexpr double PI = 3.14159265358979323846;

QVector<double> sineWave(double frequencyHz, int sampleRate, int sampleCount)
{
    QVector<double> samples;
    samples.reserve(sampleCount);
    for (int index = 0; index < sampleCount; ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        samples.append(qSin(2.0 * PI * frequencyHz * time));
    }
    return samples;
}

} // namespace

class SpectrumAnalyzerTest : public QObject
{
    Q_OBJECT

private slots:
    void findsKnownSineFrequency_data();
    void findsKnownSineFrequency();
    void usesMostRecentCappedWindow();
    void rejectsInsufficientOrDegenerateInput();
    void returnsBoundedPlotData();
};

void SpectrumAnalyzerTest::findsKnownSineFrequency_data()
{
    QTest::addColumn<int>("sampleRate");
    QTest::addColumn<double>("frequencyHz");

    for (int sampleRate : {500, 1000})
    {
        for (double frequencyHz : {20.0, 30.0, 80.0})
        {
            const QByteArray rowName = QString("%1Hz-at-%2Hz")
                                           .arg(frequencyHz)
                                           .arg(sampleRate)
                                           .toLatin1();
            QTest::newRow(rowName.constData()) << sampleRate << frequencyHz;
        }
    }
}

void SpectrumAnalyzerTest::findsKnownSineFrequency()
{
    QFETCH(int, sampleRate);
    QFETCH(double, frequencyHz);

    const QVector<double> samples = sineWave(frequencyHz, sampleRate, sampleRate * 5);
    const SpectrumResult result = SpectrumAnalyzer::analyze(samples, sampleRate);

    QVERIFY(result.isValid());
    const double binWidth = static_cast<double>(sampleRate) / 4096.0;
    QVERIFY(qAbs(result.peakFrequencyHz - frequencyHz) <= binWidth);
}

void SpectrumAnalyzerTest::usesMostRecentCappedWindow()
{
    QVector<double> samples(904, 0.0);
    samples += sineWave(37.0, 1000, 4096);

    const SpectrumResult result = SpectrumAnalyzer::analyze(samples, 1000);

    QVERIFY(result.isValid());
    QVERIFY(qAbs(result.peakFrequencyHz - 37.0) <= 1000.0 / 4096.0);
}

void SpectrumAnalyzerTest::rejectsInsufficientOrDegenerateInput()
{
    QVERIFY(!SpectrumAnalyzer::analyze({}, 1000).isValid());
    QVERIFY(!SpectrumAnalyzer::analyze(QVector<double>(255, 0.1), 1000).isValid());
    QVERIFY(!SpectrumAnalyzer::analyze(QVector<double>(1024, 0.1), 0).isValid());
    QVERIFY(!SpectrumAnalyzer::analyze(QVector<double>(1024, 0.1), 1000).isValid());
}

void SpectrumAnalyzerTest::returnsBoundedPlotData()
{
    const SpectrumResult result = SpectrumAnalyzer::analyze(sineWave(30.0, 1000, 4096),
                                                            1000);

    QVERIFY(result.isValid());
    QCOMPARE(result.frequencies.size(), result.magnitudesDb.size());
    QVERIFY(!result.frequencies.isEmpty());
    QVERIFY(result.frequencies.constLast() <= 200.0);
    for (double magnitude : result.magnitudesDb)
    {
        QVERIFY(qIsFinite(magnitude));
        QVERIFY(magnitude >= -60.0);
        QVERIFY(magnitude <= 0.0);
    }
}

QTEST_GUILESS_MAIN(SpectrumAnalyzerTest)

#include "test_spectrumanalyzer.moc"
