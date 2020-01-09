// TODO: header stuff.

#include <stdio.h>
#include <stack>
#include <string>
#include <vector>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES2/gl2.h>

extern "C" {
#include <tools/libmio0.h>
#include <ultra64.h>
#include <PR/gu.h>

#include "main.h"
#include "game.h"
#include "audio/external.h"
#include "buffers/gfx_output_buffer.h"
#include "sound_init.h"
#include "display.h"
#include "save_file.h"
#include "seq_ids.h"
#include "print.h"
#include "profiler.h"
#include "engine/level_script.h"
#include "src/engine/graph_node.h"
#include "src/engine/math_util.h"
#include "src/buffers/buffers.h"

// Message IDs (copied from src/game/main.c; values must match)
#define MESG_SP_COMPLETE 100
#define MESG_DP_COMPLETE 101
#define MESG_VI_VBLANK 102
#define MESG_START_GFX_SPTASK 103
#define MESG_NMI_REQUEST 104

extern Vec3f unused80339DC0;
extern void AllocPool(void);
extern void setup_mesg_queues(void);
extern void read_controller_inputs();
extern void thread3_main(UNUSED void *arg);
extern void handle_vblank(void);
extern void handle_dp_complete(void);
extern void handle_sp_complete(void);
extern void handle_nmi_request(void);
extern void start_gfx_sptask(void);
extern void Dummy802461DC(void);
extern void Dummy802461CC(void);
extern void create_thread(OSThread *thread, OSId id, void (*entry)(void *), void *arg, void *sp, OSPri pri);
extern OSMesg sSoundMesgBuf[1];
extern OSMesgQueue sSoundMesgQueue;
extern struct VblankHandler sSoundVblankHandler;
extern u16 sCurrFBNum;

// XXX: dummy buffers; TODO: fix and remove
u8 gBankSetsData[0x1000] = { 0 };
u8 gSoundDataRaw[0x1000] = { 0 };
u8 gSoundDataADSR[0x1000] = { 0 };
u8 gMusicData[0x1000] = { 0 };
u64 rspF3D[] = { 0 };
u64 rspF3DStart[] = { 0 };
u64 rspF3DEnd[] = { 0 };
u64 rspF3DBootStart[] = {0};
u64 rspF3DBootEnd[] = {0};
u64 rspF3DDataStart[] = {0};
u64 rspAspMainStart[] = {0};
u64 rspAspMainDataStart[] = {0};
u64 rspAspMainDataEnd[] = {0};
u8 _gd_dynlistsSegmentRomStart[] = {0};
u8 _gd_dynlistsSegmentRomEnd[] = {0};

u8 osAppNmiBuffer[64];
u32 osResetType = RESET_TYPE_COLD_RESET;
u32 osTvType = TV_TYPE_NTSC;

typedef struct OSEventMessageStruct_0_s {
    OSMesgQueue *queue;
    OSMesg msg;
} OSEventMessageStruct_0;

extern OSEventMessageStruct_0 D_80363830[16];
static const OSEventMessageStruct_0* gExceptionTable = D_80363830;

void __osEnqueueAndYield(OSThread**) {
    // TODO
}

s32 __osDisableInt() {
    // TODO
    return 0;
}

void __osRestoreInt(s32) {
    // TODO
}

u32 osGetCount(void) {
    // TODO
    return 0;
}

void osMapTLB(s32, OSPageMask, void *, u32, u32, s32) {
    // TODO
}

void osWritebackDCache(void *, size_t) {
    // TODO
}

void osWritebackDCacheAll(void) {
    // TODO
}

void __osDispatchThread() {
    // TODO
}

void __osEnqueueThread(OSThread**, OSThread*) {
    // TODO
}

OSThread* __osPopThread(OSThread**) {
    // TODO
    return 0;
}

void osInvalDCache(void *, size_t) {
    // TODO
}

void osInvalICache(void *, size_t) {
    // TODO
}

u32 osSetTimer(OSTimer *, OSTime, u64, OSMesgQueue *, OSMesg) {
    // TODO
    return 0;
}

void osUnmapTLBAll(void) {
    // TODO
}

void decompress(const void *compressed, void *dest) {
    int result = mio0_decode(reinterpret_cast<const unsigned char*>(compressed),
        reinterpret_cast<unsigned char*>(dest), NULL);
    printf("decompressed %d bytes\n", result);
}

u32 __osProbeTLB(void*) {
    // TODO
    return 0;
}

void osMapTLBRdb(void) {
    // TODO
}

void __osSetCompare(u32) {
    // TODO
}

void __osSetFpcCsr(u32) {
    // TODO
}

void __osCleanupThread(void) {
    // TODO
}

u32 __osGetSR() {
    // TODO
    return 0;
}

void __osSetSR(u32) {
    // TODO
}

void __osSpSetStatus(u32) {
    // TODO
}

// A full 4x4 matrix multiply. Needed since mtxf_mul assumes the bottom row is 0,0,0,1.
void MultMatrix(Mat4 dest, const Mat4 m0_, const Mat4 m1_)
{
    // Make copies to avoid errors when dest is m0 or m1
    Mat4 m0;
    Mat4 m1;
    mtxf_copy(m0, m0_);
    mtxf_copy(m1, m1_);

    // Mat4's appear to be addressed by [column][row]. In other words, they are column-major.
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            dest[c][r] = 0;
            for (int k = 0; k < 4; k++) {
                dest[c][r] += m0[k][r] * m1[c][k];
            }
        }
    }
}

void TransposeMatrix(Mat4 dest, const Mat4 m_) {
    Mat4 m;
    mtxf_copy(m, m_);

    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            dest[c][r] = m[r][c];
        }
    }
}

static bool sEnterPressed = false;
static bool sArrowLeftPressed = false;
static bool sArrowRightPressed = false;
static bool sArrowUpPressed = false;
static bool sArrowDownPressed = false;
static bool sXPressed = false;
static bool sZPressed = false;

extern u8 _osCont_numControllers;

void osContGetReadData(OSContPad *pad) {
    if (_osCont_numControllers > 0) {
        pad[0].errno = 0;
        pad[0].button = (sEnterPressed ? START_BUTTON : 0) |
            (sZPressed ? B_BUTTON : 0) |
            (sXPressed ? A_BUTTON : 0);
        pad[0].stick_x = (sArrowLeftPressed ? -80 : (sArrowRightPressed ? 80 : 0));
        pad[0].stick_y = (sArrowUpPressed ? 80 : (sArrowDownPressed ? -80 : 0));

        EmscriptenGamepadEvent gamepad = {0};
        if (emscripten_sample_gamepad_data() == EMSCRIPTEN_RESULT_SUCCESS &&
            emscripten_get_num_gamepads() >= 1 &&
            emscripten_get_gamepad_status(0, &gamepad) == EMSCRIPTEN_RESULT_SUCCESS)
        {
            static bool sGamepadPrinted = false;
            if (!sGamepadPrinted) {
                printf("Gamepad detected: id %s mapping %s\n", gamepad.id, gamepad.mapping);
                sGamepadPrinted = true;
            }

            pad[0].stick_x = gamepad.axis[0] * 80;
            pad[0].stick_y = gamepad.axis[1] * -80;
            pad[0].button |= (gamepad.digitalButton[9] ? START_BUTTON : 0) |
                (gamepad.digitalButton[0] ? A_BUTTON : 0) |
                (gamepad.digitalButton[2] ? B_BUTTON : 0) |
                (gamepad.digitalButton[6] ? Z_TRIG : 0);
        }
    }


    for (int i = 1; i < _osCont_numControllers; i++) {
        pad[i].errno = 0;
        pad[i].button = 0;
        pad[i].stick_x = 0;
        pad[i].stick_y = 0;
    }

    // Original:
    // OSContPackedRead *spc;
    // OSContPackedRead sp4;
    // s32 i;
    // spc = &D_80365CE0[0].read;
    // for (i = 0; i < _osCont_numControllers; i++, spc++, pad++) {
    //     sp4 = *spc;
    //     pad->errno = (sp4.unk02 & 0xc0) >> 4;
    //     if (pad->errno == 0) {
    //         pad->button = sp4.button;
    //         pad->stick_x = sp4.rawStickX;
    //         pad->stick_y = sp4.rawStickY;
    //     }
    // };
}

static const int CANVAS_WIDTH = 640;
static const int CANVAS_HEIGHT = 480;
static const size_t F3D_DMEM_LENGTH = 0x10000;
static u8 sF3DDmem[F3D_DMEM_LENGTH] = {0};
static const size_t VERTEX_BUFFER_LENGTH = 0x100000;
static u8 sVertexBuffer[VERTEX_BUFFER_LENGTH] = {0};
static GLuint sGLVertexBuffer = 0;
static Mat4 sProjection;
static const int NUM_MODELVIEW_MATS = 32;
static Mat4 sModelView[NUM_MODELVIEW_MATS];
static int sModelViewNum = 0;
static GLuint sProgram = 0;
static GLint sAPosition = 0;
static GLint sAColor = 0;
static GLint sATexCoord = 0;
static GLint sUModelView = 0;
static GLint sUProjection = 0;
static GLint sUTexScale = 0;
static GLint sUTexture0 = 0;
static const size_t VERTEX_STREAM_LENGTH = VERTEX_BUFFER_LENGTH * 2;
static u8 sVertexStream[VERTEX_STREAM_LENGTH] = {0};
static size_t sVertexStreamOffset = 0;
static u32 sOtherModeL = 0;
static u32 sOtherModeH = 0;
static Vp sViewport = {0};

struct TileDescriptor {
    bool on = false;
    // Set by G_TEXTURE
    int level = 0;
    int scaleS = 1;
    int scaleT = 1;
    // Set by G_SETTILESIZE
    int uls = 0;
    int ult = 0;
    int lrs = 0;
    int lrt = 0;
    // Set by G_SETTILE
    int fmt = 0;
    int siz = 0;
    int line = 0; // Number of 64-bit values per row
    int tmem = 0;
    int palette = 0;
    int cmS = 0;
    int maskS = 0;
    int shiftS = 0;
    int cmT = 0;
    int maskT = 0;
    int shiftT = 0;

    int getWidth() const {
        if (maskS != 0) {
            return 1 << maskS;
        } else {
            return ((lrs - uls) >> 2) + 1;
        }
    }

    int getHeight() const {
        if (maskT != 0) {
            return 1 << maskT;
        } else {
            return ((lrt - ult) >> 2) + 1;
        }
    }
};

static const int NUM_TILE_DESCRIPTORS = 8;
static TileDescriptor sTiles[NUM_TILE_DESCRIPTORS];
static const int TMEM_SIZE = 4 * 1024;
static u8 sTmem[TMEM_SIZE] = {0};
static const u8* sImageAddr = nullptr;
static int sImageSiz = 0;

static GLuint sTexture0 = 0;
static GLuint sErrorTexture = 0;

static int getSizBitsPerPixel(int siz) {
    switch (siz) {
    case G_IM_SIZ_4b: return 4;
    case G_IM_SIZ_8b: return 8;
    case G_IM_SIZ_16b: return 16;
    case G_IM_SIZ_32b: return 32;
    default:
        printf("Error: Unhandled image siz %d\n", siz);
        return 8;
    }
}

static u8 expand3to8(unsigned int n) {
    return (u8)(((n << (8 - 3)) | (n << (8 - 6)) | (n >> (9 - 8))) & 0xFF);
}

static u8 expand4to8(unsigned int n) {
    return (u8)(((n << (8 - 4)) | (n >> (8 - 8))) & 0xFF);
}

static u8 expand5To8(unsigned int n) {
    return (u8)(((n << (8 - 5)) | (n >> (10 - 8))) & 0xFF);
}

static void translateTile_IA4(const TileDescriptor &tile, const u8* tmem) {
    int tileW = tile.getWidth();
    int tileH = tile.getHeight();
    bool deinterleave = false; // TODO
    int srcOffs = tile.tmem * 8;

    std::vector<uint8_t> buf(tileW * tileH * 4);

    int dstIdx = 0;
    for (int y = 0; y < tileH; y++) {
        int srcIdx = y * tile.line * 8;
        int di = deinterleave ? ((y & 1) << 2) : 0;
        for (int x = 0; x < tileW; x += 2) {
            unsigned int b = tmem[(srcOffs + (srcIdx ^ di))];
            u8 i0 = expand3to8((b >> 5) & 0x07);
            u8 a0 = ((b >> 4) & 0x01) ? 0xFF : 0x00;
            buf[dstIdx + 0] = i0;
            buf[dstIdx + 1] = i0;
            buf[dstIdx + 2] = i0;
            buf[dstIdx + 3] = a0;
            u8 i1 = expand3to8((b >> 1) & 0x07);
            u8 a1 = ((b >> 0) & 0x01) ? 0xFF : 0x00;
            buf[dstIdx + 4] = i1;
            buf[dstIdx + 5] = i1;
            buf[dstIdx + 6] = i1;
            buf[dstIdx + 7] = a1;
            srcIdx += 0x01;
            dstIdx += 0x08;
        }
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tileW, tileH, 0, GL_RGBA, GL_UNSIGNED_BYTE, &buf[0]);
}

static void translateTile_IA8(const TileDescriptor &tile, const u8* tmem) {
    int tileW = tile.getWidth();
    int tileH = tile.getHeight();
    bool deinterleave = false; // TODO
    int srcOffs = tile.tmem * 8;

    std::vector<uint8_t> buf(tileW * tileH * 4);

    int dstIdx = 0;
    for (int y = 0; y < tileH; y++) {
        int srcIdx = y * tile.line * 8;
        int di = deinterleave ? ((y & 1) << 2) : 0;
        for (int x = 0; x < tileW; x++) {
            int b = tmem[srcOffs + (srcIdx ^ di)];
            int i = expand4to8((b >> 4) & 0x0F);
            int a = expand4to8((b >> 0) & 0x0F);
            buf[dstIdx + 0] = i;
            buf[dstIdx + 1] = i;
            buf[dstIdx + 2] = i;
            buf[dstIdx + 3] = a;
            srcIdx += 0x01;
            dstIdx += 0x04;
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tileW, tileH, 0, GL_RGBA, GL_UNSIGNED_BYTE, &buf[0]);
}

static void translateTile_IA16(const TileDescriptor &tile, const u8* tmem) {
    int tileW = tile.getWidth();
    int tileH = tile.getHeight();
    bool deinterleave = false; // TODO
    int srcOffs = tile.tmem * 8;

    std::vector<uint8_t> buf(tileW * tileH * 4);

    int dstIdx = 0;
    for (int y = 0; y < tileH; y++) {
        int srcIdx = y * tile.line * 8;
        int di = deinterleave ? ((y & 1) << 2) : 0;
        for (int x = 0; x < tileW; x++) {
            u8 i = sTmem[(srcOffs + (srcIdx ^ di) + 0x00)];
            u8 a = sTmem[(srcOffs + (srcIdx ^ di) + 0x01)];
            buf[dstIdx + 0] = i;
            buf[dstIdx + 1] = i;
            buf[dstIdx + 2] = i;
            buf[dstIdx + 3] = a;
            srcIdx += 0x02;
            dstIdx += 0x04;
        }
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tileW, tileH, 0, GL_RGBA, GL_UNSIGNED_BYTE, &buf[0]);
}

static void r5g5b5a1(u8* dst, int dstOffs, int p) {
    dst[dstOffs + 0] = expand5To8((p & 0xF800) >> 11);
    dst[dstOffs + 1] = expand5To8((p & 0x07C0) >> 6);
    dst[dstOffs + 2] = expand5To8((p & 0x003E) >> 1);
    dst[dstOffs + 3] = (p & 0x0001) ? 0xFF : 0x00;
}

static u16 readbe16(const u8* p) {
    return (p[0] << 8) | p[1];
}

static void translateTile_RGBA16(const TileDescriptor &tile, const u8* tmem) {
    int tileW = tile.getWidth();
    int tileH = tile.getHeight();
    bool deinterleave = false; // TODO
    int srcOffs = tile.tmem * 8;

    std::vector<uint8_t> buf(tileW * tileH * 4);

    int dstIdx = 0;
    for (int y = 0; y < tileH; y++) {
        int srcIdx = y * tile.line * 8;
        int di = deinterleave ? ((y & 1) << 2) : 0;
        for (int x = 0; x < tileW; x++) {
            int p = readbe16(&tmem[srcOffs + (srcIdx ^ di)]);
            r5g5b5a1(&buf[0], dstIdx + 0, p);
            srcIdx += 0x02;
            dstIdx += 0x04;
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tileW, tileH, 0, GL_RGBA, GL_UNSIGNED_BYTE, &buf[0]);
}

static void bindErrorTexture() {
    if (!sErrorTexture) {
        glGenTextures(1, &sErrorTexture);
        glBindTexture(GL_TEXTURE_2D, sErrorTexture);
        static const u32 errorTexel = 0xFFFFFFFF;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &errorTexel);
    }

    glBindTexture(GL_TEXTURE_2D, sErrorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

static void bindTextures(const TileDescriptor& tile) {
    if (tile.getWidth() == 0 || tile.getHeight() == 0) {
        bindErrorTexture();
        glUniform2f(sUTexScale, 1.0f, 1.0f);
        return;
    }

    if (!sTexture0) {
        glGenTextures(1, &sTexture0);
    }

    glBindTexture(GL_TEXTURE_2D, sTexture0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glUniform2f(sUTexScale,
        (1.0f / 32.0f) * (1.0f / tile.getWidth()),
        (1.0f / 32.0f) * (1.0f / tile.getHeight()));

    switch ((tile.fmt << 4) | tile.siz) {
    case ((G_IM_FMT_RGBA << 4) | G_IM_SIZ_16b):
        translateTile_RGBA16(tile, sTmem);
        break;
    case ((G_IM_FMT_IA << 4) | G_IM_SIZ_4b):
        translateTile_IA4(tile, sTmem);
        break;
    case ((G_IM_FMT_IA << 4) | G_IM_SIZ_8b):
        translateTile_IA8(tile, sTmem);
        break;
    case ((G_IM_FMT_IA << 4) | G_IM_SIZ_16b):
        translateTile_IA16(tile, sTmem);
        break;
    default:
        bindErrorTexture();
        glUniform2f(sUTexScale, 1.0f, 1.0f);
        printf("Error: Unhandled image fmt %d, siz %d\n", (int)tile.fmt, (int)tile.siz);
        return;
    }
}

static void loadBlock(const TileDescriptor &tile, int texels) {
    if (!sImageAddr) {
        return;
    }

    int tmemAddr = tile.tmem * 8; // TODO: CI textures have different addresses; also, use uls, ult.
    int numBytes = (getSizBitsPerPixel(sImageSiz) * (texels + 1) + 7) / 8;
    if ((numBytes - tmemAddr) > TMEM_SIZE) {
        numBytes = TMEM_SIZE - tmemAddr;
    }
    if (numBytes < 0) {
        numBytes = 0;
    }

    // printf("loading %d texture bytes from %p\n", (int)numBytes, sImageAddr);
    memcpy(&sTmem[tmemAddr], sImageAddr, numBytes);
}

static const char* VERTEX_SHADER = R"(
precision mediump float;

attribute vec3 aPosition;
attribute vec4 aColor;
attribute vec2 aTexCoord;
varying vec4 vViewPos;
varying vec4 vColor;
varying vec2 vTexCoord;
uniform mat4 uModelView;
uniform mat4 uProjection;
uniform vec2 uTexScale;

void main() {
    vViewPos = uModelView * vec4(aPosition, 1.0);
    gl_Position = uProjection * vViewPos;
    vColor = aColor;
    vTexCoord = aTexCoord * uTexScale;
}
)";

static const char* FRAGMENT_SHADER = R"(
precision mediump float;

varying vec4 vViewPos;
varying vec4 vColor;
varying vec2 vTexCoord;

uniform sampler2D uTexture0;

void main() {
    gl_FragColor = texture2D(uTexture0, vTexCoord) * vColor;
}
)";

static GLuint compileShader(GLuint type, const char* source) {
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        GLchar log[0x10000] = {0};
        glGetShaderInfoLog(shader, 0x10000, NULL, log);
        printf("Error: Failed to compile shader. Log:\n%s\n", log);
    }

    return shader;
}

static GLuint linkProgram(GLuint vertShader, GLuint fragShader) {
    GLuint program = glCreateProgram();

    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    glValidateProgram(program);
    
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        GLchar log[0x10000] = {0};
        glGetProgramInfoLog(program, 0x10000, NULL, log);
        printf("Error: Failed to link program. Log:\n%s\n", log);
    }

    return program;
}

static const float fixed2Float(int fixed, int fractBits) {
    return float(fixed) / (1 << fractBits);
}

static void flushVertexStream() {
    if (sVertexStreamOffset <= 0) {
        return;
    }

    glUseProgram(sProgram);
    glBindBuffer(GL_ARRAY_BUFFER, sGLVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sVertexStreamOffset, sVertexStream, GL_STREAM_DRAW);
    glEnableVertexAttribArray(sAPosition);
    glVertexAttribPointer(sAPosition, 3, GL_SHORT, GL_FALSE, sizeof(Vtx), 0);
    glEnableVertexAttribArray(sAColor);
    glVertexAttribPointer(sAColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), reinterpret_cast<const void*>(12));
    glEnableVertexAttribArray(sATexCoord);
    glVertexAttribPointer(sATexCoord, 2, GL_SHORT, GL_FALSE, sizeof(Vtx), reinterpret_cast<const void*>(8));
    glUniformMatrix4fv(sUProjection, 1, GL_FALSE, &sProjection[0][0]);
    glUniformMatrix4fv(sUModelView, 1, GL_FALSE, &sModelView[sModelViewNum][0][0]);

    bindTextures(sTiles[0]); // FIXME: don't always use texture 0?
    glUniform1i(sUTexture0, 0);

    float vscale[4];
    float vtrans[4];
    vscale[0] = fixed2Float(sViewport.vp.vscale[1], 2);
    vscale[1] = fixed2Float(sViewport.vp.vscale[0], 2);
    vscale[2] = fixed2Float(sViewport.vp.vscale[3], 10);
    vscale[3] = sViewport.vp.vscale[2];
    vtrans[0] = fixed2Float(sViewport.vp.vtrans[1], 2);
    vtrans[1] = fixed2Float(sViewport.vp.vtrans[0], 2);
    vtrans[2] = fixed2Float(sViewport.vp.vtrans[3], 10);
    vtrans[3] = sViewport.vp.vtrans[2];

    // printf("vscale %f,%f,%f,%f; vtrans %f,%f,%f,%f\n",
    //     vscale[0], vscale[1], vscale[2], vscale[3],
    //     vtrans[0], vtrans[1], vtrans[2], vtrans[3]);

    float vpx = vtrans[0] - vscale[0];
    float vpy = vtrans[1] - vscale[1];
    float vpw = abs(vscale[0]) * 2;
    float vph = abs(vscale[1]) * 2;
    float vpnz = vtrans[2] - vscale[2];
    float vpfz = vtrans[2] + vscale[2];

    //printf("vpx %f vpy %f vpw %f vph %f vpnz %f vpfz %f\n", vpx, vpy, vpw, vph, vpnz, vpfz);

    glDepthMask((sOtherModeL & Z_UPD) ? GL_TRUE : GL_FALSE);
    if (sOtherModeL & Z_CMP) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDrawArrays(GL_TRIANGLES, 0, sVertexStreamOffset / sizeof(Vtx));
    sVertexStreamOffset = 0;
}

static void initF3D() {
    emscripten_set_canvas_element_size("#canvas", CANVAS_WIDTH, CANVAS_HEIGHT);

    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attr);
    emscripten_webgl_make_context_current(ctx);

    glViewport(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);

    GLuint vertShader = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    sProgram = linkProgram(vertShader, fragShader);
    sAPosition = glGetAttribLocation(sProgram, "aPosition");
    sAColor = glGetAttribLocation(sProgram, "aColor");
    sATexCoord = glGetAttribLocation(sProgram, "aTexCoord");
    sUModelView = glGetUniformLocation(sProgram, "uModelView");
    sUProjection = glGetUniformLocation(sProgram, "uProjection");
    sUTexScale = glGetUniformLocation(sProgram, "uTexScale");
    sUTexture0 = glGetUniformLocation(sProgram, "uTexture0");

    sModelViewNum = 0;

    mtxf_identity(sProjection);
    for (int i = 0; i < NUM_MODELVIEW_MATS; i++) {
        mtxf_identity(sModelView[i]);
    }

    glGenBuffers(1, &sGLVertexBuffer);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    sVertexStreamOffset = 0;

    sOtherModeL = 0;
    sOtherModeH = 0;
}

static void runF3D(const Gfx* commands, u32 numEntries) {
    std::stack<const Gfx*> returnAddrs;

    bool done = false;
    while (!done) {
        // printf("running F3D data_ptr %p entries %u\n", data, entries);
        Gfx gfx = *commands;
        // printf("@%p: 0x%.08X 0x%.08X\n", commands, gfx.words.w0, gfx.words.w1);
        commands++;

        int cmd = (gfx.words.w0 >> 24) & 0xFF;
        switch (cmd) {
        case G_MTX & 0xFF: {
            flushVertexStream();
            int param = (gfx.words.w0 >> 16) & 0xFF;
            const Mtx* src = reinterpret_cast<const Mtx*>(gfx.words.w1);
            Mat4 mat;
            guMtxL2F(mat, src);
            if (param & G_MTX_PROJECTION) {
                if (param & G_MTX_LOAD) {
                    mtxf_copy(sProjection, mat);
                } else {
                    MultMatrix(sProjection, mat, sProjection);
                }
            } else {
                if ((param & G_MTX_PUSH) && (sModelViewNum < NUM_MODELVIEW_MATS - 1)) {
                    mtxf_copy(sModelView[sModelViewNum + 1], sModelView[sModelViewNum]);
                    sModelViewNum++;
                }
                if (param & G_MTX_LOAD) {
                    mtxf_copy(sModelView[sModelViewNum], mat);
                } else {
                    MultMatrix(sModelView[sModelViewNum], mat, sModelView[sModelViewNum]);
                }
            }
            break;
        }
        case G_MOVEMEM & 0xFF: {
            int i = (gfx.words.w0 >> 16) & 0xFF;
            int n = gfx.words.w0 & 0xFFFF;
            const void* src = reinterpret_cast<const void*>(gfx.words.w1);
            switch (i) {
            case G_MV_VIEWPORT:
                // printf("Updated viewport\n");
                memcpy(&sViewport, src, (n > sizeof(Vp) ? sizeof(Vp) : n));
                break;
            default:
                // TODO
                break;
            }
            break;
        }
        case G_VTX & 0xFF: {
            int numVerts = ((gfx.words.w0 >> 20) & 0xF) + 1; // Ignored?
            int dstOffs = (gfx.words.w0 >> 16) & 0xF;
            int length = gfx.words.w0 & 0xFFFF; // Or is this ignored?
            const void* src = reinterpret_cast<const void*>(gfx.words.w1);
            if (sEnterPressed) {
                //printf("vtx src %p\n", src);
            }
            memcpy(&sVertexBuffer[dstOffs * sizeof(Vtx)], src, numVerts * sizeof(Vtx));
            break;
        }
        case G_DL & 0xFF: {
            int8_t noStore = (gfx.words.w0 >> 16) & 0xFF;
            if (noStore == 0) {
                returnAddrs.push(commands);
            }
            commands = reinterpret_cast<const Gfx*>(gfx.words.w1);
            break;
        }
        case G_ENDDL & 0xFF:
            if (returnAddrs.empty()) {
                done = true;
            } else {
                commands = returnAddrs.top();
                returnAddrs.pop();
            }
            break;
        case G_TRI1 & 0xFF: {
            int a = ((gfx.words.w1 >> 16) & 0xFF) / 10; // Divide by 10? wtf? but it's true.
            int b = ((gfx.words.w1 >> 8) & 0xFF) / 10;
            int c = (gfx.words.w1 & 0xFF) / 10;
            u8 buf[sizeof(Vtx) * 3];
            if (sVertexStreamOffset + sizeof(Vtx) * 3 > VERTEX_BUFFER_LENGTH) {
                flushVertexStream();
            }
            memcpy(&buf[0],               &sVertexBuffer[a * sizeof(Vtx)], sizeof(Vtx));
            memcpy(&buf[sizeof(Vtx)],     &sVertexBuffer[b * sizeof(Vtx)], sizeof(Vtx));
            memcpy(&buf[sizeof(Vtx) * 2], &sVertexBuffer[c * sizeof(Vtx)], sizeof(Vtx));
            memcpy(&sVertexStream[sVertexStreamOffset], buf, sizeof(Vtx) * 3);
            sVertexStreamOffset += sizeof(Vtx) * 3;
            break;
        }
        case G_RDPPIPESYNC & 0xFF:
        case G_RDPTILESYNC & 0xFF:
        case G_RDPLOADSYNC & 0xFF:
        case G_RDPFULLSYNC & 0xFF:
            // Ignored
            flushVertexStream(); // TODO: move elsewhere.
            break;
        case G_POPMTX & 0xFF:
            if (gfx.words.w1 == 0 /* modelview */ && sModelViewNum > 0) {
                sModelViewNum--;
            }
            break;
        case G_CLEARGEOMETRYMODE & 0xFF:
            // TODO
            break;
        case G_SETGEOMETRYMODE & 0xFF:
            // TODO
            break;
        case G_SETOTHERMODE_L & 0xFF: {
            int s = (gfx.words.w0 >> 8) & 0xFF;
            int n = gfx.words.w0 & 0xFF;
            u32 mask = ((1 << n) - 1) << s;
            u32 d = gfx.words.w1;
            sOtherModeL &= ~mask;
            sOtherModeL |= d;
            break;
        }
        case G_SETOTHERMODE_H & 0xFF: {
            int s = (gfx.words.w0 >> 8) & 0xFF;
            int n = gfx.words.w0 & 0xFF;
            u32 mask = ((1 << n) - 1) << s;
            u32 d = gfx.words.w1;
            sOtherModeH &= ~mask;
            sOtherModeH |= d;
            break;
        }
        case G_RDPHALF_1 & 0xFF:
            // TODO
            break;
        case G_RDPHALF_2 & 0xFF:
            // TODO
            break;
        case G_TEXTURE & 0xFF:
            // TODO
            break;
        case G_MOVEWORD & 0xFF: {
            int o = (gfx.words.w0 >> 8) & 0xFFFF;
            int i = gfx.words.w0 & 0xFF;
            //printf("G_MOVEWORD o 0x%.04X, i 0x%.02X, d 0x%.08X\n", o, i, gfx.words.w1);
            // TODO
            break;
        }
        case G_SETSCISSOR & 0xFF: {
            int x = (gfx.words.w0 >> 12) & 0xFFF;
            int y = gfx.words.w0 & 0xFFF;
            int v = (gfx.words.w1 >> 12) & 0xFFF;
            int w = gfx.words.w1 & 0xFFF;
            // FIXME: why 1280, 960? Guess: it's the whole screen.
            glScissor(x * CANVAS_WIDTH / 1280, y * CANVAS_HEIGHT / 960,
                v * CANVAS_WIDTH / 1280, w * CANVAS_HEIGHT / 960);
            break;
        }
        case G_SETCOMBINE & 0xFF:
            // TODO
            break;
        case G_LOADBLOCK & 0xFF: {
            flushVertexStream();
            int uls = (gfx.words.w0 >> 12) & 0xFFF;
            int ult = gfx.words.w0 & 0xFFF;
            int tile = (gfx.words.w1 >> 24) & 0x7;
            int texels = (gfx.words.w1 >> 12) & 0xFFF;
            int dxt = gfx.words.w1 & 0xFFF;
            loadBlock(sTiles[tile], texels);
            break;
        }
        case G_SETFILLCOLOR & 0xFF:
            // TODO
            break;
        case G_SETPRIMCOLOR & 0xFF:
            // TODO
            break;
        case G_SETENVCOLOR & 0xFF:
            // TODO
            break;
        case G_SETBLENDCOLOR & 0xFF:
            // TODO
            break;
        case G_SETFOGCOLOR & 0xFF:
            // TODO
            break;
        case G_FILLRECT & 0xFF:
            // TODO
            break;
        case G_TEXRECT & 0xFF:
            // TODO
            break;
        case G_SETTILESIZE & 0xFF: {
            flushVertexStream();
            int tileNum = (gfx.words.w1 >> 24) & 0x7;
            TileDescriptor& tile = sTiles[tileNum];
            tile.uls = (gfx.words.w0 >> 12) & 0xFFF;
            tile.ult = gfx.words.w0 & 0xFFF;
            tile.lrs = (gfx.words.w1 >> 12) & 0xFFF;
            tile.lrt = gfx.words.w1 & 0xFFF;
            break;
        }
        case G_SETTILE & 0xFF: {
            flushVertexStream();
            int tileNum = (gfx.words.w1 >> 24) & 0x7;
            TileDescriptor& tile = sTiles[tileNum];
            tile.fmt = (gfx.words.w0 >> 21) & 0x7;
            tile.siz = (gfx.words.w0 >> 19) & 0x3;
            tile.line = (gfx.words.w0 >> 9) & 0x1FF;
            tile.tmem = gfx.words.w0 & 0x1FF;
            tile.palette = (gfx.words.w1 >> 20) & 0xF;
            tile.cmT = (gfx.words.w1 >> 18) & 0x3;
            tile.maskT = (gfx.words.w1 >> 14) & 0xF;
            tile.shiftT = (gfx.words.w1 >> 10) & 0xF;
            tile.cmS = (gfx.words.w1 >> 8) & 0x3;
            tile.maskS = (gfx.words.w1 >> 4) & 0xF;
            tile.shiftS = gfx.words.w1 & 0xF;
            break;
        }
        case G_SETTIMG & 0xFF: {
            flushVertexStream();
            int fmt = (gfx.words.w0 >> 21) & 0x7;
            sImageSiz = (gfx.words.w0 >> 19) & 0x3;
            int w = (gfx.words.w0 & 0xFFF) + 1;
            sImageAddr = reinterpret_cast<const u8*>(gfx.words.w1);
            break;
        }
        case G_SETZIMG & 0xFF:
            // TODO
            break;
        case G_SETCIMG & 0xFF:
            // TODO
            break;
        default:
            printf("Error: Unknown DL opcode 0x%.02X\n", (int)cmd);
            break;
        }
    }

    // FIXME: should this trigger on ENDDL?
    if (gExceptionTable[OS_EVENT_DP].queue) {
        osSendMesg(gExceptionTable[OS_EVENT_DP].queue, gExceptionTable[OS_EVENT_DP].msg, OS_MESG_NOBLOCK);
    }
}

void osSpTaskStartGo(UNUSED OSTask *task) {
    // TODO
    // printf("starting sp task ucode %p, data_ptr %p, data_size %u\n",
    //     task->t.ucode, task->t.ucode, task->t.data_ptr);
    if (task->t.ucode == rspF3DStart) {
        const Gfx* commands = reinterpret_cast<const Gfx*>(task->t.data_ptr);
        u32 numEntries = task->t.data_size / sizeof(Gfx);
        runF3D(commands, numEntries);
    }

    if (gExceptionTable[OS_EVENT_SP].queue) {
        osSendMesg(gExceptionTable[OS_EVENT_SP].queue, gExceptionTable[OS_EVENT_SP].msg, OS_MESG_NOBLOCK);
    }
}

}

static double sTime = 0.0;
static const double FRAME_DELTA = 1000.0 / 30.0; // 30fps
static struct LevelCommand *s_addr;

// See src/game/game.cpp, thread5_game_loop
void game_loop() {
    double newTime = emscripten_get_now();
    if ((newTime - sTime) < FRAME_DELTA) {
        return;
    }
    sTime += FRAME_DELTA;

    struct LevelCommand* &addr = s_addr;
    
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // See src/game/game.cpp, thread5_game_loop

    // if the reset timer is active, run the process to reset the game.
    if (gResetTimer) {
        func_80247D84();
        return;
    }
    profiler_log_thread5_time(THREAD5_START);

    // if any controllers are plugged in, start read the data for when
    // read_controller_inputs is called later.
    if (gControllerBits) {
        osContStartReadData(&gSIEventMesgQueue);
    }

    audio_game_loop_tick();
    func_80247FAC();
    osSendMesg(gExceptionTable[OS_EVENT_SI].queue, gExceptionTable[OS_EVENT_SI].msg, OS_MESG_NOBLOCK);
    read_controller_inputs();
    addr = level_script_execute(addr);
    // display_and_vsync();

    // when debug info is enabled, print the "BUF %d" information.
    if (gShowDebugText) {
        // subtract the end of the gfx pool with the display list to obtain the
        // amount of free space remaining.
        print_text_fmt_int(180, 20, "BUF %d", gGfxPoolEnd - (u8 *) gDisplayListHead);
    }
    
    // See src/game/main.c, thread3_main
    OSMesg msg;

    // Empty out the message queue
    while (osRecvMesg(&gIntrMesgQueue, &msg, OS_MESG_NOBLOCK) == 0) {
        switch ((uintptr_t) msg) {
            case MESG_VI_VBLANK:
                // handle_vblank();
                break;
            case MESG_SP_COMPLETE:
                handle_sp_complete();
                break;
            case MESG_DP_COMPLETE:
                handle_dp_complete();
                break;
            case MESG_START_GFX_SPTASK:
                start_gfx_sptask();
                break;
            case MESG_NMI_REQUEST:
                handle_nmi_request();
                break;
        }
        Dummy802461DC();
    }

    // See display.c, display_and_vsync
    handle_vblank();
    //handle_vblank(); // Handle two fields
    profiler_log_thread5_time(BEFORE_DISPLAY_LISTS);
    // osRecvMesg(&D_80339CB8, &D_80339BEC, OS_MESG_BLOCK);
    if (D_8032C6A0 != NULL) {
        D_8032C6A0();
        D_8032C6A0 = NULL;
    }
    send_display_list(&gGfxPool->spTask);
    profiler_log_thread5_time(AFTER_DISPLAY_LISTS);
    // osRecvMesg(&gGameVblankQueue, &D_80339BEC, OS_MESG_BLOCK);
    osViSwapBuffer((void *) PHYSICAL_TO_VIRTUAL(gPhysicalFrameBuffers[sCurrFBNum]));
    profiler_log_thread5_time(THREAD5_END);
    // osRecvMesg(&gGameVblankQueue, &D_80339BEC, OS_MESG_BLOCK);
    if (++sCurrFBNum == 3) {
        sCurrFBNum = 0;
    }
    if (++frameBufferIndex == 3) {
        frameBufferIndex = 0;
    }
    gGlobalTimer++;
}

void setup_game_loop() {
    struct LevelCommand *addr;

    printf("sizeof(Vtx) %d\n", sizeof(Vtx));
    printf("rspF3DBootStart %p\n", rspF3DBootStart);
    printf("rspF3D %p\n", rspF3D);
    printf("rspAspMainStart %p\n", rspAspMainStart);
    printf("rspF3DDataStart %p\n", rspF3DDataStart);
    printf("rspF3DStart %p\n", rspF3DStart);
    
    // See src/game/main.c, Main
    osInitialize();
    Dummy802461CC();

    // See src/game/main.c, thread1_idle
#ifdef VERSION_US
    s32 sp24 = osTvType;
#endif

    osCreateViManager(OS_PRIORITY_VIMGR);
#ifdef VERSION_US
    if (sp24 == TV_TYPE_NTSC) {
        osViSetMode(&osViModeTable[OS_VI_NTSC_LAN1]);
    } else {
        osViSetMode(&osViModeTable[OS_VI_PAL_LAN1]);
    }
#else
    osViSetMode(&osViModeTable[OS_VI_NTSC_LAN1]);
#endif
    osViBlack(TRUE);
    osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON);
    osViSetSpecialFeatures(OS_VI_GAMMA_OFF);
    osCreatePiManager(OS_PRIORITY_PIMGR, &gPIMesgQueue, gPIMesgBuf, ARRAY_COUNT(gPIMesgBuf));
    create_thread(&gMainThread, 3, thread3_main, NULL, gThread3Stack + 0x2000, 100);
    if (D_8032C650 == 0) {
        osStartThread(&gMainThread);
    }
    osSetThreadPri(NULL, 0);

    // See src/game/main.c, thread3_main
    setup_mesg_queues();
    AllocPool();
    load_engine_code_segment();

    // See src/game/main.c, thread4_sound
    audio_init();
    sound_init();

    // Zero-out unused vector
    vec3f_copy(unused80339DC0, gVec3fZero);

    osCreateMesgQueue(&sSoundMesgQueue, sSoundMesgBuf, ARRAY_COUNT(sSoundMesgBuf));
    set_vblank_handler(1, &sSoundVblankHandler, &sSoundMesgQueue, (OSMesg*) 512);

    // See src/game/game.c, thread5_game_loop
    setup_game_memory();
    init_controllers();
    save_file_load_all();

    set_vblank_handler(2, &gGameVblankHandler, &gGameVblankQueue, (OSMesg*) 1);

    // point addr to the entry point into the level script data.
    addr = (struct LevelCommand*)segmented_to_virtual(level_script_entry);

    play_music(2, SEQUENCE_ARGS(0, SEQ_SOUND_PLAYER), 0);
    set_sound_mode(save_file_get_sound_mode());
    func_80247ED8();

    s_addr = addr;
    sTime = emscripten_get_now();
    emscripten_set_main_loop(game_loop, 0, 0);
}

static EM_BOOL onKeyDown(int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData) {
    std::string key(keyEvent->code);
    if (key == "Enter") {
        sEnterPressed = true;
        return EM_TRUE;
    } else if (key == "ArrowLeft") {
        sArrowLeftPressed = true;
        return EM_TRUE;
    } else if (key == "ArrowRight") {
        sArrowRightPressed = true;
        return EM_TRUE;
    } else if (key == "ArrowUp") {
        sArrowUpPressed = true;
        return EM_TRUE;
    } else if (key == "ArrowDown") {
        sArrowDownPressed = true;
        return EM_TRUE;
    } else if (key == "KeyZ") {
        sZPressed = true;
        return EM_TRUE;
    } else if (key == "KeyX") {
        sXPressed = true;
        return EM_TRUE;
    }
    return EM_FALSE;
}

static EM_BOOL onKeyUp(int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData) {
    std::string key(keyEvent->code);
    if (key == "Enter") {
        sEnterPressed = false;
        return EM_TRUE;
    } else if (key == "ArrowLeft") {
        sArrowLeftPressed = false;
        return EM_TRUE;
    } else if (key == "ArrowRight") {
        sArrowRightPressed = false;
        return EM_TRUE;
    } else if (key == "ArrowUp") {
        sArrowUpPressed = false;
        return EM_TRUE;
    } else if (key == "ArrowDown") {
        sArrowDownPressed = false;
        return EM_TRUE;
    } else if (key == "KeyZ") {
        sZPressed = false;
        return EM_TRUE;
    } else if (key == "KeyX") {
        sXPressed = false;
        return EM_TRUE;
    }
    printf("keyup: %s\n", keyEvent->code);
    return EM_FALSE;
}

int main() {
    emscripten_set_keydown_callback("#canvas", NULL, false, onKeyDown);
    emscripten_set_keyup_callback("#canvas", NULL, false, onKeyUp);
    initF3D();
    setup_game_loop();
    return 0;
}
