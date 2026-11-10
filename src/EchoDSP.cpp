#include "EchoDSP.h"

namespace echo {
float noise(uint32_t seed, int64_t band, int64_t frame, int echoIndex) {
    uint32_t h = seed ^ (static_cast<uint32_t>(band) * 374761393U)
        ^ (static_cast<uint32_t>(frame) * 668265263U) ^ (static_cast<uint32_t>(echoIndex) * 2246822519U);
    h = (h ^ (h >> 13)) * 1274126177U;
    h ^= h >> 16;
    return static_cast<float>(h & 0xffffffU) / 8388607.5f - 1;
}

static Colour hueRotate(Colour c, float turns) {
    const float a = turns * static_cast<float>(2 * pi), co = std::cos(a), si = std::sin(a);
    return {
        clamp((.213f + co*.787f - si*.213f)*c.r + (.715f - co*.715f - si*.715f)*c.g + (.072f - co*.072f + si*.928f)*c.b),
        clamp((.213f - co*.213f + si*.143f)*c.r + (.715f + co*.285f + si*.140f)*c.g + (.072f - co*.072f - si*.283f)*c.b),
        clamp((.213f - co*.213f - si*.787f)*c.r + (.715f - co*.715f + si*.715f)*c.g + (.072f + co*.928f + si*.072f)*c.b)};
}

EchoState prepare(const Settings& s, int index) {
    EchoState e{};
    e.age = static_cast<float>(index) / std::max(1, s.count);
    e.damage = clamp(std::pow(e.age, s.decayCurve) * (1 + s.audio * s.audioDamage * 2));
    e.opacity = clamp(s.opacity * std::pow(s.decay, index - 1) * (1 + s.audio * s.audioOpacity));
    const float motion = 1 + s.audio * s.audioMotion * 2;
    e.offsetX = s.offsetX * index * motion * s.scaleX;
    e.offsetY = s.offsetY * index * motion * s.scaleY;
    e.inverseZoom = 1 / std::max(.05f, std::pow(1 + s.zoom * .01f, static_cast<float>(index)));
    const float angle = s.rotation * index * static_cast<float>(pi / 180);
    e.sine = std::sin(angle); e.cosine = std::cos(angle);
    e.blockX = std::max(1.0f, (1 + (s.pixelSize - 1) * e.damage) * s.scaleX);
    e.blockY = std::max(1.0f, (1 + (s.pixelSize - 1) * e.damage) * s.scaleY);
    e.bend = s.bend * e.damage * s.scaleX;
    e.split = s.rgb * e.damage * s.scaleX;
    e.wave = s.wave * e.damage * s.scaleX;
    e.levels = std::exp2(16 + (s.bits - 16) * e.damage) - 1;
    // The first and last echoes reach the chosen endpoints, regardless of count.
    const float t = s.count > 1 ? static_cast<float>(index - 1) / (s.count - 1) : 0;
    const Colour a = t < .5f ? s.nearColour : s.midColour;
    const Colour b = t < .5f ? s.midColour : s.farColour;
    const float f = t < .5f ? t * 2 : (t - .5f) * 2;
    e.colour = hueRotate({a.r + (b.r-a.r)*f, a.g + (b.g-a.g)*f, a.b + (b.b-a.b)*f},
                        static_cast<float>(std::fmod(s.seconds * s.hueSpeed, 1.0)) + s.audio * s.audioColour * e.age);
    return e;
}

Pixel composite(Pixel d, Pixel s, Blend mode) {
    Pixel o{};
    o.a = s.a + d.a * (1 - s.a);
    auto channel = [&](float dc, float sc) {
        if (mode == Blend::Screen) return std::min(o.a, sc + dc - sc * dc);
        if (mode == Blend::Add) return std::min(o.a, sc + dc);
        return sc + dc * (1 - s.a);
    };
    o.r = channel(d.r, s.r); o.g = channel(d.g, s.g); o.b = channel(d.b, s.b);
    return o;
}

float audioEnvelope(const int16_t* samples, std::size_t frames, int channels,
                    double sampleRate, Band band, float gainDB, float threshold) {
    if (!samples || frames == 0 || channels < 1 || channels > 2 || sampleRate <= 0) return 0;
    const double bassA = 1 - std::exp(-2 * pi * 250 / sampleRate);
    const double highA = 1 - std::exp(-2 * pi * 2500 / sampleRate);
    double low[2]{}, mid[2]{}, energy = 0, weights = 0;
    for (std::size_t i = 0; i < frames; ++i) {
        // Recent samples carry more weight, without history shared between render threads.
        const double weight = .1 + .9 * static_cast<double>(i + 1) / frames;
        for (int ch = 0; ch < channels; ++ch) {
            const double x = samples[i * channels + ch] / 32768.0;
            low[ch] += bassA * (x - low[ch]);
            mid[ch] += highA * (x - mid[ch]);
            double filtered = x;
            if (band == Band::Bass) filtered = low[ch];
            else if (band == Band::Mid) filtered = mid[ch] - low[ch];
            else if (band == Band::High) filtered = x - mid[ch];
            energy += filtered * filtered * weight;
            weights += weight;
        }
    }
    const float level = static_cast<float>(std::sqrt(energy / weights) * std::pow(10, gainDB / 20));
    return clamp((level - threshold) / std::max(.001f, 1 - threshold));
}
} // namespace echo
