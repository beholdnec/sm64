#include "libultra_internal.h"
#include "osAi.h"
#include "hardware.h"

u32 osAiGetLength() {
#ifdef __EMSCRIPTEN__
    return 0; // TODO
#else
    return HW_REG(AI_LEN_REG, u32);
#endif
}
