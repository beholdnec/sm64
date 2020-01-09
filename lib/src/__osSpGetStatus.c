#include "libultra_internal.h"
#include "hardware.h"
u32 __osSpGetStatus() {
#ifdef __EMSCRIPTEN__
    // TODO
    return 0;
#else
    return HW_REG(SP_STATUS_REG, u32);
#endif
}
