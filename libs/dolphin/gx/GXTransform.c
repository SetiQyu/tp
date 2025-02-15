#include "dolphin/gx/GXTransform.h"
#include "dolphin/gx.h"

void __GXSetMatrixIndex();

static void Copy6Floats(register f32 src[6], register f32 dst[6]) {
    register f32 ps_0, ps_1, ps_2;

    // clang-format off
    asm {
        psq_l  ps_0,  0(src), 0, 0
        psq_l  ps_1,  8(src), 0, 0
        psq_l  ps_2, 16(src), 0, 0
        psq_st ps_0,  0(dst), 0, 0
        psq_st ps_1,  8(dst), 0, 0
        psq_st ps_2, 16(dst), 0, 0
    }
    // clang-format on
}

static void WriteProjPS(const register f32 src[6], register volatile void* dst) {
    register f32 ps_0, ps_1, ps_2;

    // clang-format off
    asm {
        psq_l  ps_0,  0(src), 0, 0
        psq_l  ps_1,  8(src), 0, 0
        psq_l  ps_2, 16(src), 0, 0
        psq_st ps_0,  0(dst), 0, 0
        psq_st ps_1,  0(dst), 0, 0
        psq_st ps_2,  0(dst), 0, 0
    }
    // clang-format on
}

/* 8035FF60-803600D4 35A8A0 0174+00 0/0 1/1 0/0 .text            GXProject */
void GXProject(f32 model_x, f32 model_y, f32 model_z, Mtx model_mtx, f32* proj_mtx, f32* viewpoint,
               f32* screen_x, f32* screen_y, f32* screen_z) {
    f32 sp10[3];
    f32 var_f30;
    f32 var_f29;
    f32 var_f28;
    f32 var_f31;

    ASSERT(proj_mtx != NULL && viewpoint != NULL && screen_x != NULL && screen_y != NULL &&
               screen_z != NULL,
           "GXGet*: invalid null pointer");

    sp10[0] = (model_mtx[0][0] * model_x) + (model_mtx[0][1] * model_y) +
              (model_mtx[0][2] * model_z) + model_mtx[0][3];
    sp10[1] = (model_mtx[1][0] * model_x) + (model_mtx[1][1] * model_y) +
              (model_mtx[1][2] * model_z) + model_mtx[1][3];
    sp10[2] = (model_mtx[2][0] * model_x) + (model_mtx[2][1] * model_y) +
              (model_mtx[2][2] * model_z) + model_mtx[2][3];

    if (proj_mtx[0] == 0.0f) {
        var_f30 = (sp10[0] * proj_mtx[1]) + (sp10[2] * proj_mtx[2]);
        var_f29 = (sp10[1] * proj_mtx[3]) + (sp10[2] * proj_mtx[4]);
        var_f28 = proj_mtx[6] + (sp10[2] * proj_mtx[5]);
        var_f31 = 1.0f / -sp10[2];
    } else {
        var_f30 = proj_mtx[2] + (sp10[0] * proj_mtx[1]);
        var_f29 = proj_mtx[4] + (sp10[1] * proj_mtx[3]);
        var_f28 = proj_mtx[6] + (sp10[2] * proj_mtx[5]);
        var_f31 = 1.0f;
    }

    *screen_x = (viewpoint[2] / 2) + (viewpoint[0] + (var_f31 * (var_f30 * viewpoint[2] / 2)));
    *screen_y = (viewpoint[3] / 2) + (viewpoint[1] + (var_f31 * (-var_f29 * viewpoint[3] / 2)));
    *screen_z = viewpoint[5] + (var_f31 * (var_f28 * (viewpoint[5] - viewpoint[4])));
}

void __GXSetProjection(void) {
    GX_XF_LOAD_REGS(6, GX_XF_REG_PROJECTIONA);
    WriteProjPS(__GXData->projMtx, (volatile void*)GXFIFO_ADDR);
    GX_WRITE_U32(__GXData->projType);
}

/* 803600D4-80360178 35AA14 00A4+00 0/0 15/15 2/2 .text            GXSetProjection */
void GXSetProjection(const Mtx44 proj, GXProjectionType type) {
    volatile void* fifo;
    __GXData->projType = type;

    __GXData->projMtx[0] = proj[0][0];
    __GXData->projMtx[2] = proj[1][1];
    __GXData->projMtx[4] = proj[2][2];
    __GXData->projMtx[5] = proj[2][3];

    if (type == GX_ORTHOGRAPHIC) {
        __GXData->projMtx[1] = proj[0][3];
        __GXData->projMtx[3] = proj[1][3];
    } else {
        __GXData->projMtx[1] = proj[0][2];
        __GXData->projMtx[3] = proj[1][2];
    }

    __GXSetProjection();

    __GXData->bpSentNot = GX_TRUE;
}

/* 80360178-80360204 35AAB8 008C+00 0/0 1/1 1/1 .text            GXSetProjectionv */
void GXSetProjectionv(f32* proj) {
    __GXData->projType = proj[0] == 0.0f ? GX_PERSPECTIVE : GX_ORTHOGRAPHIC;
    Copy6Floats(&proj[1], __GXData->projMtx);

    __GXSetProjection();
    __GXData->bpSentNot = GX_TRUE;
}

/* 80360204-8036024C 35AB44 0048+00 0/0 1/1 1/1 .text            GXGetProjectionv */
void GXGetProjectionv(f32* proj) {
    *proj = (u32)__GXData->projType != GX_PERSPECTIVE ? 1.0f : 0.0f;
    Copy6Floats(__GXData->projMtx, &proj[1]);
}

static void WriteMTXPS4x3(register volatile void* dst, register const Mtx src) {
    register f32 ps_0, ps_1, ps_2, ps_3, ps_4, ps_5;

    // clang-format off
    asm {
        psq_l  ps_0,  0(src), 0, 0
        psq_l  ps_1,  8(src), 0, 0
        psq_l  ps_2, 16(src), 0, 0
        psq_l  ps_3, 24(src), 0, 0
        psq_l  ps_4, 32(src), 0, 0
        psq_l  ps_5, 40(src), 0, 0

        psq_st ps_0, 0(dst),  0, 0
        psq_st ps_1, 0(dst),  0, 0
        psq_st ps_2, 0(dst),  0, 0
        psq_st ps_3, 0(dst),  0, 0
        psq_st ps_4, 0(dst),  0, 0
        psq_st ps_5, 0(dst),  0, 0
    }
    // clang-format on
}

/* 8036024C-8036029C 35AB8C 0050+00 0/0 83/83 9/9 .text            GXLoadPosMtxImm */
void GXLoadPosMtxImm(Mtx mtx, u32 id) {
    GX_XF_LOAD_REGS(4 * 3 - 1, id * 4 + GX_XF_MEM_POSMTX);
    WriteMTXPS4x3(&GXWGFifo, mtx);
}

static void WriteMTXPS3x3(register volatile void* dst, register const Mtx src) {
    register f32 ps_0, ps_1, ps_2, ps_3, ps_4, ps_5;

    // clang-format off
    asm {
        psq_l  ps_0,  0(src), 0, 0
        lfs    ps_1,  8(src)
        psq_l  ps_2, 16(src), 0, 0
        lfs    ps_3, 24(src)
        psq_l  ps_4, 32(src), 0, 0
        lfs    ps_5, 40(src)

        psq_st ps_0, 0(dst),  0, 0
        stfs   ps_1, 0(dst)
        psq_st ps_2, 0(dst),  0, 0
        stfs   ps_3, 0(dst)
        psq_st ps_4, 0(dst),  0, 0
        stfs   ps_5, 0(dst)
    }
    // clang-format on
}

/* 8036029C-803602EC 35ABDC 0050+00 0/0 11/11 7/7 .text            GXLoadNrmMtxImm */
void GXLoadNrmMtxImm(Mtx mtx, u32 id) {
    GX_XF_LOAD_REGS(3 * 3 - 1, id * 3 + GX_XF_MEM_NRMMTX);
    WriteMTXPS3x3(&GXWGFifo, mtx);
}

/* 803602EC-80360320 35AC2C 0034+00 0/0 51/51 2/2 .text            GXSetCurrentMtx */
void GXSetCurrentMtx(u32 id) {
    GX_SET_REG(__GXData->matIdxA, id, GX_XF_MTXIDX0_GEOM_ST, GX_XF_MTXIDX0_GEOM_END);
    __GXSetMatrixIndex(GX_VA_PNMTXIDX);
}

static void WriteMTXPS4x2(register volatile void* dst, register const Mtx src) {
    register f32 ps_0, ps_1, ps_2, ps_3;

    // clang-format off
    asm {
        psq_l  ps_0,  0(src), 0, 0
        psq_l  ps_1,  8(src), 0, 0
        psq_l  ps_2, 16(src), 0, 0
        psq_l  ps_3, 24(src), 0, 0

        psq_st ps_0, 0(dst),  0, 0
        psq_st ps_1, 0(dst),  0, 0
        psq_st ps_2, 0(dst),  0, 0
        psq_st ps_3, 0(dst),  0, 0
    }
    // clang-format on
}

/* 80360320-803603D4 35AC60 00B4+00 0/0 15/15 0/0 .text            GXLoadTexMtxImm */
void GXLoadTexMtxImm(const Mtx mtx, u32 id, GXTexMtxType type) {
    u32 addr;
    u32 num;
    u32 reg;

    // Matrix address in XF memory
    addr = id >= GX_PTTEXMTX0 ? (id - GX_PTTEXMTX0) * 4 + GX_XF_MEM_DUALTEXMTX :
                                id * 4 + (u64)GX_XF_MEM_POSMTX;

    // Number of elements in matrix
    num = type == GX_MTX2x4 ? (u64)(2 * 4) : 3 * 4;

    reg = addr | (num - 1) << 16;

    GX_XF_LOAD_REG_HDR(reg);

    if (type == GX_MTX3x4) {
        WriteMTXPS4x3(&GXWGFifo, mtx);
    } else {
        WriteMTXPS4x2(&GXWGFifo, mtx);
    }
}

/* 803603D4-80360464 35AD14 0090+00 1/1 0/0 0/0 .text            __GXSetViewport */
void __GXSetViewport(void) {
    f32 a, b, c, d, e, f;
    f32 near, far;

    a = __GXData->vpWd / 2;
    b = -__GXData->vpHt / 2;
    d = __GXData->vpLeft + (__GXData->vpWd / 2) + 342.0f;
    e = __GXData->vpTop + (__GXData->vpHt / 2) + 342.0f;

    near = __GXData->vpNearz * __GXData->zScale;
    far = __GXData->vpFarz * __GXData->zScale;

    c = far - near;
    f = far + __GXData->zOffset;

    GX_XF_LOAD_REGS(5, GX_XF_REG_SCALEX);
    GX_WRITE_F32(a);
    GX_WRITE_F32(b);
    GX_WRITE_F32(c);
    GX_WRITE_F32(d);
    GX_WRITE_F32(e);
    GX_WRITE_F32(f);
}

/* 80360464-803604AC 35ADA4 0048+00 0/0 10/10 1/1 .text            GXSetViewport */
void GXSetViewport(f32 left, f32 top, f32 width, f32 height, f32 nearZ, f32 farZ) {
    __GXData->vpLeft = left;
    __GXData->vpTop = top;
    __GXData->vpWd = width;
    __GXData->vpHt = height;
    __GXData->vpNearz = nearZ;
    __GXData->vpFarz = farZ;
    __GXSetViewport();
    __GXData->bpSentNot = GX_TRUE;
}

/* 803604AC-803604D0 35ADEC 0024+00 0/0 1/1 1/1 .text            GXGetViewportv */
void GXGetViewportv(f32* p) {
    Copy6Floats(&__GXData->vpLeft, p);
}

/* 803604D0-80360548 35AE10 0078+00 0/0 11/11 4/4 .text            GXSetScissor */
void GXSetScissor(u32 left, u32 top, u32 width, u32 height) {
    u32 y1, x1, y2, x2;
    u32 reg;

    y1 = top + 342;
    x1 = left + 342;

    GX_SET_REG(__GXData->suScis0, y1, GX_BP_SCISSORTL_TOP_ST, GX_BP_SCISSORTL_TOP_END);
    GX_SET_REG(__GXData->suScis0, x1, GX_BP_SCISSORTL_LEFT_ST, GX_BP_SCISSORTL_LEFT_END);

    y2 = y1 + height - 1;
    x2 = (x1 + width) - 1;

    GX_SET_REG(__GXData->suScis1, y2, GX_BP_SCISSORBR_BOT_ST, GX_BP_SCISSORBR_BOT_END);
    GX_SET_REG(__GXData->suScis1, x2, GX_BP_SCISSORBR_RIGHT_ST, GX_BP_SCISSORBR_RIGHT_END);

    GX_BP_LOAD_REG(__GXData->suScis0);
    GX_BP_LOAD_REG(__GXData->suScis1);
    __GXData->bpSentNot = FALSE;
}

/* 80360548-80360590 35AE88 0048+00 0/0 6/6 2/2 .text            GXGetScissor */
void GXGetScissor(u32* left, u32* top, u32* width, u32* height) {
    u32 y1 = (__GXData->suScis0 & 0x0007FF) >> 0;
    u32 x1 = (__GXData->suScis0 & 0x7FF000) >> 12;
    u32 y2 = (__GXData->suScis1 & 0x0007FF) >> 0;
    u32 x2 = (__GXData->suScis1 & 0x7FF000) >> 12;

    *left = x1 - 0x156;
    *top = y1 - 0x156;
    *width = (x2 - x1) + 1;
    *height = (y2 - y1) + 1;
}

/* 80360590-803605D0 35AED0 0040+00 0/0 1/1 0/0 .text            GXSetScissorBoxOffset */
void GXSetScissorBoxOffset(s32 x, s32 y) {
    u32 cmd = 0;
    u32 x1;
    u32 y1;

    x1 = (u32)(x + 342) / 2;
    y1 = (u32)(y + 342) / 2;
    GX_SET_REG(cmd, x1, GX_BP_SCISSOROFS_OX_ST, GX_BP_SCISSOROFS_OX_END);
    GX_SET_REG(cmd, y1, GX_BP_SCISSOROFS_OY_ST, GX_BP_SCISSOROFS_OY_END);

    GX_SET_REG(cmd, GX_BP_REG_SCISSOROFFSET, 0, 7);

    GX_BP_LOAD_REG(cmd);
    __GXData->bpSentNot = GX_FALSE;
}

/* 803605D0-803605F8 35AF10 0028+00 0/0 27/27 2/2 .text            GXSetClipMode */
void GXSetClipMode(GXClipMode mode) {
    GX_XF_LOAD_REG(GX_XF_REG_CLIPDISABLE, mode);
    __GXData->bpSentNot = GX_TRUE;
}

/* 803605F8-8036067C 35AF38 0084+00 1/1 1/1 0/0 .text            __GXSetMatrixIndex */
void __GXSetMatrixIndex(GXAttr index) {
    // Tex4 and after is stored in XF MatrixIndex1
    if (index < GX_VA_TEX4MTXIDX) {
        GX_CP_LOAD_REG(GX_CP_REG_MTXIDXA, __GXData->matIdxA);
        GX_XF_LOAD_REG(GX_XF_REG_MATRIXINDEX0, __GXData->matIdxA);
    } else {
        GX_CP_LOAD_REG(GX_CP_REG_MTXIDXB, __GXData->matIdxB);
        GX_XF_LOAD_REG(GX_XF_REG_MATRIXINDEX1, __GXData->matIdxB);
    }

    __GXData->bpSentNot = GX_TRUE;
}
