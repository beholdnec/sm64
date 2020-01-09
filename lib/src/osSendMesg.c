#ifdef __EMSCRIPTEN__
#include <stdio.h>
#include <emscripten.h>
#endif

#include "libultra_internal.h"

extern OSThread *D_803348A0;

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag) {
    register u32 int_disabled;
    register s32 index;
    register OSThread *s2;
    int_disabled = __osDisableInt();

#ifdef __EMSCRIPTEN__
    // printf("sending message %p to queue %p\n", msg, mq);
    if (flag) {
        printf("Error: Blocked sending messages in queue %p. Please rework the code.\n", mq);
        EM_ASM(
            console.warn(new Error().stack);
        );
        return 0;
    }
#endif

    while (mq->validCount >= mq->msgCount) {
        if (flag == OS_MESG_BLOCK) {
            D_803348A0->state = 8;
            __osEnqueueAndYield(&mq->fullqueue);
        } else {
            __osRestoreInt(int_disabled);
            return -1;
        }
    }

    index = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[index] = msg;
    mq->validCount++;

    if (mq->mtqueue->next != NULL) {
        s2 = __osPopThread(&mq->mtqueue);
        osStartThread(s2);
    }

    __osRestoreInt(int_disabled);
    return 0;
}
