#ifndef WAVEFORMMODEL_H
#define WAVEFORMMODEL_H

#include <QVector>

class WaveformModel
{
public:
    explicit WaveformModel(int channelCount = 100, int maxPoints = 800);

    int channelCount() const;
    int maxPoints() const;
    void setMaxPoints(int maxPoints);
    void clear();
    const QVector<double> &samples(int channel) const;
    void appendSamples(int channel, const QVector<double> &values);
    double peak(int channel) const;
    double rms(int channel) const;
    void setEventDetected(int channel, bool detected);
    bool eventDetected(int channel) const;

private:
    struct ChannelBuffer
    {
        QVector<double> storage;
        int start = 0;
        int size = 0;
        mutable QVector<double> view;
        mutable bool dirty = true;
    };

    bool isValidChannel(int channel) const;

    int m_maxPoints;
    mutable QVector<ChannelBuffer> m_channels;
    QVector<bool> m_eventDetected;
};

#endif
