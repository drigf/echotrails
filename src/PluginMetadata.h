#pragma once

#define ECHO_NAME "Echo Trails"
#define ECHO_MATCH_NAME "Signal Forge Echo Trails"
#define ECHO_CATEGORY "Stylize"
// WIDE_TIME_INPUT | NON_PARAM_VARY | I_USE_AUDIO | DEEP_COLOR_AWARE
#define ECHO_OUT_FLAGS 0x02100006
// SUPPORTS_THREADED_RENDERING; no mutable global/sequence state.
#define ECHO_OUT_FLAGS2 0x08000000
// PF_VERSION(1, 0, 1, PF_Stage_DEVELOP, 1)
#define ECHO_VERSION 526337
