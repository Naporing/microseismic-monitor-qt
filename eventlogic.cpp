#include "eventlogic.h"

#include <QtMath>

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double EVENT_SLOT_SECONDS = 5.0;
constexpr double ARRIVAL_DELAY_PER_SENSOR_SECONDS = 0.018;

quint32 mixBits(quint32 value)
{
    value ^= value >> 16;
    value *= 0x7FEB352DU;
    value ^= value >> 15;
    value *= 0x846CA68BU;
    return value ^ (value >> 16);
}

double unitValue(quint32 value)
{
    return static_cast<double>(mixBits(value)) / 4294967295.0;
}

}

SeismicSignalGenerator::SeismicSignalGenerator(int channelCount, quint32 seed)
    : m_seed(seed)
    , m_randomStates(qMax(0, channelCount))
    , m_coloredNoise(qMax(0, channelCount), 0.0)
    , m_lowNoise(qMax(0, channelCount), 0.0)
    , m_previousWhite(qMax(0, channelCount), 0.0)
    , m_gain(qMax(0, channelCount))
    , m_noiseScale(qMax(0, channelCount))
    , m_phase(qMax(0, channelCount))
    , m_humCoupling(qMax(0, channelCount))
{
    for (int channel = 0; channel < m_randomStates.size(); ++channel)
    {
        const quint32 channelSeed = mixBits(seed + static_cast<quint32>(channel + 1) * 2654435761U);
        m_gain[channel] = 0.82 + 0.36 * unitValue(channelSeed + 1U);
        m_noiseScale[channel] = 0.85 + 0.30 * unitValue(channelSeed + 2U);
        m_phase[channel] = 2.0 * PI * unitValue(channelSeed + 3U);
        m_humCoupling[channel] = 0.004 + 0.010 * unitValue(channelSeed + 4U);
    }
    reset();
}

double SeismicSignalGenerator::sample(int channel, double timeSeconds, int sampleRate)
{
    if (!isValidChannel(channel) || sampleRate <= 0)
        return 0.0;

    const double white = nextNoise(channel);
    const double coloredAlpha = qExp(-2.0 * PI * 18.0 / sampleRate);
    const double lowAlpha = qExp(-2.0 * PI * 0.45 / sampleRate);
    m_coloredNoise[channel] = coloredAlpha * m_coloredNoise[channel]
                              + (1.0 - coloredAlpha) * white;
    m_lowNoise[channel] = lowAlpha * m_lowNoise[channel]
                          + (1.0 - lowAlpha) * white;
    const double highNoise = white - m_previousWhite[channel];
    m_previousWhite[channel] = white;

    const double phase = m_phase[channel];
    const double envelope = 0.82
                            + 0.14 * qSin(2.0 * PI * 0.11 * timeSeconds + phase)
                            + 0.06 * qSin(2.0 * PI * 0.037 * timeSeconds + phase * 0.37);
    const double environmental = 0.025 * qSin(2.0 * PI * 0.72 * timeSeconds + phase * 0.08)
                                 + 0.012 * qSin(2.0 * PI * 1.31 * timeSeconds + phase * 0.16);
    const double hum = m_humCoupling[channel]
                       * qSin(2.0 * PI * 50.0 * timeSeconds + phase);
    const double stochastic = m_noiseScale[channel]
                              * (0.23 * envelope * m_coloredNoise[channel]
                                 + 0.022 * highNoise
                                 + 0.55 * m_lowNoise[channel]);
    return m_gain[channel] * (stochastic + environmental + hum);
}

double SeismicSignalGenerator::sineSample(int channel,
                                          double timeSeconds,
                                          int sampleRate,
                                          double frequencyHz)
{
    if (!isValidChannel(channel) || sampleRate <= 0
        || frequencyHz < 1.0 || frequencyHz > 200.0)
        return 0.0;

    const double phase = m_phase[channel];
    const double modulation = 1.0
                              + 0.025 * qSin(2.0 * PI * 0.17 * timeSeconds
                                             + phase * 0.31);
    const double sineGain = 0.90 + (m_gain[channel] - 0.82) * (0.20 / 0.36);
    const double carrier = 0.38 * sineGain * modulation
                           * qSin(2.0 * PI * frequencyHz * timeSeconds + phase);
    const double noise = 0.012 * m_noiseScale[channel] * nextNoise(channel);
    return carrier + noise;
}

void SeismicSignalGenerator::reset()
{
    m_coloredNoise.fill(0.0);
    m_lowNoise.fill(0.0);
    m_previousWhite.fill(0.0);
    for (int channel = 0; channel < m_randomStates.size(); ++channel)
    {
        quint32 state = mixBits(m_seed + static_cast<quint32>(channel + 1) * 2246822519U);
        m_randomStates[channel] = state == 0U ? 0xA341316CU : state;
    }
}

bool SeismicSignalGenerator::isValidChannel(int channel) const
{
    return channel >= 0 && channel < m_randomStates.size();
}

double SeismicSignalGenerator::nextNoise(int channel)
{
    quint32 &state = m_randomStates[channel];
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return 2.0 * static_cast<double>(state) / 4294967295.0 - 1.0;
}

DemoEventScheduler::DemoEventScheduler(int channelCount, quint32 seed)
    : m_channelCount(qMax(0, channelCount))
    , m_gridWidth(qMax(1, qCeil(qSqrt(qMax(1, channelCount)))))
    , m_seed(seed)
{
}

int DemoEventScheduler::sourceChannelForEvent(int eventIndex) const
{
    if (eventIndex < 0 || m_channelCount == 0)
        return -1;
    return static_cast<int>(mixBits(m_seed
                                    + static_cast<quint32>(eventIndex + 1) * 2654435761U)
                            % static_cast<quint32>(m_channelCount));
}

double DemoEventScheduler::eventStartTime(int eventIndex) const
{
    if (eventIndex < 0)
        return -1.0;
    const double jitter = 0.8
                          + 2.4 * unitValue(m_seed
                                            + static_cast<quint32>(eventIndex + 1)
                                                  * 2246822519U);
    return eventIndex * EVENT_SLOT_SECONDS + jitter;
}

double DemoEventScheduler::arrivalTime(int channel, int eventIndex) const
{
    if (!isTarget(channel, eventIndex))
        return -1.0;
    return eventStartTime(eventIndex)
           + distanceFromSource(channel, eventIndex) * ARRIVAL_DELAY_PER_SENSOR_SECONDS;
}

QVector<int> DemoEventScheduler::targetsForEvent(int eventIndex) const
{
    QVector<int> targets;
    if (eventIndex < 0 || m_channelCount == 0)
        return targets;

    for (int channel = 0; channel < m_channelCount; ++channel)
    {
        if (isTarget(channel, eventIndex))
            targets.append(channel);
    }
    return targets;
}

bool DemoEventScheduler::isTarget(int channel, int eventIndex) const
{
    if (channel < 0 || channel >= m_channelCount || eventIndex < 0)
        return false;

    const int source = sourceChannelForEvent(eventIndex);
    return qAbs(channel / m_gridWidth - source / m_gridWidth) <= 1
           && qAbs(channel % m_gridWidth - source % m_gridWidth) <= 1;
}

double DemoEventScheduler::distanceFromSource(int channel, int eventIndex) const
{
    const int source = sourceChannelForEvent(eventIndex);
    if (channel < 0 || channel >= m_channelCount || source < 0)
        return 0.0;
    const double rowDistance = channel / m_gridWidth - source / m_gridWidth;
    const double columnDistance = channel % m_gridWidth - source % m_gridWidth;
    return qSqrt(rowDistance * rowDistance + columnDistance * columnDistance);
}

double DemoEventScheduler::impulse(int channel, double timeSeconds) const
{
    if (channel < 0 || channel >= m_channelCount || timeSeconds < 0.0)
        return 0.0;

    const int eventIndex = static_cast<int>(timeSeconds / EVENT_SLOT_SECONDS);
    if (!isTarget(channel, eventIndex))
        return 0.0;

    const double localTime = timeSeconds - arrivalTime(channel, eventIndex);
    const double eventVariation = unitValue(m_seed
                                            + static_cast<quint32>(eventIndex + 1)
                                                  * 668265263U);
    const double eventDuration = 1.25 + 0.20 * eventVariation;
    if (localTime < 0.0 || localTime >= eventDuration)
        return 0.0;

    const double distance = distanceFromSource(channel, eventIndex);
    const double strength = 1.80
                            + 0.20 * unitValue(m_seed
                                               + static_cast<quint32>(eventIndex + 1)
                                                     * 3266489917U);
    const double attenuation = qExp(-0.32 * distance);
    const double pFrequency = 22.0 + 10.0 * eventVariation;
    const double sFrequency = 8.0
                              + 6.0 * unitValue(m_seed
                                                + static_cast<quint32>(eventIndex + 1)
                                                      * 374761393U);
    const double sDelay = 0.15
                          + 0.06 * unitValue(m_seed
                                             + static_cast<quint32>(eventIndex + 1)
                                                   * 2246822519U);
    const double eventPhase = 2.0 * PI
                              * unitValue(m_seed
                                          + static_cast<quint32>(eventIndex + 1)
                                                * 3266489917U);

    const double rickerArgument = PI * pFrequency * (localTime - 0.040);
    const double square = rickerArgument * rickerArgument;
    const double pWave = (1.0 - 2.0 * square) * qExp(-square);

    const double sTime = localTime - sDelay;
    const double sEnvelope = sTime > 0.0
                                 ? (1.0 - qExp(-28.0 * sTime))
                                       * qExp(-2.8 * sTime)
                                 : 0.0;
    const double sWave = sEnvelope
                         * (qSin(2.0 * PI * sFrequency * sTime + eventPhase)
                            + 0.28
                                  * qSin(2.0 * PI * sFrequency * 1.63 * sTime
                                         + eventPhase * 0.41));

    const double codaTime = localTime - sDelay - 0.20;
    const double codaEnvelope = codaTime > 0.0
                                    ? (1.0 - qExp(-18.0 * codaTime))
                                          * qExp(-2.4 * codaTime)
                                    : 0.0;
    const double coda = codaEnvelope
                        * (0.34 * qSin(2.0 * PI * sFrequency * 0.73 * codaTime
                                      + eventPhase * 0.27)
                           + 0.18
                                 * qSin(2.0 * PI * sFrequency * 1.37 * codaTime
                                        + eventPhase * 0.82));
    const double taperStart = eventDuration - 0.18;
    const double tailWindow = localTime > taperStart
                                  ? 0.5 * (1.0
                                           + qCos(PI * (localTime - taperStart)
                                                  / (eventDuration - taperStart)))
                                  : 1.0;
    return strength * attenuation * (0.42 * pWave + 1.05 * sWave + coda)
           * tailWindow;
}

ChannelEventDetector::ChannelEventDetector(int channelCount,
                                           double threshold,
                                           int confirmationBatches,
                                           int recoveryBatches)
    : m_threshold(qMax(0.0, threshold))
    , m_confirmationBatches(qMax(1, confirmationBatches))
    , m_recoveryBatches(qMax(1, recoveryBatches))
    , m_abnormalCounts(qMax(0, channelCount), 0)
    , m_normalCounts(qMax(0, channelCount), 0)
    , m_detected(qMax(0, channelCount), false)
{
}

void ChannelEventDetector::update(int channel, const QVector<double> &batch)
{
    if (!isValidChannel(channel) || batch.isEmpty())
        return;

    double peak = 0.0;
    for (double value : batch)
        peak = qMax(peak, qAbs(value));

    if (peak > m_threshold)
    {
        m_normalCounts[channel] = 0;
        ++m_abnormalCounts[channel];
        if (m_abnormalCounts[channel] >= m_confirmationBatches)
            m_detected[channel] = true;
        return;
    }

    m_abnormalCounts[channel] = 0;
    if (m_detected[channel] && ++m_normalCounts[channel] >= m_recoveryBatches)
    {
        m_detected[channel] = false;
        m_normalCounts[channel] = 0;
    }
}

bool ChannelEventDetector::eventDetected(int channel) const
{
    return isValidChannel(channel) && m_detected[channel];
}

void ChannelEventDetector::reset()
{
    m_abnormalCounts.fill(0);
    m_normalCounts.fill(0);
    m_detected.fill(false);
}

bool ChannelEventDetector::isValidChannel(int channel) const
{
    return channel >= 0 && channel < m_detected.size();
}
