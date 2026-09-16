#ifndef WAVEFORMMODEL_H
#define WAVEFORMMODEL_H

#include <QVector>

class WaveformModel
{
public:
    explicit WaveformModel(int channelCount = 100, int maxPoints = 800);

    int channelCount() const;
    int maxPoints() const;
    const QVector<double> &samples(int channel) const;
    void appendSamples(int channel, const QVector<double> &values);
    double peak(int channel) const;
    double rms(int channel) const;

private:
    bool isValidChannel(int channel) const;

    QVector<QVector<double>> m_channels;
    int m_maxPoints;
};

#endif
