#pragma once
#include <cstdint>

namespace xdk {

// D3DPRESENT_PARAMETERS.FullScreen_PresentationInterval -> vertical blanks a frame is shown for. XDK values: ONE 1,
// TWO 2, THREE 4, FOUR 8, IMMEDIATE 0x80000000, and the Xbox-only ONE/TWO/THREE_OR_IMMEDIATE (IMMEDIATE ored in: wait
// for that many vblanks, but present at once when the frame is already late, which Pace's "only wait while early"
// already does). DEFAULT (0) and IMMEDIATE are one vblank here.
inline uint32_t PresentIntervalVblanks(uint32_t interval) {
    switch (interval & 0x7FFFFFFFu) {
        case 2: return 2;
        case 4: return 3;
        case 8: return 4;
        default: return 1;
    }
}

}  // namespace xdk
