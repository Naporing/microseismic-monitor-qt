#include "waveformmodel.h"

#include <QtMath>

WaveformModel::WaveformModel(int channelCount, int maxPoints)
    : m_maxPoints(qMax(1, maxPoints))
    , m_channels(qMax(0, channelCount))
    , m_eventDetected(qMax(0, channelCount), false)
{
    for (ChannelBuffer &buffer : m_channels)
        buffer.storage.resize(m_maxPoints);
}

int WaveformModel::channelCount() const
{
    return m_channels.size();
}

int WaveformModel::maxPoints() const
{
    return m_maxPoints;
}

void WaveformModel::setMaxPoints(int maxPoints)
{
    const int newMaximum = qMax(1, maxPoints);
    if (newMaximum == m_maxPoints)
        return;

    for (int channel = 0; channel < m_channels.size(); ++channel)
    {
        const QVector<double> snapshot = samples(channel);
        ChannelBuffer &buffer = m_channels[channel];
        buffer.storage.resize(newMaximum);
        buffer.start = 0;
        buffer.size = qMin(snapshot.size(), newMaximum);
        const int sourceStart = snapshot.size() - buffer.size;
        for (int index = 0; index < buffer.size; ++index)
            buffer.storage[index] = snapshot[sourceStart + index];
        buffer.dirty = true;
    }
    m_maxPoints = newMaximum;
}

void WaveformModel::clear()
{
    for (ChannelBuffer &buffer : m_channels)
    {
        buffer.start = 0;
        buffer.size = 0;
        buffer.view.clear();
        buffer.dirty = false;
    }
    m_eventDetected.fill(false);
}

const QVector<double> &WaveformModel::samples(int channel) const
{
    static const QVector<double> empty;
    if (!isValidChannel(channel))
        return empty;

    ChannelBuffer &buffer = m_channels[channel];
    if (buffer.dirty)
    {
        buffer.view.resize(buffer.size);
        for (int index = 0; index < buffer.size; ++index)
            buffer.view[index] = buffer.storage[(buffer.start + index) % m_maxPoints];
        buffer.dirty = false;
    }
    return buffer.view;
}

void WaveformModel::appendSamples(int channel, const QVector<double> &values)
{
    if (!isValidChannel(channel) || values.isEmpty())
        return;

    ChannelBuffer &buffer = m_channels[channel];
    if (values.size() >= m_maxPoints)
    {
        const int sourceStart = values.size() - m_maxPoints;
        for (int index = 0; index < m_maxPoints; ++index)
            buffer.storage[index] = values[sourceStart + index];
        buffer.start = 0;
        buffer.size = m_maxPoints;
        buffer.dirty = true;
        return;
    }

    for (double value : values)
    {
        if (buffer.size < m_maxPoints)
        {
            buffer.storage[(buffer.start + buffer.size) % m_maxPoints] = value;
            ++buffer.size;
        }
        else
        {
            buffer.storage[buffer.start] = value;
            buffer.start = (buffer.start + 1) % m_maxPoints;
        }
    }
    buffer.dirty = true;
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

void WaveformModel::setEventDetected(int channel, bool detected)
{
    if (isValidChannel(channel))
        m_eventDetected[channel] = detected;
}

bool WaveformModel::eventDetected(int channel) const
{
    return isValidChannel(channel) && m_eventDetected[channel];
}

bool WaveformModel::isValidChannel(int channel) const
{
    return channel >= 0 && channel < m_channels.size();
}
