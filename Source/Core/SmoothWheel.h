#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

namespace Idler
{
    class SmoothWheel
    {
    public:
        void Add(float notches)
        {
            if (m_left * notches < 0.0f)
                m_left = 0.0f;
            if (std::fabs(notches) < 1.0f)
                m_direct += notches;
            else
                m_left += notches;
        }

        float Take(float seconds, bool animated)
        {
            float step = std::exchange(m_direct, 0.0f);
            if (m_left == 0.0f)
                return step;
            if (!animated)
                return step + std::exchange(m_left, 0.0f);

            const float share = 1.0f - std::exp(-std::min(seconds, kLongestFrameSeconds) / kSeconds);
            float part = m_left * share;
            if (std::fabs(part) < kSmallest)
                part = std::copysign(kSmallest, m_left);
            if (std::fabs(part) >= std::fabs(m_left))
                part = m_left;
            m_left -= part;
            return step + part;
        }

        bool Busy() const { return m_left != 0.0f || m_direct != 0.0f; }

    private:
        static constexpr float kSeconds = 0.04f;
        static constexpr float kLongestFrameSeconds = 1.0f / 30.0f;
        static constexpr float kSmallest = 1.0f / 40.0f;

        float m_left = 0.0f;
        float m_direct = 0.0f;
    };
}
