#include "AnalogDSP.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace
{
void check (bool condition, const char* reason)
{
    if (! condition)
    {
        std::cerr << "FAIL: " << reason << '\n';
        std::exit (1);
    }
}

void testShapes()
{
    for (int character = 0; character < 3; ++character)
    {
        for (int i = -300; i <= 300; ++i)
        {
            const auto x = i / 100.0;
            check (vi::saturate (x, 0.0, character) == x,
                   "zero drive must pass signal unchanged");
            for (auto drive : { 0.1, 6.0, 12.0, 24.0 })
                check (std::isfinite (vi::saturate (x, drive, character)),
                       "nonlinearity must have finite output");
        }
        check (std::abs (vi::saturate (0.0, 24.0, character)) < 1.0e-12,
               "saturator must not create static DC at silence");
        const auto low = std::abs (vi::saturate (0.5, 24.0, character));
        check (low > 0.5 && low < 1.25, "drive should produce measurable harmonics");
    }
    // Independent v0.5 transfer-curve reference.
    for (int character = 0; character < 3; ++character)
        for (double drive : { 0.0, 0.1, 3.0, 12.0, 18.0, 24.0 })
        {
            const auto coeff = vi::makeSaturationParameters (drive);
            const auto amount = std::clamp (drive / 24.0, 0.0, 1.0);
            const auto gain = std::pow (10.0, drive / 20.0);
            for (int n = -100; n <= 100; ++n)
            {
                const auto x = n / 100.0;
                double old = x;
                if (amount > 0.0)
                {
                    double shaped = 0.0;
                    if (character == 1)
                    {
                        constexpr double bias = 0.35;
                        shaped = (std::tanh (gain * x + bias) - std::tanh (bias))
                               / (std::tanh (gain + bias) - std::tanh (bias));
                    }
                    else if (character == 2)
                        shaped = std::atan (gain * x) / std::atan (gain);
                    else
                        shaped = std::tanh (gain * x) / std::tanh (gain);
                    old = x + amount * (shaped - x);
                }
                check (std::abs (old - vi::saturateWithParameters (x, coeff, character)) < 1.0e-12,
                       "Optimization changed historical saturation curve");
            }
        }
    check (vi::saturate (0.5, 18.0, 0) != vi::saturate (0.5, 18.0, 2),
           "characters should differ");
    check (std::abs (vi::saturate (-0.5, 18.0, 1)
                     + vi::saturate (0.5, 18.0, 1)) > 0.01,
           "valve character should be asymmetric");
}

void testDelay()
{
    vi::SampleDelay<128> delay;
    for (auto length : { 0, 1, 17, 127 })
    {
        delay.reset();
        for (int i = 0; i < 300; ++i)
        {
            const auto v = delay.process (i == 0 ? 1.0f : 0.0f, length);
            check (v == (i == length ? 1.0f : 0.0f), "delay alignment must be exact");
        }
    }
}
} // namespace

int main()
{
    testShapes();
    testDelay();
    std::cout << "Analog DSP tests passed\n";
}
