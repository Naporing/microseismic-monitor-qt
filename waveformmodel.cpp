#include "waveformmodel.h"

#include <QtMath>

WaveformModel::WaveformModel(int channelCount, int maxPoints)
    : m_channels(qMax(0, channelCount))
    , m_maxPoints(qMax(1, maxPoints))
{
}

int WaveformModel::channelCount() const
{
    return m_channels.size();
}

int WaveformModel::maxPoints() const
{
    return m_maxPoints;
}

const QVector<double> &WaveformModel::samples(int channel) const
{
    static const QVector<double> empty;
    return isValidChannel(channel) ? m_channels[channel] : empty;
}

void WaveformModel::appendSamples(int channel, const QVector<double> &values)
{
    if (!isValidChannel(channel) || values.isEmpty())
        return;

    QVector<double> &buffer = m_channels[channel];
    buffer.append(values);
    const int overflow = buffer.size() - m_maxPoints;
    if (overflow > 0)
        buffer.remove(0, overflow);
}

double WaveformModel::peak(int channel) const
{
    double result = 0.0;
    for (double value : samples(channel))
        result = qMax(result, qAbs(value));
    return result;
}

double WaveformModel::rms(int channel) const
{
    const QVector<double> &buffer = samples(channel);
    if (buffer.isEmpty())
        return 0.0;

    double squareSum = 0.0;
    for (double value : buffer)
        squareSum += value * value;
    return qSqrt(squareSum / buffer.size());
}

bool WaveformModel::isValidChannel(int channel) const
{
    return channel >= 0 && channel < m_channels.size();
}
