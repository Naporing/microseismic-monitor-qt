#include "spectrumanalyzer.h"

#include <QtMath>

#include <algorithm>
#include <complex>

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr int MINIMUM_FFT_SIZE = 256;
constexpr double MINIMUM_ENERGY = 1.0e-12;

void transform(QVector<std::complex<double>> &values)
{
    const int size = values.size();
    for (int index = 1, reversed = 0; index < size; ++index)
    {
        int bit = size >> 1;
        while (reversed & bit)
        {
            reversed ^= bit;
            bit >>= 1;
        }
        reversed ^= bit;
        if (index < reversed)
            std::swap(values[index], values[reversed]);
    }

    for (int length = 2; length <= size; length <<= 1)
    {
        const double angle = -2.0 * PI / length;
        const std::complex<double> step(qCos(angle), qSin(angle));
        for (int offset = 0; offset < size; offset += length)
        {
            std::complex<double> factor(1.0, 0.0);
            for (int index = 0; index < length / 2; ++index)
            {
                const std::complex<double> even = values[offset + index];
                const std::complex<double> odd = values[offset + index + length / 2]
                                                 * factor;
                values[offset + index] = even + odd;
                values[offset + index + length / 2] = even - odd;
                factor *= step;
            }
        }
    }
}

} // namespace

bool SpectrumResult::isValid() const
{
    return !frequencies.isEmpty()
           && frequencies.size() == magnitudesDb.size()
           && peakFrequencyHz > 0.0;
}

SpectrumResult SpectrumAnalyzer::analyze(const QVector<double> &samples,
                                         int sampleRate,
                                         int maximumInputSize,
                                         double maximumFrequencyHz)
{
    SpectrumResult result;
    if (sampleRate <= 0 || maximumInputSize < MINIMUM_FFT_SIZE
        || maximumFrequencyHz <= 0.0)
        return result;

    const int available = qMin(samples.size(), maximumInputSize);
    int fftSize = 1;
    while (fftSize <= available / 2)
        fftSize <<= 1;
    if (fftSize < MINIMUM_FFT_SIZE)
        return result;

    const int start = samples.size() - fftSize;
    double mean = 0.0;
    for (int index = start; index < samples.size(); ++index)
        mean += samples[index];
    mean /= fftSize;

    QVector<std::complex<double>> values(fftSize);
    double energy = 0.0;
    for (int index = 0; index < fftSize; ++index)
    {
        const double window = 0.5 - 0.5 * qCos(2.0 * PI * index / (fftSize - 1));
        const double value = (samples[start + index] - mean) * window;
        values[index] = std::complex<double>(value, 0.0);
        energy += value * value;
    }
    if (energy <= MINIMUM_ENERGY)
        return result;

    transform(values);

    const double binWidth = static_cast<double>(sampleRate) / fftSize;
    const double visibleMaximum = qMin(maximumFrequencyHz, sampleRate / 2.0);
    const int lastBin = qMin(fftSize / 2,
                             static_cast<int>(qFloor(visibleMaximum / binWidth)));
    if (lastBin < 1)
        return result;

    QVector<double> magnitudes(lastBin + 1);
    double maximumMagnitude = 0.0;
    int peakBin = 0;
    for (int bin = 0; bin <= lastBin; ++bin)
    {
        magnitudes[bin] = std::abs(values[bin]);
        if (bin > 0 && magnitudes[bin] > maximumMagnitude)
        {
            maximumMagnitude = magnitudes[bin];
            peakBin = bin;
        }
    }
    if (maximumMagnitude <= MINIMUM_ENERGY || peakBin == 0)
        return result;

    result.frequencies.reserve(lastBin + 1);
    result.magnitudesDb.reserve(lastBin + 1);
    for (int bin = 0; bin <= lastBin; ++bin)
    {
        const double ratio = qMax(magnitudes[bin] / maximumMagnitude, 0.001);
        result.frequencies.append(bin * binWidth);
        result.magnitudesDb.append(qBound(-60.0,
                                          20.0 * qLn(ratio) / qLn(10.0),
                                          0.0));
    }
    result.peakFrequencyHz = peakBin * binWidth;
    return result;
}
