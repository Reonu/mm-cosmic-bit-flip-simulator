#include "modding.h"
#include "global.h"
#include "recomputils.h"
#include "recompconfig.h"
#include "sys_flashrom.h"
#include "libu64/stackcheck.h"
#include "z64thread.h"

extern f32 Rand_ZeroOne(void);

int get_random_value() {
    return Rand_ZeroOne() * 0x7A1200;
}
void* pointer;
RECOMP_CALLBACK("*", recomp_on_play_main)
void solar_storm(PlayState* play) {
    f32 bitflipChance = recomp_get_config_u32("bit_flip_chance") / 100.0f;
    if (Rand_ZeroOne() > bitflipChance) {
        return;
    }
    uintptr_t rand_addr = (uintptr_t)get_random_value() + 0x80000000;
    int bit_index = (int)(Rand_ZeroOne() * 8);
    pointer = (void*)rand_addr;
    recomp_printf("Flipping bit %d at address: %p\n", bit_index, pointer);
    if (pointer != NULL) {
        char* value = (char*)pointer;
        *value ^= (1 << bit_index);
    }
}

extern FlashromRequest sFlashromRequest;;
extern OSMesg sSysFlashromMsgBuf[1];
extern StackEntry sSysFlashromStackInfo;
extern STACK(sSysFlashromStack, 0x1000);
extern OSThread sSysFlashromThread;
extern void SysFlashrom_ThreadEntry(void* arg);
extern s32 SysFlashrom_IsInit(void);

RECOMP_PATCH void SysFlashrom_WriteDataSync(void* addr, u32 pageNum, u32 pageCount) {
    if (recomp_get_config_u32("allow_saving") == 0) {
        return;
    }
    SysFlashrom_WriteDataAsync(addr, pageNum, pageCount);
    SysFlashrom_AwaitResult();
}

RECOMP_PATCH void SysFlashrom_WriteDataAsync(u8* addr, u32 pageNum, u32 pageCount) {
    if (recomp_get_config_u32("allow_saving") == 0) {
        return;
    }
    FlashromRequest* req = &sFlashromRequest;
    if (SysFlashrom_IsInit()) {
        req->requestType = FLASHROM_REQUEST_WRITE;
        req->addr = addr;
        req->pageNum = pageNum;
        req->pageCount = pageCount;
        osCreateMesgQueue(&req->messageQueue, sSysFlashromMsgBuf, ARRAY_COUNT(sSysFlashromMsgBuf));
        StackCheck_Init(&sSysFlashromStackInfo, sSysFlashromStack, STACK_TOP(sSysFlashromStack), 0, 0x100,
                        "sys_flashrom");
        osCreateThread(&sSysFlashromThread, Z_THREAD_ID_FLASHROM, SysFlashrom_ThreadEntry, req,
                       STACK_TOP(sSysFlashromStack), Z_PRIORITY_FLASHROM);
        osStartThread(&sSysFlashromThread);
    }    
}