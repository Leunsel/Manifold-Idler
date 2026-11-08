#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>

namespace Idler::Clock
{
    inline double Seconds()
    {
#if defined(_WIN64)
        const std::uint64_t interrupt = *reinterpret_cast<const volatile std::uint64_t*>(std::uintptr_t{ 0x7FFE0008 });
        return static_cast<double>(interrupt) * 1.0e-7;
#else
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
    }

    inline std::uint64_t TickMilliseconds()
    {
#if defined(_WIN64)
        const std::uint64_t ticks = *reinterpret_cast<const volatile std::uint64_t*>(std::uintptr_t{ 0x7FFE0320 });
        const std::uint32_t multiplier = *reinterpret_cast<const volatile std::uint32_t*>(std::uintptr_t{ 0x7FFE0004 });
        return (ticks * multiplier) >> 24;
#else
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
    }

    inline double OwnSeconds()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    class PaceMeter
    {
    public:
        bool Note(double own, double real)
        {
            const double ownPassed = own - m_ownFrom;
            const double realPassed = real - m_realFrom;
            if (m_realFrom < 0.0 || realPassed > kGapSeconds || realPassed < 0.0 || ownPassed < 0.0)
            {
                m_ownFrom = own;
                m_realFrom = real;
                return false;
            }
            if (realPassed < kWindowSeconds)
                return false;
            m_ownFrom = own;
            m_realFrom = real;
            double pace = ownPassed / realPassed;
            if (std::fabs(pace - 1.0) < kTolerance)
                pace = 1.0;
            if (std::fabs(pace / m_pace - 1.0) < kTolerance)
                return false;
            m_pace = pace;
            return true;
        }

        double Pace() const { return m_pace; }

    private:
        static constexpr double kWindowSeconds = 1.0;
        static constexpr double kGapSeconds = 5.0;
        static constexpr double kTolerance = 0.05;

        double m_ownFrom = 0.0;
        double m_realFrom = -1.0;
        double m_pace = 1.0;
    };
}
