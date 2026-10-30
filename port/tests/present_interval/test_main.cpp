// xdk/present_interval.h: the vblanks a frame is shown for, from D3DPRESENT_PARAMETERS.FullScreen_PresentationInterval.
// Build and run (UCRT64):  g++ -std=c++17 -Wall -I port/src/scda port/tests/present_interval/test_main.cpp -o pi_test
#include <cstdio>

#include "xdk/present_interval.h"

static int g_failed = 0;
#define CHECK_EQ(a, b)                                                                     \
    do {                                                                                   \
        unsigned x_ = (a), y_ = (b);                                                       \
        if (x_ != y_) {                                                                    \
            std::printf("FAIL %s:%d %s = %u, want %u\n", __FILE__, __LINE__, #a, x_, y_);  \
            g_failed++;                                                                    \
        }                                                                                  \
    } while (0)

int main() {
    using xdk::PresentIntervalVblanks;
    CHECK_EQ(PresentIntervalVblanks(0x00000000u), 1);  // DEFAULT
    CHECK_EQ(PresentIntervalVblanks(0x00000001u), 1);  // ONE
    CHECK_EQ(PresentIntervalVblanks(0x00000002u), 2);  // TWO
    CHECK_EQ(PresentIntervalVblanks(0x00000004u), 3);  // THREE
    CHECK_EQ(PresentIntervalVblanks(0x00000008u), 4);  // FOUR
    CHECK_EQ(PresentIntervalVblanks(0x80000000u), 1);  // IMMEDIATE
    CHECK_EQ(PresentIntervalVblanks(0x80000001u), 1);  // ONE_OR_IMMEDIATE
    CHECK_EQ(PresentIntervalVblanks(0x80000002u), 2);  // TWO_OR_IMMEDIATE (Prisoner of Azkaban)
    CHECK_EQ(PresentIntervalVblanks(0x80000004u), 3);  // THREE_OR_IMMEDIATE
    std::printf("%s (%d failed)\n", g_failed ? "FAILED" : "all passed", g_failed);
    return g_failed ? 1 : 0;
}
