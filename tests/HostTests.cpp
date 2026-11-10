#include <cstring>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <set>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "Parameters.h"
#include "PluginMetadata.h"

extern "C" PF_Err EffectMain(PF_Cmd, PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*, void*);
static void require(bool pass, const char* label) {
    if (!pass) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}

struct Host {
    PF_InData in{};
    PF_OutData out{};
    PF_ParamDef definitions[ParamCount]{};
    PF_ParamDef* params[ParamCount]{};
    std::vector<PF_Pixel8> source, past, destination;
    std::vector<int16_t> audio;
    std::vector<A_long> times;
    PF_LayerDef output{};
    int added = 1, checked = 0, returned = 0, audioChecked = 0, audioReturned = 0, abortCalls = 0;
    bool failCheckout = false, cancel = false, hasAudio = false, failAudio = false;
    bool throwAbort = false;
    int audioHandles = 0;
    Host() : source(32*16), past(32*16), destination(32*16), audio(4410, 6000) {
        for (int i=0; i<ParamCount; ++i) params[i] = &definitions[i];
        in.effect_ref = reinterpret_cast<PF_ProgPtr>(this);
        in.current_time = 1000; in.time_scale = 1000; in.time_step = 40;
        in.downsample_x = {1,1}; in.downsample_y = {1,1}; in.pixel_aspect_ratio = {1,1};
        in.inter.add_param = [](PF_ProgPtr ref, PF_ParamIndex, PF_ParamDef* def) -> PF_Err {
            auto& h = *reinterpret_cast<Host*>(ref); h.definitions[h.added++] = *def; return 0;
        };
        in.inter.checkout_param = [](PF_ProgPtr ref, PF_ParamIndex index, A_long time, A_long, A_u_long, PF_ParamDef* def) -> PF_Err {
            auto& h = *reinterpret_cast<Host*>(ref); ++h.checked; h.times.push_back(time);
            require(index == Input, "checkout input layer history");
            *def = h.definitions[Input]; def->u.ld.data = time >= 0 ? h.past.data() : nullptr;
            return h.failCheckout ? PF_Err_BAD_CALLBACK_PARAM : 0;
        };
        in.inter.checkin_param = [](PF_ProgPtr ref, PF_ParamDef*) -> PF_Err {
            ++reinterpret_cast<Host*>(ref)->returned; return 0;
        };
        in.inter.checkout_layer_audio = [](PF_ProgPtr ref, PF_ParamIndex index, A_long start, A_long duration,
            A_u_long scale, PF_UFixed rate, A_long bytes, A_long channels, A_long format, PF_LayerAudio* result) -> PF_Err {
            auto& h = *reinterpret_cast<Host*>(ref); ++h.audioChecked;
            require(index == AudioLayer && start < h.in.current_time && duration > 0 && scale == 1000, "audio request time");
            require(rate == (22050UL << 16) && bytes == 2 && channels == 2 && format == PF_SIGNED_PCM, "audio request format");
            *result = h.hasAudio ? reinterpret_cast<PF_LayerAudio>(&h) : nullptr;
            if (*result) ++h.audioHandles;
            return h.failAudio ? PF_Err_BAD_CALLBACK_PARAM : 0;
        };
        in.inter.get_audio_data = [](PF_ProgPtr ref, PF_LayerAudio, PF_SndSamplePtr* data, A_long* count,
            PF_UFixed* rate, A_long* bytes, A_long* channels, A_long* format) -> PF_Err {
            auto& h = *reinterpret_cast<Host*>(ref); *data = h.audio.data(); *count = 2205;
            *rate = 22050UL << 16; *bytes = 2; *channels = 2; *format = PF_SIGNED_PCM; return 0;
        };
        in.inter.checkin_layer_audio = [](PF_ProgPtr ref, PF_LayerAudio handle) -> PF_Err {
            require(handle != nullptr, "never check in a null audio handle");
            ++reinterpret_cast<Host*>(ref)->audioReturned; return 0;
        };
        in.inter.abort = [](PF_ProgPtr ref) -> PF_Err {
            auto& h = *reinterpret_cast<Host*>(ref); ++h.abortCalls;
            if (h.throwAbort) throw static_cast<PF_Err>(PF_Interrupt_CANCEL);
            return h.cancel && h.abortCalls > 1 ? PF_Interrupt_CANCEL : 0;
        };
        in.inter.progress = [](PF_ProgPtr, A_long, A_long) -> PF_Err { return 0; };
        require(EffectMain(PF_Cmd_PARAMS_SETUP, &in, &out, params, nullptr, nullptr) == 0, "parameter setup");
        definitions[Input].u.ld.data = source.data();
        definitions[Input].u.ld.width = 32; definitions[Input].u.ld.height = 16;
        definitions[Input].u.ld.rowbytes = 32*sizeof(PF_Pixel8);
        output = definitions[Input].u.ld; output.data = destination.data();
        definitions[EchoCount].u.sd.value = 3;
        source[5*32+5] = {255,255,255,255}; past[5*32+3] = {255,255,255,255};
    }
    PF_Err render() { return EffectMain(PF_Cmd_RENDER, &in, &out, params, &output, nullptr); }
    void balanced() { require(checked == returned && audioHandles == audioReturned, "all acquired handles balanced"); }
};

int main() {
    Host h;
    require(h.added == ParamCount && h.out.num_params == ParamCount, "registered parameter count");
    std::set<A_long> ids;
    int groups = 0;
    for (int i=1; i<ParamCount; ++i) {
        require(h.definitions[i].uu.id == i && ids.insert(h.definitions[i].uu.id).second, "stable unique disk IDs");
        if (h.definitions[i].param_type == PF_Param_GROUP_START) ++groups;
        if (h.definitions[i].param_type == PF_Param_GROUP_END) --groups;
        require(groups >= 0, "balanced parameter groups");
    }
    require(groups == 0, "all groups closed");
    require(EffectMain(PF_Cmd_GLOBAL_SETUP, &h.in, &h.out, h.params, nullptr, nullptr) == 0, "global setup");
    require(h.out.out_flags == ECHO_OUT_FLAGS && h.out.out_flags2 == ECHO_OUT_FLAGS2, "PiPL consistency");
    require(h.render() == 0 && h.checked == 3, "render uses historical frames"); h.balanced();
    require(h.times[0] == 880 && h.times[1] == 920 && h.times[2] == 960, "echo delay in seconds");
    require(h.destination[5*32+5].alpha == 255, "current source retained");
    Host failed; failed.failCheckout = true;
    require(failed.render() == PF_Err_BAD_CALLBACK_PARAM, "checkout failure propagated"); failed.balanced();
    Host cancelled; cancelled.cancel = true;
    require(cancelled.render() == PF_Interrupt_CANCEL, "render can cancel"); cancelled.balanced();
    Host audio; audio.hasAudio = true;
    require(audio.render() == 0, "selected stereo music renders"); audio.balanced();
    Host audioFailed; audioFailed.failAudio = true;
    require(audioFailed.render() == PF_Err_BAD_CALLBACK_PARAM, "audio failure propagated"); audioFailed.balanced();
    Host thrown; thrown.throwAbort = true;
    require(thrown.render() == PF_Interrupt_CANCEL, "thrown host cancellation must not become fatal structure damage"); thrown.balanced();
    Host zero; zero.definitions[EchoCount].u.sd.value = 0;
    require(zero.render() == 0 && std::memcmp(zero.source.data(), zero.destination.data(), zero.source.size()*sizeof(PF_Pixel8)) == 0,
            "zero echoes is identity at full source opacity"); zero.balanced();
    Host noHistory; noHistory.definitions[EchoDelay].u.fs_d.value = 0;
    require(noHistory.render() == 0 && noHistory.checked == 0, "zero delay duplicates current buffer"); noHistory.balanced();
    Host start; start.in.current_time = 0; start.definitions[TrailsOnly].u.bd.value = TRUE;
    require(start.render() == 0, "start-of-layer render"); start.balanced();
    for (const auto& p : start.destination) require(p.alpha == 0, "out-of-range history is transparent");

    // 16 bpc with row padding: AE's channel maximum is 32768, not 65535.
    Host deep;
    std::vector<PF_Pixel16> input16(34*16), output16(34*16);
    input16[5*34+5] = {32768, 16384, 8192, 4096};
    deep.definitions[Input].u.ld.data = reinterpret_cast<PF_PixelPtr>(input16.data());
    deep.definitions[Input].u.ld.world_flags = PF_WorldFlag_DEEP;
    deep.definitions[Input].u.ld.rowbytes = 34 * sizeof(PF_Pixel16);
    deep.output = deep.definitions[Input].u.ld;
    deep.output.data = reinterpret_cast<PF_PixelPtr>(output16.data());
    deep.definitions[EchoCount].u.sd.value = 0;
    require(deep.render() == 0 && output16[5*34+5].red == 16384 && output16[5*34+5].alpha == 32768,
            "16 bpc and padded rows"); deep.balanced();
    std::cout << "Host checks passed: registration, history, audio, error cleanup, cancellation, 8/16 bpc.\n";
}
