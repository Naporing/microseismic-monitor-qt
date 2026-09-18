#ifndef EVENTLOGIC_H
#define EVENTLOGIC_H

#include <QtGlobal>
#include <QVector>

class SeismicSignalGenerator
{
public:
    explicit SeismicSignalGenerator(int channelCount, quint32 seed = 16092026);

    double sample(int channel, double timeSeconds, int sampleRate);
    double sineSample(int channel,
                      double timeSeconds,
                      int sampleRate,
                      double frequencyHz);
    void reset();

private:
    bool isValidChannel(int channel) const;
    double nextNoise(int channel);

    quint32 m_seed;
    QVector<quint32> m_randomStates;
    QVector<double> m_coloredNoise;
    QVector<double> m_lowNoise;
    QVector<double> m_previousWhite;
    QVector<double> m_gain;
    QVector<double> m_noiseScale;
    QVector<double> m_phase;
    QVector<double> m_humCoupling;
};

class DemoEventScheduler
{
public:
    explicit DemoEventScheduler(int channelCount, quint32 seed = 20260916);

    int sourceChannelForEvent(int eventIndex) const;
    double eventStartTime(int eventIndex) const;
    double arrivalTime(int channel, int eventIndex) const;
    QVector<int> targetsForEvent(int eventIndex) const;
    double impulse(int channel, double timeSeconds) const;

private:
    double distanceFromSource(int channel, int eventIndex) const;
    bool isTarget(int channel, int eventIndex) const;

    int m_channelCount;
    int m_gridWidth;
    quint32 m_seed;
};

class ChannelEventDetector
{
public:
    explicit ChannelEventDetector(int channelCount,
                                  double threshold = 0.78,
                                  int confirmationBatches = 2,
                                  int recoveryBatches = 50);

    void update(int channel, const QVector<double> &batch);
    bool eventDetected(int channel) const;
    void reset();

private:
    bool isValidChannel(int channel) const;

    double m_threshold;
    int m_confirmationBatches;
    int m_recoveryBatches;
    QVector<int> m_abnormalCounts;
    QVector<int> m_normalCounts;
    QVector<bool> m_detected;
};

#endif
