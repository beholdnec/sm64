#include "libultra_internal.h"
#include "hardware.h"

s32 __osSiDeviceBusy() {
#ifdef __EMSCRIPTEN__
    return 0;
#else
    register u32 status;
    status = HW_REG(SI_STATUS_REG, u32);
    if (status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_READ_BUSY)) {
        return 1;
    } else {
        return 0;
    }
#endif
}
