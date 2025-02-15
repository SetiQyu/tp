//
// Generated By: dol2asm
// Translation Unit: J3DStruct
//

#include "JSystem/J3DGraphBase/J3DStruct.h"
#include "JSystem/JMath/JMath.h"
#include "dol2asm.h"

//
// Declarations:
//

/* 803256C4-80325718 320004 0054+00 0/0 11/11 24/24 .text __as__12J3DLightInfoFRC12J3DLightInfo */
J3DLightInfo& J3DLightInfo::operator=(J3DLightInfo const& param_0) {
    JMath::gekko_ps_copy6(&mLightPosition, &param_0.mLightPosition);
    mColor = param_0.mColor;
    JMath::gekko_ps_copy6(&mCosAtten, &param_0.mCosAtten);
    return *this;
}

/* 80325718-80325794 320058 007C+00 0/0 4/4 0/0 .text __as__13J3DTexMtxInfoFRC13J3DTexMtxInfo */
J3DTexMtxInfo& J3DTexMtxInfo::operator=(J3DTexMtxInfo const& param_0) {
    mProjection = param_0.mProjection;
    mInfo = param_0.mInfo;
    JMath::gekko_ps_copy3(&mCenter, &param_0.mCenter);
    mSRT = param_0.mSRT;
    JMath::gekko_ps_copy16(&mEffectMtx, &param_0.mEffectMtx);
    return *this;
}

/* ############################################################################################## */
/* 80456410-80456414 004A10 0004+00 1/1 0/0 0/0 .sdata2          @409 */
SECTION_SDATA2 static f32 lit_409 = 1.0f;

/* 80456414-80456418 004A14 0004+00 1/1 0/0 0/0 .sdata2          @410 */
SECTION_SDATA2 static u8 lit_410[4] = {
    0x00,
    0x00,
    0x00,
    0x00,
};

/* 80325794-803257DC 3200D4 0048+00 0/0 2/2 7/7 .text            setEffectMtx__13J3DTexMtxInfoFPA4_f
 */
// needs inline asm?
#ifdef NONMATCHING
void J3DTexMtxInfo::setEffectMtx(Mtx param_0) {
    JMath::gekko_ps_copy12(&mEffectMtx, param_0);
    mEffectMtx[3][0] = 0.0f;
    mEffectMtx[3][1] = 0.0f;
    mEffectMtx[3][2] = 0.0f;
    mEffectMtx[3][3] = 1.0f;
}
#else
#pragma push
#pragma optimization_level 0
#pragma optimizewithasm off
asm void J3DTexMtxInfo::setEffectMtx(f32 (*param_0)[4]) {
    nofralloc
#include "asm/JSystem/J3DGraphBase/J3DStruct/setEffectMtx__13J3DTexMtxInfoFPA4_f.s"
}
#pragma pop
#endif

/* 803257DC-80325800 32011C 0024+00 0/0 5/5 0/0 .text
 * __as__16J3DIndTexMtxInfoFRC16J3DIndTexMtxInfo                */
J3DIndTexMtxInfo& J3DIndTexMtxInfo::operator=(J3DIndTexMtxInfo const& param_0) {
    JMath::gekko_ps_copy6(field_0x0, param_0.field_0x0);
    field_0x18 = param_0.field_0x18;
    return *this;
}

/* 80325800-8032587C 320140 007C+00 0/0 6/6 0/0 .text            __as__10J3DFogInfoFRC10J3DFogInfo
 */
J3DFogInfo& J3DFogInfo::operator=(J3DFogInfo const& param_0) {
    field_0x0 = param_0.field_0x0;
    field_0x1 = param_0.field_0x1;
    field_0x2 = param_0.field_0x2;
    field_0x4 = param_0.field_0x4;
    field_0x8 = param_0.field_0x8;
    field_0xc = param_0.field_0xc;
    field_0x10 = param_0.field_0x10;
    field_0x14 = param_0.field_0x14;
    for (int i = 0; i < 10; i++) {
        field_0x18.fogVals[i] = param_0.field_0x18.fogVals[i];
    }
    return *this;
}

/* 8032587C-803258A0 3201BC 0024+00 0/0 6/6 0/0 .text __as__15J3DNBTScaleInfoFRC15J3DNBTScaleInfo
 */
J3DNBTScaleInfo& J3DNBTScaleInfo::operator=(J3DNBTScaleInfo const& param_0) {
    mbHasScale = param_0.mbHasScale;
    mScale = param_0.mScale;
    return *this;
}