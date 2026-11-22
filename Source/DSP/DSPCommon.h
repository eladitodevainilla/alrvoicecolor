#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace alr::dsp
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kEpsilon = 1.0e-12f;

inline float killDenormal(float x) noexcept
{
    return (std::abs(x) < 1.0e-20f) ? 0.0f : x;
}

inline float clampf(float v, float lo, float hi) noexcept
{
    return std::max(lo, std::min(hi, v));
}

inline float dbToGain(float db) noexcept
{
    return std::pow(10.0f, db * 0.05f);
}

inline float gainToDb(float gain) noexcept
{
    return 20.0f * std::log10(std::max(gain, 1.0e-7f));
}

inline float hzToMidi(float hz) noexcept
{
    if (hz <= 1.0f) return 0.0f;
    return 69.0f + 12.0f * std::log2(hz / 440.0f);
}

inline float midiToHz(float midi) noexcept
{
    return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
}

// ============================================================================
// One-Pole Parameter Smoother (Audio-thread safe, zero allocations)
// ============================================================================
class OnePoleSmoother
{
public:
    void prepare(double sampleRate, float timeMs = 15.0f) noexcept
    {
        sr_ = std::max(8000.0, sampleRate);
        setTimeMs(timeMs);
    }

    void setTimeMs(float timeMs) noexcept
    {
        const float tSec = std::max(0.0005f, timeMs * 0.001f);
        coeff_ = std::exp(-1.0f / (static_cast<float>(sr_) * tSec));
    }

    void reset(float value) noexcept
    {
        current_ = value;
        target_ = value;
    }

    void setTarget(float target) noexcept { target_ = target; }
    float getTarget() const noexcept { return target_; }
    float getCurrent() const noexcept { return current_; }

    inline float process() noexcept
    {
        current_ = target_ + coeff_ * (current_ - target_);
        current_ = killDenormal(current_);
        return current_;
    }

private:
    double sr_ = 48000.0;
    float coeff_ = 0.99f;
    float current_ = 0.0f;
    float target_ = 0.0f;
};

// ============================================================================
// Direct Form II Transposed Biquad Filter (RBJ Cookbook)
// ============================================================================
enum class BiquadType
{
    Bypass,
    HighPass,
    LowPass,
    BandPass,
    Peak,
    LowShelf,
    HighShelf,
    Notch
};

class BiquadFilter
{
public:
    void reset() noexcept
    {
        z1_ = 0.0f;
        z2_ = 0.0f;
    }

    void setParameters(BiquadType type, double sampleRate, float freqHz, float q, float gainDb = 0.0f) noexcept
    {
        const double sr = std::max(8000.0, sampleRate);
        const double nyquist = sr * 0.48;
        const double f0 = std::clamp(static_cast<double>(freqHz), 15.0, nyquist);
        const double Q = std::clamp(static_cast<double>(q), 0.1, 24.0);

        if (type == BiquadType::Bypass || (type == BiquadType::Peak && std::abs(gainDb) < 0.01f) ||
            ((type == BiquadType::LowShelf || type == BiquadType::HighShelf) && std::abs(gainDb) < 0.01f))
        {
            b0_ = 1.0f; b1_ = 0.0f; b2_ = 0.0f; a1_ = 0.0f; a2_ = 0.0f;
            return;
        }

        const double w0 = 2.0 * 3.14159265358979323846 * f0 / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * Q);
        const double A = std::pow(10.0, static_cast<double>(gainDb) / 40.0);

        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a0 = 1.0, a1 = 0.0, a2 = 0.0;

        switch (type)
        {
            case BiquadType::HighPass:
                b0 = (1.0 + cosW0) * 0.5;
                b1 = -(1.0 + cosW0);
                b2 = (1.0 + cosW0) * 0.5;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosW0;
                a2 = 1.0 - alpha;
                break;

            case BiquadType::LowPass:
                b0 = (1.0 - cosW0) * 0.5;
                b1 = 1.0 - cosW0;
                b2 = (1.0 - cosW0) * 0.5;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosW0;
                a2 = 1.0 - alpha;
                break;

            case BiquadType::BandPass:
                b0 = alpha;
                b1 = 0.0;
                b2 = -alpha;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosW0;
                a2 = 1.0 - alpha;
                break;

            case BiquadType::Notch:
                b0 = 1.0;
                b1 = -2.0 * cosW0;
                b2 = 1.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosW0;
                a2 = 1.0 - alpha;
                break;

            case BiquadType::Peak:
                b0 = 1.0 + alpha * A;
                b1 = -2.0 * cosW0;
                b2 = 1.0 - alpha * A;
                a0 = 1.0 + alpha / A;
                a1 = -2.0 * cosW0;
                a2 = 1.0 - alpha / A;
                break;

            case BiquadType::LowShelf:
            {
                const double sqrtA = std::sqrt(A);
                const double twoSqrtAAlpha = 2.0 * sqrtA * alpha;
                b0 = A * ((A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha);
                b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0);
                b2 = A * ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha);
                a0 = (A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha;
                a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosW0);
                a2 = (A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha;
                break;
            }

            case BiquadType::HighShelf:
            {
                const double sqrtA = std::sqrt(A);
                const double twoSqrtAAlpha = 2.0 * sqrtA * alpha;
                b0 = A * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha);
                b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0);
                b2 = A * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha);
                a0 = (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha;
                a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW0);
                a2 = (A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha;
                break;
            }

            case BiquadType::Bypass:
            default:
                break;
        }

        const double invA0 = 1.0 / a0;
        b0_ = static_cast<float>(b0 * invA0);
        b1_ = static_cast<float>(b1 * invA0);
        b2_ = static_cast<float>(b2 * invA0);
        a1_ = static_cast<float>(a1 * invA0);
        a2_ = static_cast<float>(a2 * invA0);
    }

    inline float processSample(float x) noexcept
    {
        const float y = b0_ * x + z1_;
        z1_ = killDenormal(b1_ * x - a1_ * y + z2_);
        z2_ = killDenormal(b2_ * x - a2_ * y);
        return y;
    }

    // Magnitude response at frequency fHz (for GUI curve display)
    float getMagnitudeDb(double sampleRate, float freqHz) const noexcept
    {
        const double w = 2.0 * 3.14159265358979323846 * static_cast<double>(freqHz) / std::max(8000.0, sampleRate);
        const std::complex<double> z1 = std::polar(1.0, -w);
        const std::complex<double> z2 = std::polar(1.0, -2.0 * w);
        const std::complex<double> num = static_cast<double>(b0_) + static_cast<double>(b1_) * z1 + static_cast<double>(b2_) * z2;
        const std::complex<double> den = 1.0 + static_cast<double>(a1_) * z1 + static_cast<double>(a2_) * z2;
        const double mag = std::abs(num / den);
        return static_cast<float>(20.0 * std::log10(std::max(mag, 1.0e-7)));
    }

private:
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;
    float z1_ = 0.0f, z2_ = 0.0f;
};

// ============================================================================
// Stereo Biquad Pair (shares coefficients, separate state per channel)
// ============================================================================
class StereoBiquad
{
public:
    void reset() noexcept
    {
        left_.reset();
        right_.reset();
    }

    void setParameters(BiquadType type, double sampleRate, float freqHz, float q, float gainDb = 0.0f) noexcept
    {
        left_.setParameters(type, sampleRate, freqHz, q, gainDb);
        right_.setParameters(type, sampleRate, freqHz, q, gainDb);
    }

    inline float processLeft(float x) noexcept { return left_.processSample(x); }
    inline float processRight(float x) noexcept { return right_.processSample(x); }

    float getMagnitudeDb(double sampleRate, float freqHz) const noexcept
    {
        return left_.getMagnitudeDb(sampleRate, freqHz);
    }

private:
    BiquadFilter left_;
    BiquadFilter right_;
};

// ============================================================================
// Pre-allocated Radix-2 Iterative FFT (Zero heap allocations during compute)
// ============================================================================
class Radix2FFT
{
public:
    static constexpr std::size_t kMaxSize = 4096;

    void prepare(std::size_t fftSize)
    {
        size_ = fftSize;
        bitRev_.resize(size_);
        twiddleRe_.resize(size_ / 2);
        twiddleIm_.resize(size_ / 2);
        window_.resize(size_);
        re_.assign(size_, 0.0f);
        im_.assign(size_, 0.0f);

        std::size_t logN = 0;
        while ((std::size_t(1) << logN) < size_) ++logN;

        for (std::size_t i = 0; i < size_; ++i)
        {
            std::size_t rev = 0;
            for (std::size_t b = 0; b < logN; ++b)
                if (i & (std::size_t(1) << b))
                    rev |= (std::size_t(1) << (logN - 1 - b));
            bitRev_[i] = rev;
            window_[i] = 0.5f * (1.0f - std::cos(kTwoPi * static_cast<float>(i) / static_cast<float>(size_ - 1)));
        }

        for (std::size_t k = 0; k < size_ / 2; ++k)
        {
            const float angle = -kTwoPi * static_cast<float>(k) / static_cast<float>(size_);
            twiddleRe_[k] = std::cos(angle);
            twiddleIm_[k] = std::sin(angle);
        }
    }

    // Computes magnitude spectrum (length = size_/2 + 1) normalized by window sum
    void computeMagnitudeSpectrum(const float* inputTime, float* outMagnitudes) noexcept
    {
        if (size_ == 0) return;

        for (std::size_t i = 0; i < size_; ++i)
        {
            const std::size_t j = bitRev_[i];
            re_[j] = inputTime[i] * window_[i];
            im_[j] = 0.0f;
        }

        for (std::size_t len = 2; len <= size_; len <<= 1)
        {
            const std::size_t half = len >> 1;
            const std::size_t step = size_ / len;

            for (std::size_t i = 0; i < size_; i += len)
            {
                std::size_t twIdx = 0;
                for (std::size_t j = 0; j < half; ++j)
                {
                    const float wr = twiddleRe_[twIdx];
                    const float wi = twiddleIm_[twIdx];
                    twIdx += step;

                    const std::size_t u = i + j;
                    const std::size_t v = u + half;

                    const float tr = wr * re_[v] - wi * im_[v];
                    const float ti = wr * im_[v] + wi * re_[v];

                    re_[v] = re_[u] - tr;
                    im_[v] = im_[u] - ti;
                    re_[u] += tr;
                    im_[u] += ti;
                }
            }
        }

        // Hann coherent gain is 0.5, single-sided scaling factor is 4 / N
        const float norm = 4.0f / static_cast<float>(size_);
        const std::size_t numBins = size_ / 2 + 1;
        for (std::size_t k = 0; k < numBins; ++k)
        {
            const float mag = std::sqrt(re_[k] * re_[k] + im_[k] * im_[k]) * norm;
            outMagnitudes[k] = mag;
        }
    }

    std::size_t getSize() const noexcept { return size_; }

private:
    std::size_t size_ = 0;
    std::vector<std::size_t> bitRev_;
    std::vector<float> twiddleRe_;
    std::vector<float> twiddleIm_;
    std::vector<float> window_;
    std::vector<float> re_;
    std::vector<float> im_;
};

// ============================================================================
// Lock-Free Single-Producer Single-Consumer Ring Buffer (Audio -> UI/Worker)
// ============================================================================
template <typename T, std::size_t CapacityPow2>
class LockFreeSPSCQueue
{
    static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0, "Capacity must be a power of 2");

public:
    void reset() noexcept
    {
        readIdx_.store(0, std::memory_order_relaxed);
        writeIdx_.store(0, std::memory_order_relaxed);
    }

    std::size_t push(const T* data, std::size_t count) noexcept
    {
        const std::size_t w = writeIdx_.load(std::memory_order_relaxed);
        const std::size_t r = readIdx_.load(std::memory_order_acquire);
        const std::size_t freeSpace = CapacityPow2 - (w - r) - 1;
        const std::size_t toWrite = std::min(count, freeSpace);

        for (std::size_t i = 0; i < toWrite; ++i)
            buffer_[(w + i) & kMask] = data[i];

        writeIdx_.store(w + toWrite, std::memory_order_release);
        return toWrite;
    }

    std::size_t pop(T* dest, std::size_t maxCount) noexcept
    {
        const std::size_t r = readIdx_.load(std::memory_order_relaxed);
        const std::size_t w = writeIdx_.load(std::memory_order_acquire);
        const std::size_t available = w - r;
        const std::size_t toRead = std::min(maxCount, available);

        for (std::size_t i = 0; i < toRead; ++i)
            dest[i] = buffer_[(r + i) & kMask];

        readIdx_.store(r + toRead, std::memory_order_release);
        return toRead;
    }

    std::size_t availableToRead() const noexcept
    {
        return writeIdx_.load(std::memory_order_acquire) - readIdx_.load(std::memory_order_relaxed);
    }

private:
    static constexpr std::size_t kMask = CapacityPow2 - 1;
    std::array<T, CapacityPow2> buffer_{};
    alignas(64) std::atomic<std::size_t> writeIdx_{0};
    alignas(64) std::atomic<std::size_t> readIdx_{0};
};

} // namespace alr::dsp
