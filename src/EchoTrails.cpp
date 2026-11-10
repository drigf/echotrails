#include <cstring>
#include <cstdio>
#include <limits>
#include <new>
#include <exception>
#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "EchoDSP.h"
#include "Parameters.h"
#include "PluginMetadata.h"

static_assert(ECHO_OUT_FLAGS == (PF_OutFlag_WIDE_TIME_INPUT | PF_OutFlag_NON_PARAM_VARY |
    PF_OutFlag_I_USE_AUDIO | PF_OutFlag_DEEP_COLOR_AWARE), "PiPL flags must match code");
static_assert(ECHO_OUT_FLAGS2 == PF_OutFlag2_SUPPORTS_THREADED_RENDERING, "PiPL flags must match code");
static_assert(ECHO_VERSION == PF_VERSION(1, 0, 1, PF_Stage_DEVELOP, 1), "PiPL version must match code");

namespace {
PF_Err paramsSetup(PF_InData* in_data, PF_OutData* out_data) {
    PF_ParamDef def{};
#define GROUP(NAME, ID) do { def = {}; PF_ADD_TOPIC(NAME, ID); } while (0)
#define END(ID) do { def = {}; PF_END_TOPIC(ID); } while (0)
#define SLIDER(NAME, MIN, MAX, DEFAULT, ID) PF_ADD_FLOAT_SLIDERX(NAME, MIN, MAX, MIN, MAX, DEFAULT, 2, 0, 0, ID)
#define INTEGER(NAME, MIN, MAX, DEFAULT, ID) do { def = {}; PF_ADD_SLIDER(NAME, MIN, MAX, MIN, MAX, DEFAULT, ID); } while (0)
#define COLOUR(NAME, R, G, B, ID) do { def = {}; PF_ADD_COLOR(NAME, R, G, B, ID); } while (0)
    GROUP("Audio Reaction", AudioGroup);
    def = {}; PF_ADD_LAYER("Music Layer", PF_LayerDefault_NONE, AudioLayer);
    PF_ADD_POPUPX("Frequency Band", 4, 1, "Full Spectrum|Bass (below 250 Hz)|Mid (250-2500 Hz)|High (above 2500 Hz)", 0, AudioBand);
    SLIDER("Audio Gain (dB)", -24, 48, 12, AudioGain);
    SLIDER("Noise Gate (%)", 0, 95, 2, AudioThreshold);
    SLIDER("Response Window (ms)", 10, 1000, 120, AudioWindow);
    SLIDER("Manual Drive (%)", 0, 100, 0, ManualDrive);
    SLIDER("Audio to Opacity (%)", 0, 200, 35, AudioOpacity);
    SLIDER("Audio to Motion (%)", 0, 200, 40, AudioMotion);
    SLIDER("Audio to Damage (%)", 0, 200, 70, AudioDamage);
    SLIDER("Audio to Colour (%)", 0, 100, 20, AudioColour);
    END(AudioEnd);

    GROUP("Echo Trails", EchoGroup);
    INTEGER("Echo Count", 0, 48, 12, EchoCount);
    SLIDER("Echo Delay (seconds)", 0, 2, .04, EchoDelay);
    SLIDER("Trail Opacity (%)", 0, 100, 55, EchoOpacity);
    SLIDER("Echo Retention (%)", 0, 100, 86, EchoDecay);
    SLIDER("Offset X per Echo (px)", -500, 500, 12, EchoOffsetX);
    SLIDER("Offset Y per Echo (px)", -500, 500, 0, EchoOffsetY);
    SLIDER("Scale per Echo (%)", -10, 10, 0, EchoZoom);
    SLIDER("Rotation per Echo (deg)", -45, 45, 0, EchoRotation);
    PF_ADD_POPUPX("Trail Blend", 3, 2, "Normal (source on top)|Screen|Add", 0, EchoBlend);
    SLIDER("Source Opacity (%)", 0, 100, 100, SourceOpacity);
    PF_ADD_CHECKBOXX("Trails Only", FALSE, 0, TrailsOnly);
    END(EchoEnd);

    GROUP("Trail Colours", ColourGroup);
    COLOUR("Near Colour", 26, 230, 255, NearColour);
    COLOUR("Middle Colour", 204, 38, 255, MidColour);
    COLOUR("Far Colour", 255, 38, 26, FarColour);
    SLIDER("Colour Mix (%)", 0, 100, 75, TintMix);
    SLIDER("Hue Cycles per Second", -2, 2, 0, HueSpeed);
    END(ColourEnd);

    GROUP("Decay / Circuit Bending", DamageGroup);
    SLIDER("Damage Falloff Curve", .1, 5, 1.5, DamageCurve);
    SLIDER("Far Pixel Size (px)", 1, 128, 24, PixelSize);
    INTEGER("Far Colour Bit Depth", 1, 16, 5, BitDepth);
    SLIDER("Circuit Bend (px)", 0, 500, 35, CircuitBend);
    SLIDER("Circuit Band Height (px)", 1, 128, 12, CircuitBandHeight);
    SLIDER("Signal Dropout (%)", 0, 100, 10, Dropout);
    SLIDER("RGB Separation (px)", 0, 150, 12, RGBSplit);
    SLIDER("Wave Modulation (px)", 0, 300, 12, WaveAmount);
    SLIDER("Wave Length (px)", 8, 2000, 160, WaveLength);
    SLIDER("Signal Speed (Hz)", 0, 30, 2, SignalRate);
    INTEGER("Random Seed", 0, 1000000, 1, RandomSeed);
    END(DamageEnd);
    out_data->num_params = ParamCount;
    return PF_Err_NONE;
}

float value(PF_ParamDef* p[], Param id) { return static_cast<float>(p[id]->u.fs_d.value); }
float percent(PF_ParamDef* p[], Param id) { return value(p, id) * .01f; }
echo::Colour colour(PF_ParamDef* p[], Param id) {
    const auto c = p[id]->u.cd.value;
    return {c.red / 255.0f, c.green / 255.0f, c.blue / 255.0f};
}

// Callbacks must be balanced even on errors. Stack ownership also handles C++ exceptions.
struct AudioCheckout {
    PF_InData* in;
    PF_LayerAudio audio = nullptr;
    bool attempted = false;
    ~AudioCheckout() noexcept {
        // A None/silent layer may return success without allocating an audio handle.
        // Never pass that null handle back into AE, or let cleanup throw during unwinding.
        if (attempted && audio) {
            try { PF_CHECKIN_LAYER_AUDIO(in, audio); } catch (...) {}
        }
    }
};
struct FrameCheckout {
    PF_InData* in;
    PF_ParamDef param{};
    bool attempted = false;
    ~FrameCheckout() noexcept {
        if (attempted) {
            try { PF_CHECKIN_PARAM(in, &param); } catch (...) {}
        }
    }
};

PF_Err readAudio(PF_InData* in, PF_ParamDef* p[], float& level) {
    level = percent(p, ManualDrive);
    const A_long duration = std::max<A_long>(1, static_cast<A_long>(std::llround(value(p, AudioWindow) * .001 * in->time_scale)));
    AudioCheckout checkout{in};
    checkout.attempted = true;
    // Stereo PCM avoids cancellation when the two channels have opposite polarity.
    PF_Err err = PF_CHECKOUT_LAYER_AUDIO(in, AudioLayer, in->current_time - duration, duration,
        in->time_scale, static_cast<PF_UFixed>(22050UL << 16), 2, 2, PF_SIGNED_PCM, &checkout.audio);
    if (err) return err;
    if (!checkout.audio) return PF_Err_NONE; // None / a layer without audio.
    PF_SndSamplePtr data = nullptr;
    A_long frames = 0, bytes = 0, channels = 0, format = 0;
    PF_UFixed rate = 0;
    err = PF_GET_AUDIO_DATA(in, checkout.audio, &data, &frames, &rate, &bytes, &channels, &format);
    if (err) return err;
    if (data && frames > 0 && bytes == 2 && format == PF_SIGNED_PCM) {
        level = std::max(level, echo::audioEnvelope(static_cast<const int16_t*>(data), frames, channels,
            rate / 65536.0, static_cast<echo::Band>(p[AudioBand]->u.pd.value), value(p, AudioGain), percent(p, AudioThreshold)));
    }
    return PF_Err_NONE;
}

echo::Settings settings(PF_InData* in, PF_ParamDef* p[], float audio) {
    echo::Settings s;
    s.count = p[EchoCount]->u.sd.value;
    s.opacity = percent(p, EchoOpacity); s.decay = percent(p, EchoDecay);
    s.offsetX = value(p, EchoOffsetX); s.offsetY = value(p, EchoOffsetY);
    s.zoom = value(p, EchoZoom); s.rotation = value(p, EchoRotation);
    s.blend = static_cast<echo::Blend>(p[EchoBlend]->u.pd.value);
    s.nearColour = colour(p, NearColour); s.midColour = colour(p, MidColour); s.farColour = colour(p, FarColour);
    s.tint = percent(p, TintMix); s.hueSpeed = value(p, HueSpeed);
    s.decayCurve = value(p, DamageCurve); s.pixelSize = value(p, PixelSize);
    s.bits = p[BitDepth]->u.sd.value; s.bend = value(p, CircuitBend);
    s.bandHeight = value(p, CircuitBandHeight); s.dropout = percent(p, Dropout);
    s.rgb = value(p, RGBSplit); s.wave = value(p, WaveAmount);
    s.wavelength = value(p, WaveLength); s.rate = value(p, SignalRate);
    s.seed = p[RandomSeed]->u.sd.value;
    s.audio = audio; s.audioOpacity = percent(p, AudioOpacity); s.audioMotion = percent(p, AudioMotion);
    s.audioDamage = percent(p, AudioDamage); s.audioColour = percent(p, AudioColour);
    s.scaleX = static_cast<float>(in->downsample_x.num) / std::max<A_long>(1, in->downsample_x.den);
    s.scaleY = static_cast<float>(in->downsample_y.num) / std::max<A_long>(1, in->downsample_y.den);
    s.pixelAspect = static_cast<float>(in->pixel_aspect_ratio.num) / std::max<A_long>(1, in->pixel_aspect_ratio.den);
    s.seconds = static_cast<double>(in->current_time) / std::max<A_u_long>(1, in->time_scale);
    return s;
}

template<typename P, int Max>
struct World {
    PF_LayerDef* world;
    P* row(int y) const { return reinterpret_cast<P*>(reinterpret_cast<char*>(world->data) + static_cast<std::ptrdiff_t>(y) * world->rowbytes); }
    echo::Pixel read(int x, int y) const {
        if (!world->data || x < 0 || y < 0 || x >= world->width || y >= world->height) return {};
        const auto& p = row(y)[x];
        return {p.alpha / float(Max), p.red / float(Max), p.green / float(Max), p.blue / float(Max)};
    }
    void write(int x, int y, echo::Pixel p) const {
        auto& q = row(y)[x];
        q.alpha = static_cast<decltype(q.alpha)>(std::lround(echo::clamp(p.a) * Max));
        q.red = static_cast<decltype(q.red)>(std::lround(echo::clamp(p.r) * Max));
        q.green = static_cast<decltype(q.green)>(std::lround(echo::clamp(p.g) * Max));
        q.blue = static_cast<decltype(q.blue)>(std::lround(echo::clamp(p.b) * Max));
    }
};

template<typename P, int Max>
PF_Err render(PF_InData* in, PF_ParamDef* p[], PF_LayerDef* output) {
    float audio = 0;
    PF_Err err = readAudio(in, p, audio);
    if (err) return err;
    const auto s = settings(in, p, audio);
    const World<P, Max> out{output}, source{&p[Input]->u.ld};
    const bool only = p[TrailsOnly]->u.bd.value;
    const float sourceOpacity = percent(p, SourceOpacity);
    auto sourceAt = [&](int x, int y) {
        auto pixel = source.read(x, y);
        pixel.a *= sourceOpacity; pixel.r *= sourceOpacity; pixel.g *= sourceOpacity; pixel.b *= sourceOpacity;
        return pixel;
    };
    for (int y = 0; y < output->height; ++y) {
        if ((y & 31) == 0 && (err = PF_ABORT(in))) return err;
        for (int x = 0; x < output->width; ++x)
            out.write(x, y, !only && s.blend != echo::Blend::Over ? sourceAt(x, y) : echo::Pixel{});
    }
    // One historical frame at a time keeps memory bounded even with 48 echoes.
    for (int i = s.count; i >= 1; --i) {
        const auto state = echo::prepare(s, i);
        if (state.opacity < .00001f) continue;
        const double target = in->current_time - i * static_cast<double>(value(p, EchoDelay)) * in->time_scale;
        if (target < std::numeric_limits<A_long>::min() || target > std::numeric_limits<A_long>::max()) continue;
        FrameCheckout frame{in};
        PF_LayerDef* historical = &p[Input]->u.ld;
        if (value(p, EchoDelay) > 0) {
            frame.attempted = true;
            err = PF_CHECKOUT_PARAM(in, Input, static_cast<A_long>(std::llround(target)), in->time_step, in->time_scale, &frame.param);
            if (err) return err;
            historical = &frame.param.u.ld;
        }
        if (!historical->data) continue; // Outside source duration: transparent, never wrap to the first frame.
        if (PF_WORLD_IS_DEEP(historical) != PF_WORLD_IS_DEEP(output)) return PF_Err_BAD_CALLBACK_PARAM;
        const World<P, Max> past{historical};
        const auto reader = [&](int x, int y) { return past.read(x, y); };
        for (int y = 0; y < output->height; ++y) {
            if ((y & 15) == 0 && (err = PF_ABORT(in))) return err;
            echo::renderRow(s, state, i, y, output->width, (output->width - 1) * .5f, (output->height - 1) * .5f,
                reader, [&](int x, echo::Pixel pixel) { out.write(x, y, echo::composite(out.read(x, y), pixel, s.blend)); });
        }
        err = PF_PROGRESS(in, s.count - i + 1, std::max(1, s.count));
        if (err) return err;
    }
    if (!only && s.blend == echo::Blend::Over) {
        for (int y = 0; y < output->height; ++y) {
            if ((y & 31) == 0 && (err = PF_ABORT(in))) return err;
            for (int x = 0; x < output->width; ++x)
                out.write(x, y, echo::composite(out.read(x, y), sourceAt(x, y), echo::Blend::Over));
        }
    }
    return PF_Err_NONE;
}
} // namespace

extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr ptr, PF_PluginDataCB2 callback,
    SPBasicSuite*, const char*, const char*) {
    PF_Err result = PF_Err_NONE; // The Adobe registration macro references this local by name.
    result = PF_REGISTER_EFFECT_EXT2(ptr, callback, ECHO_NAME, ECHO_MATCH_NAME, ECHO_CATEGORY,
        AE_RESERVED_INFO, "EffectMain", "");
    return result;
}

extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData* in, PF_OutData* out,
    PF_ParamDef* p[], PF_LayerDef* output, void*) {
    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:
                std::snprintf(out->return_msg, sizeof(out->return_msg),
                    "Echo Trails 1.0.1\rAudio-reactive temporal echoes, colour decay and circuit bending.\rSelect Music Layer in Audio Reaction. Precompose animated layers before applying.");
                break;
            case PF_Cmd_GLOBAL_SETUP:
                out->my_version = ECHO_VERSION;
                out->out_flags = ECHO_OUT_FLAGS;
                out->out_flags2 = ECHO_OUT_FLAGS2;
                break;
            case PF_Cmd_PARAMS_SETUP: return paramsSetup(in, out);
            case PF_Cmd_RENDER:
                return PF_WORLD_IS_DEEP(output) ? render<PF_Pixel16, PF_MAX_CHAN16>(in, p, output)
                                                : render<PF_Pixel8, PF_MAX_CHAN8>(in, p, output);
            default: break;
        }
        return PF_Err_NONE;
    } catch (PF_Err error) { return error; }
      catch (const std::bad_alloc&) { return PF_Err_OUT_OF_MEMORY; }
      catch (...) {
        // INTERNAL_STRUCT_DAMAGED tells AE to shut down; an unknown exception
        // does not establish corruption. Fail this operation with a useful message.
        if (out) {
            std::snprintf(out->return_msg, sizeof(out->return_msg),
                "Echo Trails 1.0.1: unexpected exception during command %d.", static_cast<int>(cmd));
            out->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
        }
        return PF_Err_BAD_CALLBACK_PARAM;
      }
}
