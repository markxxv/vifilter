#include "ResponseMath.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

void check (bool ok, const char* reason)
{
    if (! ok)
    {
        std::cerr << "FAIL: " << reason << '\n';
        std::exit (1);
    }
}

int main()
{
    constexpr double fs = 48000.0, fc = 1200.0;
    const auto m = [] (double freq, int mode, double mix = 1.0,
                       double trim = 0.0, double output = 0.0)
    {
        return vi::response::magnitude (fs, freq, fc, 0.0, mode, mix, trim, output);
    };

    check (m (100, 0) > 0.9 && m (10000, 0) < 0.03, "LP curve incorrect");
    check (m (100, 1) < 0.02 && m (10000, 1) > 0.9, "HP curve incorrect");
    check (m (1200, 2) > m (100, 2) * 4.0, "BP should peak near cutoff");
    check (m (1200, 2) > m (10000, 2) * 4.0, "BP should reject highs");
    check (std::abs (m (18000, 0, 0.0) - 1.0) < 1.0e-9, "Dry path must be flat");
    check (std::abs (m (1000, 1, 0.0, 6.0, -6.0) - 1.0) < 1.0e-9,
           "Gain offsets must cancel");
    for (int mode = 0; mode < 3; ++mode)
        for (int i = 20; i < 21000; i += 37)
            check (std::isfinite (m (i, mode)), "Response curve must stay finite");
    std::cout << "ResponseMath tests passed\n";
}
