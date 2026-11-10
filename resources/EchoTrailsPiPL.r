#include "AEConfig.h"
#include "AE_EffectVers.h"
#include "AE_General.r"
#include "PluginMetadata.h"

resource 'PiPL' (16000) {
    {
        Kind { AEEffect },
        Name { ECHO_NAME },
        Category { ECHO_CATEGORY },
        CodeMacIntel64 { "EffectMain" },
        CodeMacARM64 { "EffectMain" },
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { ECHO_VERSION },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags { ECHO_OUT_FLAGS },
        AE_Effect_Global_OutFlags_2 { ECHO_OUT_FLAGS2 },
        AE_Effect_Match_Name { ECHO_MATCH_NAME },
        AE_Reserved_Info { 0 }
    }
};
