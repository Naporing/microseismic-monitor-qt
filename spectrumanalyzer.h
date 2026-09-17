#ifndef SPECTRUMANALYZER_H
#define SPECTRUMANALYZER_H

#include <QVector>

struct SpectrumResult
{
    QVector<double> frequencies;
    QVector<double> magnitudesDb;
    double peakFrequencyHz = 0.0;

    bool isValid() const;
};

class SpectrumAnalyzer
{
public:
    static SpectrumResult analyze(const QVector<double> &samples,
                                  int sampleRate,
                                  int maximumInputSize = 4096,
                                  double maximumFrequencyHz = 200.0);
};

#endif
