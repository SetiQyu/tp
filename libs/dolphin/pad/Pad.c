#include "dolphin/pad.h"
#include "dolphin/os.h"
#include "dolphin/si/SIBios.h"
#include "dolphin/si/SISamplingRate.h"

u8 UnkVal : (OS_BASE_CACHED | 0x30e3);
u16 __OSWirelessPadFixMode : (OS_BASE_CACHED | 0x30E0);

/* 80450A20-80450A24 -00001 0004+00 1/1 0/0 0/0 .sdata           __PADVersion */
static char* __PADVersion = "<< Dolphin SDK - PAD\trelease build: Apr  5 2004 04:14:49 (0x2301) >>";

static void UpdateOrigin(s32 chan);
static void SPEC0_MakeStatus(s32 chan, PADStatus* status, u32 data[2]);
static void SPEC1_MakeStatus(s32 chan, PADStatus* status, u32 data[2]);
static void SPEC2_MakeStatus(s32 chan, PADStatus* status, u32 data[2]);
static BOOL OnReset(BOOL final);
static void SamplingHandler(__OSInterrupt interrupt, OSContext* context);
static PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback);
BOOL __PADDisableRecalibration(BOOL disable);
static void PADOriginCallback(s32 chan, u32 error, OSContext* context);
static void PADOriginUpdateCallback(s32 chan, u32 error, OSContext* context);
static void PADProbeCallback(s32 chan, u32 error, OSContext* context);
static void PADTypeAndStatusCallback(s32 chan, u32 type);
static void PADReceiveCheckCallback(s32 chan, u32 type);

extern u32 __PADFixBits;

/* 8044CB70-8044CB80 079890 0010+00 3/3 0/0 0/0 .bss             Type */
static u32 Type[4];

/* 8044CB80-8044CBB0 0798A0 0030+00 8/8 0/0 0/0 .bss             Origin */
static PADStatus Origin[4];

/* 80450A24-80450A28 0004A4 0004+00 7/7 0/0 0/0 .sdata           ResettingChan */
static s32 ResettingChan = 0x00000020;

/* 80450A28-80450A2C 0004A8 0004+00 1/1 0/0 0/0 .sdata           XPatchBits */
static u32 XPatchBits = PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT;

/* 80450A2C-80450A30 0004AC 0004+00 7/7 0/0 0/0 .sdata           AnalogMode */
static u32 AnalogMode = 0x00000300;

/* 8034E2B4-8034E458 348BF4 01A4+00 2/2 0/0 0/0 .text            UpdateOrigin */
static void UpdateOrigin(s32 chan) {
    PADStatus* origin;
    u32 chanBit = (u32)PAD_CHAN0_BIT >> chan;

    origin = &Origin[chan];
    switch (AnalogMode & 0x00000700u) {
    case 0x00000000u:
    case 0x00000500u:
    case 0x00000600u:
    case 0x00000700u:
        origin->trigger_left &= ~15;
        origin->trigger_right &= ~15;
        origin->analog_a &= ~15;
        origin->analog_b &= ~15;
        break;
    case 0x00000100u:
        origin->substick_x &= ~15;
        origin->substick_y &= ~15;
        origin->analog_a &= ~15;
        origin->analog_b &= ~15;
        break;
    case 0x00000200u:
        origin->substick_x &= ~15;
        origin->substick_y &= ~15;
        origin->trigger_left &= ~15;
        origin->trigger_right &= ~15;
        break;
    case 0x00000300u:
        break;
    case 0x00000400u:
        break;
    }

    origin->stick_x -= 128;
    origin->stick_y -= 128;
    origin->substick_x -= 128;
    origin->substick_y -= 128;

    if (XPatchBits & chanBit) {
        if (64 < origin->stick_x && (SIGetType(chan) & 0xFFFF0000) == SI_GC_CONTROLLER) {
            origin->stick_x = 0;
        }
    }
}

/* ############################################################################################## */
/* 80451848-8045184C 000D48 0004+00 1/1 0/0 0/0 .sbss            Initialized */
static BOOL Initialized;

/* 8045184C-80451850 000D4C 0004+00 10/10 0/0 0/0 .sbss            EnabledBits */
static u32 EnabledBits;

/* 80451850-80451854 000D50 0004+00 7/7 0/0 0/0 .sbss            ResettingBits */
static u32 ResettingBits;

inline void PADEnable(s32 chan) {
    u32 cmd;
    u32 chanBit;
    u32 data[2];

    chanBit = (u32)PAD_CHAN0_BIT >> chan;
    EnabledBits |= chanBit;
    SIGetResponse(chan, data);
    cmd = (0x40 << 16) | AnalogMode;
    SISetCommand(chan, cmd);
    SIEnablePolling(EnabledBits);
}

inline void DoReset(void) {
    u32 chanBit;

    ResettingChan = __cntlzw(ResettingBits);
    if (ResettingChan == 32) {
        return;
    }

    chanBit = (u32)PAD_CHAN0_BIT >> ResettingChan;
    ResettingBits &= ~chanBit;

    memset(&Origin[ResettingChan], 0, sizeof(PADStatus));
    SIGetTypeAsync(ResettingChan, PADTypeAndStatusCallback);
}

/* 8034E458-8034E51C 348D98 00C4+00 1/1 0/0 0/0 .text            PADOriginCallback */
static void PADOriginCallback(s32 chan, u32 error, OSContext* context) {
    if (!(error &
          (SI_ERROR_UNDER_RUN | SI_ERROR_OVER_RUN | SI_ERROR_NO_RESPONSE | SI_ERROR_COLLISION)))
    {
        UpdateOrigin(ResettingChan);
        PADEnable(ResettingChan);
    }
    DoReset();
}

/* ############################################################################################## */
/* 80451854-80451858 000D54 0004+00 4/4 0/0 0/0 .sbss            RecalibrateBits */
static u32 RecalibrateBits;

/* 80451858-8045185C 000D58 0004+00 7/7 0/0 0/0 .sbss            WaitingBits */
static u32 WaitingBits;

/* 8045185C-80451860 000D5C 0004+00 6/6 0/0 0/0 .sbss            CheckingBits */
static u32 CheckingBits;

/* 80451860-80451864 000D60 0004+00 6/6 0/0 0/0 .sbss            PendingBits */
static u32 PendingBits;

/* 80451864-80451868 000D64 0004+00 6/6 0/0 0/0 .sbss            BarrelBits */
static u32 BarrelBits;

inline void PADDisable(s32 chan) {
    BOOL enabled;
    u32 chanBit;

    enabled = OSDisableInterrupts();

    chanBit = (u32)PAD_CHAN0_BIT >> chan;
    SIDisablePolling(chanBit);
    EnabledBits &= ~chanBit;
    WaitingBits &= ~chanBit;
    CheckingBits &= ~chanBit;
    PendingBits &= ~chanBit;
    BarrelBits &= ~chanBit;
    OSSetWirelessID(chan, 0);

    OSRestoreInterrupts(enabled);
}

/* 8034E51C-8034E5E8 348E5C 00CC+00 2/2 0/0 0/0 .text            PADOriginUpdateCallback */
static void PADOriginUpdateCallback(s32 chan, u32 error, OSContext* context) {
    if (!(EnabledBits & ((u32)PAD_CHAN0_BIT >> chan))) {
        return;
    }

    if (!(error &
          (SI_ERROR_UNDER_RUN | SI_ERROR_OVER_RUN | SI_ERROR_NO_RESPONSE | SI_ERROR_COLLISION)))
    {
        UpdateOrigin(chan);
    }

    if (error & SI_ERROR_NO_RESPONSE) {
        PADDisable(chan);
    }
}

/* 8034E5E8-8034E6C0 348F28 00D8+00 1/1 0/0 0/0 .text            PADProbeCallback */
static void PADProbeCallback(s32 chan, u32 error, OSContext* context) {
    if (!(error &
          (SI_ERROR_UNDER_RUN | SI_ERROR_OVER_RUN | SI_ERROR_NO_RESPONSE | SI_ERROR_COLLISION)))
    {
        PADEnable(ResettingChan);
        WaitingBits |= (u32)PAD_CHAN0_BIT >> ResettingChan;
    }
    DoReset();
}

/* ############################################################################################## */
/* 80450A30-80450A34 0004B0 0004+00 4/4 0/0 0/0 .sdata           Spec */
static u32 Spec = 5;

/* 80450A34-80450A38 -00001 0004+00 2/2 0/0 0/0 .sdata           MakeStatus */
static void (*MakeStatus)(s32, PADStatus*, u32[2]) = SPEC2_MakeStatus;

/* 80450A38-80450A3C 0004B8 0004+00 3/3 0/0 0/0 .sdata           CmdReadOrigin */
static f32 CmdReadOrigin = 8.0f;

/* 80450A3C-80450A40 0004BC 0004+00 1/1 0/0 0/0 .sdata           CmdCalibrate */
static f32 CmdCalibrate = 32.0f;

/* 8044CBB0-8044CBC0 0798D0 0010+00 0/1 0/0 0/0 .bss             CmdProbeDevice */
#pragma push
#pragma force_active on
static u32 CmdProbeDevice[4];
#pragma pop

/* 8034E6C0-8034E9EC 349000 032C+00 4/4 0/0 0/0 .text            PADTypeAndStatusCallback */
static void PADTypeAndStatusCallback(s32 chan, u32 type) {
    u32 chanBit;
    u32 recalibrate;
    BOOL rc = TRUE;
    u32 error;
    chanBit = (u32)PAD_CHAN0_BIT >> ResettingChan;
    error = type & 0xFF;
    recalibrate = RecalibrateBits & chanBit;
    RecalibrateBits &= ~chanBit;

    if (error &
        (SI_ERROR_UNDER_RUN | SI_ERROR_OVER_RUN | SI_ERROR_NO_RESPONSE | SI_ERROR_COLLISION))
    {
        DoReset();
        return;
    }

    type &= ~0xFF;

    Type[ResettingChan] = type;

    if ((type & SI_TYPE_MASK) != SI_TYPE_GC || !(type & SI_GC_STANDARD)) {
        DoReset();
        return;
    }

    if (Spec < PAD_SPEC_2) {
        PADEnable(ResettingChan);
        DoReset();
        return;
    }

    if (!(type & SI_GC_WIRELESS) || (type & SI_WIRELESS_IR)) {
        if (recalibrate) {
            rc = SITransfer(ResettingChan, &CmdCalibrate, 3, &Origin[ResettingChan], 10,
                            PADOriginCallback, 0);
        } else {
            rc = SITransfer(ResettingChan, &CmdReadOrigin, 1, &Origin[ResettingChan], 10,
                            PADOriginCallback, 0);
        }
    } else if ((type & SI_WIRELESS_FIX_ID) && (type & SI_WIRELESS_CONT_MASK) == SI_WIRELESS_CONT &&
               !(type & SI_WIRELESS_LITE))
    {
        if (type & SI_WIRELESS_RECEIVED) {
            rc = SITransfer(ResettingChan, &CmdReadOrigin, 1, &Origin[ResettingChan], 10,
                            PADOriginCallback, 0);
        } else {
            rc = SITransfer(ResettingChan, &CmdProbeDevice[ResettingChan], 3,
                            &Origin[ResettingChan], 8, PADProbeCallback, 0);
        }
    }
    if (!rc) {
        PendingBits |= chanBit;
        DoReset();
        return;
    }
}

/* 8034E9EC-8034EB2C 34932C 0140+00 1/1 0/0 0/0 .text            PADReceiveCheckCallback */
static void PADReceiveCheckCallback(s32 chan, u32 type) {
    u32 error;
    u32 chanBit;

    chanBit = (u32)PAD_CHAN0_BIT >> chan;
    if (!(EnabledBits & chanBit)) {
        return;
    }

    error = type & 0xFF;
    type &= ~0xFF;

    WaitingBits &= ~chanBit;
    CheckingBits &= ~chanBit;

    if (!(error &
          (SI_ERROR_UNDER_RUN | SI_ERROR_OVER_RUN | SI_ERROR_NO_RESPONSE | SI_ERROR_COLLISION)) &&
        (type & SI_GC_WIRELESS) && (type & SI_WIRELESS_FIX_ID) && (type & SI_WIRELESS_RECEIVED) &&
        !(type & SI_WIRELESS_IR) && (type & SI_WIRELESS_CONT_MASK) == SI_WIRELESS_CONT &&
        !(type & SI_WIRELESS_LITE))
    {
        SITransfer(chan, &CmdReadOrigin, 1, &Origin[chan], 10, PADOriginUpdateCallback, 0);
    } else {
        PADDisable(chan);
    }
}

/* 8034EB2C-8034EC3C 34946C 0110+00 2/2 1/1 0/0 .text            PADReset */
BOOL PADReset(u32 mask) {
    BOOL enabled;
    u32 diableBits;

    enabled = OSDisableInterrupts();

    mask |= PendingBits;
    PendingBits = 0;
    mask &= ~(WaitingBits | CheckingBits);
    ResettingBits |= mask;
    diableBits = ResettingBits & EnabledBits;
    EnabledBits &= ~mask;
    BarrelBits &= ~mask;

    if (Spec == PAD_SPEC_4) {
        RecalibrateBits |= mask;
    }

    SIDisablePolling(diableBits);

    if (ResettingChan == 32) {
        DoReset();
    }
    OSRestoreInterrupts(enabled);
    return TRUE;
}

/* 8034EC3C-8034ED50 34957C 0114+00 1/1 1/1 0/0 .text            PADRecalibrate */
BOOL PADRecalibrate(u32 mask) {
    BOOL enabled;
    u32 disableBits;

    enabled = OSDisableInterrupts();

    mask |= PendingBits;
    PendingBits = 0;
    mask &= ~(WaitingBits | CheckingBits);
    ResettingBits |= mask;
    disableBits = ResettingBits & EnabledBits;
    EnabledBits &= ~mask;
    BarrelBits &= ~mask;

    if (!(UnkVal & 0x40)) {
        RecalibrateBits |= mask;
    }

    SIDisablePolling(disableBits);
    if (ResettingChan == 32) {
        DoReset();
    }
    OSRestoreInterrupts(enabled);
    return TRUE;
}

/* ############################################################################################## */
/* 803D1B90-803D1BA0 -00001 0010+00 1/1 0/0 0/0 .data            ResetFunctionInfo */
static OSResetFunctionInfo ResetFunctionInfo = {
    OnReset,
    0x0000007F,
};

/* 80451868-8045186C 000D68 0004+00 3/3 0/0 0/0 .sbss            SamplingCallback */
static void (*SamplingCallback)(void);

/* 8045186C-80451870 000D6C 0004+00 1/1 0/0 0/0 .sbss            recalibrated$388 */
static BOOL recalibrated;

/* 80451870-80451878 000D70 0004+04 2/2 1/1 0/0 .sbss            __PADSpec */
extern u32 __PADSpec;
u32 __PADSpec;

/* 8034ED50-8034EEA0 349690 0150+00 0/0 1/1 0/0 .text            PADInit */
BOOL PADInit() {
    s32 chan;
    if (Initialized) {
        return TRUE;
    }

    OSRegisterVersion(__PADVersion);

    if (__PADSpec) {
        PADSetSpec(__PADSpec);
    }

    Initialized = TRUE;

    if (__PADFixBits != 0) {
        OSTime time = OSGetTime();
        __OSWirelessPadFixMode = (u16)((((time) & 0xffff) + ((time >> 16) & 0xffff) +
                                        ((time >> 32) & 0xffff) + ((time >> 48) & 0xffff)) &
                                       0x3fffu);
        RecalibrateBits = PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT;
    }

    for (chan = 0; chan < SI_MAX_CHAN; ++chan) {
        CmdProbeDevice[chan] =
            (0x4D << 24) | (chan << 22) | ((__OSWirelessPadFixMode & 0x3fffu) << 8);
    }

    SIRefreshSamplingRate();
    OSRegisterResetFunction(&ResetFunctionInfo);

    return PADReset(PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT);
}

/* 8034EEA0-8034F1A0 3497E0 0300+00 0/0 1/1 0/0 .text            PADRead */
u32 PADRead(PADStatus* status) {
    BOOL enabled;
    s32 chan;
    u32 data[2];
    u32 chanBit;
    u32 sr;
    int chanShift;
    u32 motor;

    enabled = OSDisableInterrupts();

    motor = 0;
    for (chan = 0; chan < SI_MAX_CHAN; chan++, status++) {
        chanBit = (u32)PAD_CHAN0_BIT >> chan;
        chanShift = 8 * (SI_MAX_CHAN - 1 - chan);

        if (PendingBits & chanBit) {
            PADReset(0);
            status->error = PAD_ERR_NOT_READY;
            memset(status, 0, offsetof(PADStatus, error));
            continue;
        }

        if ((ResettingBits & chanBit) || ResettingChan == chan) {
            status->error = PAD_ERR_NOT_READY;
            memset(status, 0, offsetof(PADStatus, error));
            continue;
        }

        if (!(EnabledBits & chanBit)) {
            status->error = (s8)PAD_ERR_NO_CONTROLLER;
            memset(status, 0, offsetof(PADStatus, error));
            continue;
        }

        if (SIIsChanBusy(chan)) {
            status->error = PAD_ERR_TRANSFER;
            memset(status, 0, offsetof(PADStatus, error));
            continue;
        }

        sr = SIGetStatus(chan);
        if (sr & SI_ERROR_NO_RESPONSE) {
            SIGetResponse(chan, data);

            if (WaitingBits & chanBit) {
                status->error = (s8)PAD_ERR_NONE;
                memset(status, 0, offsetof(PADStatus, error));

                if (!(CheckingBits & chanBit)) {
                    CheckingBits |= chanBit;
                    SIGetTypeAsync(chan, PADReceiveCheckCallback);
                }
                continue;
            }

            PADDisable(chan);

            status->error = (s8)PAD_ERR_NO_CONTROLLER;
            memset(status, 0, offsetof(PADStatus, error));
            continue;
        }

        if (!(SIGetType(chan) & SI_GC_NOMOTOR)) {
            motor |= chanBit;
        }

        if (!SIGetResponse(chan, data)) {
            status->error = PAD_ERR_TRANSFER;
            memset(status, 0, offsetof(PADStatus, error));
            continue;
        }

        if (data[0] & 0x80000000) {
            status->error = PAD_ERR_TRANSFER;
            memset(status, 0, offsetof(PADStatus, error));
            continue;
        }

        MakeStatus(chan, status, data);

        // Check and clear PAD_ORIGIN bit
        if (status->button & 0x2000) {
            status->error = PAD_ERR_TRANSFER;
            memset(status, 0, offsetof(PADStatus, error));

            // Get origin. It is okay if the following transfer fails
            // since the PAD_ORIGIN bit remains until the read origin
            // command complete.
            SITransfer(chan, &CmdReadOrigin, 1, &Origin[chan], 10, PADOriginUpdateCallback, 0);
            continue;
        }

        status->error = PAD_ERR_NONE;

        // Clear PAD_INTERFERE bit
        status->button &= ~0x0080;
    }

    OSRestoreInterrupts(enabled);
    return motor;
}

/* 8034F1A0-8034F258 349AE0 00B8+00 0/0 2/2 0/0 .text            PADControlMotor */
void PADControlMotor(s32 chan, u32 command) {
    BOOL enabled;
    u32 chanBit;

    enabled = OSDisableInterrupts();
    chanBit = (u32)PAD_CHAN0_BIT >> chan;
    if ((EnabledBits & chanBit) && !(SIGetType(chan) & SI_GC_NOMOTOR)) {
        if (Spec < PAD_SPEC_2 && command == PAD_MOTOR_STOP_HARD) {
            command = PAD_MOTOR_STOP;
        }

        if (UnkVal & 0x20) {
            command = PAD_MOTOR_STOP;
        }

        SISetCommand(chan, (0x40 << 16) | AnalogMode | (command & (0x00000001 | 0x00000002)));
        SITransferCommands();
    }
    OSRestoreInterrupts(enabled);
}

/* 8034F258-8034F2B8 349B98 0060+00 1/1 1/1 0/0 .text            PADSetSpec */
void PADSetSpec(u32 spec) {
    __PADSpec = 0;
    switch (spec) {
    case PAD_SPEC_0:
        MakeStatus = SPEC0_MakeStatus;
        break;
    case PAD_SPEC_1:
        MakeStatus = SPEC1_MakeStatus;
        break;
    case PAD_SPEC_2:
    case PAD_SPEC_3:
    case PAD_SPEC_4:
    case PAD_SPEC_5:
        MakeStatus = SPEC2_MakeStatus;
        break;
    }
    Spec = spec;
}

/* 8034F2B8-8034F42C 349BF8 0174+00 1/1 0/0 0/0 .text            SPEC0_MakeStatus */
static void SPEC0_MakeStatus(s32 chan, PADStatus* status, u32 data[2]) {
    status->button = 0;
    status->button |= ((data[0] >> 16) & 0x0008) ? PAD_BUTTON_A : 0;
    status->button |= ((data[0] >> 16) & 0x0020) ? PAD_BUTTON_B : 0;
    status->button |= ((data[0] >> 16) & 0x0100) ? PAD_BUTTON_X : 0;
    status->button |= ((data[0] >> 16) & 0x0001) ? PAD_BUTTON_Y : 0;
    status->button |= ((data[0] >> 16) & 0x0010) ? PAD_BUTTON_START : 0;

    status->stick_x = (s8)(data[1] >> 16);
    status->stick_y = (s8)(data[1] >> 24);

    status->substick_x = (s8)(data[1]);
    status->substick_y = (s8)(data[1] >> 8);

    status->trigger_left = (u8)(data[0] >> 8);
    status->trigger_right = (u8)data[0];

    status->analog_a = 0;
    status->analog_b = 0;

    if (170 <= status->trigger_left) {
        status->button |= PAD_TRIGGER_L;
    }

    if (170 <= status->trigger_right) {
        status->button |= PAD_TRIGGER_R;
    }

    status->stick_x -= 128;
    status->stick_y -= 128;

    status->substick_x -= 128;
    status->substick_y -= 128;
}

/* 8034F42C-8034F5A0 349D6C 0174+00 1/1 0/0 0/0 .text            SPEC1_MakeStatus */
static void SPEC1_MakeStatus(s32 chan, PADStatus* status, u32 data[2]) {
    status->button = 0;
    status->button |= ((data[0] >> 16) & 0x0080) ? PAD_BUTTON_A : 0;
    status->button |= ((data[0] >> 16) & 0x0100) ? PAD_BUTTON_B : 0;
    status->button |= ((data[0] >> 16) & 0x0020) ? PAD_BUTTON_X : 0;
    status->button |= ((data[0] >> 16) & 0x0010) ? PAD_BUTTON_Y : 0;
    status->button |= ((data[0] >> 16) & 0x0200) ? PAD_BUTTON_START : 0;

    status->stick_x = (s8)(data[1] >> 16);
    status->stick_y = (s8)(data[1] >> 24);
    status->substick_x = (s8)(data[1]);
    status->substick_y = (s8)(data[1] >> 8);

    status->trigger_left = (u8)(data[0] >> 8);
    status->trigger_right = (u8)data[0];

    status->analog_a = 0;
    status->analog_b = 0;

    if (170 <= status->trigger_left) {
        status->button |= PAD_TRIGGER_L;
    }

    if (170 <= status->trigger_right) {
        status->button |= PAD_TRIGGER_R;
    }

    status->stick_x -= 128;
    status->stick_y -= 128;
    status->substick_x -= 128;
    status->substick_y -= 128;
}

inline s8 ClampS8(s8 var, s8 org) {
    if (0 < org) {
        s8 min = (s8)(-128 + org);
        if (var < min) {
            var = min;
        }
    } else if (org < 0) {
        s8 max = (s8)(127 + org);
        if (max < var) {
            var = max;
        }
    }
    return var -= org;
}

inline u8 ClampU8(u8 var, u8 org) {
    if (var < org) {
        var = org;
    }
    return var -= org;
}

#define PAD_ALL                                                                                    \
    (PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_UP | PAD_TRIGGER_Z |        \
     PAD_TRIGGER_R | PAD_TRIGGER_L | PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_Y |   \
     PAD_BUTTON_MENU | 0x2000 | 0x0080)

/* 8034F5A0-8034FA10 349EE0 0470+00 2/1 0/0 0/0 .text            SPEC2_MakeStatus */
static void SPEC2_MakeStatus(s32 chan, PADStatus* status, u32 data[2]) {
    PADStatus* origin;
    u32 type;

    status->button = (u16)((data[0] >> 16) & PAD_ALL);
    status->stick_x = (s8)(data[0] >> 8);
    status->stick_y = (s8)(data[0]);

    switch (AnalogMode & 0x00000700) {
    case 0x00000000:
    case 0x00000500:
    case 0x00000600:
    case 0x00000700:
        status->substick_x = (s8)(data[1] >> 24);
        status->substick_y = (s8)(data[1] >> 16);
        status->trigger_left = (u8)(((data[1] >> 12) & 0x0f) << 4);
        status->trigger_right = (u8)(((data[1] >> 8) & 0x0f) << 4);
        status->analog_a = (u8)(((data[1] >> 4) & 0x0f) << 4);
        status->analog_b = (u8)(((data[1] >> 0) & 0x0f) << 4);
        break;
    case 0x00000100:
        status->substick_x = (s8)(((data[1] >> 28) & 0x0f) << 4);
        status->substick_y = (s8)(((data[1] >> 24) & 0x0f) << 4);
        status->trigger_left = (u8)(data[1] >> 16);
        status->trigger_right = (u8)(data[1] >> 8);
        status->analog_a = (u8)(((data[1] >> 4) & 0x0f) << 4);
        status->analog_b = (u8)(((data[1] >> 0) & 0x0f) << 4);
        break;
    case 0x00000200:
        status->substick_x = (s8)(((data[1] >> 28) & 0x0f) << 4);
        status->substick_y = (s8)(((data[1] >> 24) & 0x0f) << 4);
        status->trigger_left = (u8)(((data[1] >> 20) & 0x0f) << 4);
        status->trigger_right = (u8)(((data[1] >> 16) & 0x0f) << 4);
        status->analog_a = (u8)(data[1] >> 8);
        status->analog_b = (u8)(data[1] >> 0);
        break;
    case 0x00000300:
        status->substick_x = (s8)(data[1] >> 24);
        status->substick_y = (s8)(data[1] >> 16);
        status->trigger_left = (u8)(data[1] >> 8);
        status->trigger_right = (u8)(data[1] >> 0);
        status->analog_a = 0;
        status->analog_b = 0;
        break;
    case 0x00000400:
        status->substick_x = (s8)(data[1] >> 24);
        status->substick_y = (s8)(data[1] >> 16);
        status->trigger_left = 0;
        status->trigger_right = 0;
        status->analog_a = (u8)(data[1] >> 8);
        status->analog_b = (u8)(data[1] >> 0);
        break;
    }

    status->stick_x -= 128;
    status->stick_y -= 128;
    status->substick_x -= 128;
    status->substick_y -= 128;

    type = Type[chan];

    if (((type & (0xffff0000)) == SI_GC_CONTROLLER) && ((status->button & 0x80) ^ 0x80)) {
        BarrelBits |= (PAD_CHAN0_BIT >> chan);
        status->stick_x = 0;
        status->stick_y = 0;
        status->substick_x = 0;
        status->substick_y = 0;
        return;
    } else {
        BarrelBits &= ~(PAD_CHAN0_BIT >> chan);
    }

    origin = &Origin[chan];
    status->stick_x = ClampS8(status->stick_x, origin->stick_x);
    status->stick_y = ClampS8(status->stick_y, origin->stick_y);
    status->substick_x = ClampS8(status->substick_x, origin->substick_x);
    status->substick_y = ClampS8(status->substick_y, origin->substick_y);
    status->trigger_left = ClampU8(status->trigger_left, origin->trigger_left);
    status->trigger_right = ClampU8(status->trigger_right, origin->trigger_right);
}

/* 8034FA10-8034FA84 34A350 0074+00 0/0 2/2 0/0 .text            PADSetAnalogMode */
void PADSetAnalogMode(u32 mode) {
    BOOL enabled;
    u32 mask;

    enabled = OSDisableInterrupts();
    AnalogMode = mode << 8;
    mask = EnabledBits;

    EnabledBits &= ~mask;
    WaitingBits &= ~mask;
    CheckingBits &= ~mask;

    SIDisablePolling(mask);
    OSRestoreInterrupts(enabled);
}

inline BOOL PADSync(void) {
    return ResettingBits == 0 && ResettingChan == 32 && !SIBusy();
}

/* 8034FA84-8034FB40 34A3C4 00BC+00 1/0 0/0 0/0 .text            OnReset */
static BOOL OnReset(BOOL f) {
    static BOOL recalibrated = FALSE;
    BOOL sync;

    if (SamplingCallback) {
        PADSetSamplingCallback(NULL);
    }

    if (!f) {
        sync = PADSync();
        if (!recalibrated && sync) {
            recalibrated =
                PADRecalibrate(PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT);
            return FALSE;
        }
        return sync;
    } else {
        recalibrated = FALSE;
    }

    return TRUE;
}

/* 8034FB40-8034FBA0 34A480 0060+00 1/1 0/0 0/0 .text            SamplingHandler */
static void SamplingHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;

    if (SamplingCallback) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        SamplingCallback();
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
}

/* 8034FBA0-8034FBF4 34A4E0 0054+00 1/1 0/0 0/0 .text            PADSetSamplingCallback */
PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback) {
    PADSamplingCallback prev;

    prev = SamplingCallback;
    SamplingCallback = callback;
    if (callback) {
        SIRegisterPollingHandler(SamplingHandler);
    } else {
        SIUnregisterPollingHandler(SamplingHandler);
    }
    return prev;
}

/* 8034FBF4-8034FC70 34A534 007C+00 0/0 1/1 0/0 .text            __PADDisableRecalibration */
BOOL __PADDisableRecalibration(BOOL disable) {
    BOOL enabled;
    BOOL prev;

    enabled = OSDisableInterrupts();
    prev = (UnkVal & 0x40) ? TRUE : FALSE;
    UnkVal &= ~0x40;
    if (disable) {
        UnkVal |= 0x40;
    }
    OSRestoreInterrupts(enabled);
    return prev;
}
