#include "libultra_internal.h"
#include "hardware.h"

s32 __osAiDeviceBusy(void) {
#ifdef __EMSCRIPTEN__
    return 0;
#else
    register s32 status = HW_REG(AI_STATUS_REG, u32);
    if ((status & AI_STATUS_AI_FULL) != 0) {
        return 1;
    } else {
        return 0;
    }
#endif
}
