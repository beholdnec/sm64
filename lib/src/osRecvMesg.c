#ifdef __EMSCRIPTEN__
#include <stdio.h>
#include <emscripten.h>
#endif

#include "libultra_internal.h"

extern OSThread *D_803348A0;

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag) {
    register u32 int_disabled;
    register OSThread *thread;
    int_disabled = __osDisableInt();

#ifdef __EMSCRIPTEN__
    // printf("receiving message from queue %p\n", mq);
    if (!mq->validCount) {
        if (flag) {
            printf("Error: Blocked with no messages in queue %p. Please rework the code.\n", mq);
            EM_ASM(
                console.warn(new Error().stack);
            );
            return 0;
        } else {
            return -1;
        }
    }
#else
    while (!mq->validCount) {
        if (!flag) {
            __osRestoreInt(int_disabled);
            return -1;
        }
        D_803348A0->state = OS_STATE_WAITING;
        __osEnqueueAndYield(&mq->mtqueue);
    }
#endif

    if (msg != NULL) {
        *msg = *(mq->first + mq->msg);
    }

    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;

    if (mq->fullqueue->next != NULL) {
        thread = __osPopThread(&mq->fullqueue);
        osStartThread(thread);
    }

    __osRestoreInt(int_disabled);
    return 0;
}
