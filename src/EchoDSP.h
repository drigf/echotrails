#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace echo {
constexpr double pi = 3.14159265358979323846;
inline float clamp(float x) { return std::clamp(x, 0.0f, 1.0f); }
struct Pixel { float a = 0, r = 0, g = 0, b = 0; }; // premultiplied RGBA
struct Colour { float r, g, b; };
enum class Blend { Over = 1, Screen, Add };
enum class Band { Full = 1, Bass, Mid, High };

struct Settings {
    int count = 12;
    float opacity = .55f, decay = .86f, decayCurve = 1.5f;
    float offsetX = 12, offsetY = 0, zoom = 0, rotation = 0;
    float pixelSize = 24, bend = 35, bandHeight = 12, dropout = .10f;
    float bits = 5, rgb = 12, wave = 12, wavelength = 160, rate = 2;
    float tint = .75f, hueSpeed = 0;
    Colour nearColour{.1f, .9f, 1}, midColour{.8f, .15f, 1}, farColour{1, .15f, .1f};
    float audio = 0, audioOpacity = .35f, audioMotion = .4f;
    float audioDamage = .7f, audioColour = .2f;
    float scaleX = 1, scaleY = 1, pixelAspect = 1;
    double seconds = 0;
    uint32_t seed = 1;
    Blend blend = Blend::Screen;
};

struct EchoState {
    float age, damage, opacity, offsetX, offsetY, inverseZoom, sine, cosine;
    float blockX, blockY, bend, split, wave, levels;
    Colour colour;
};

float noise(uint32_t seed, int64_t band, int64_t frame, int echoIndex);
EchoState prepare(const Settings& s, int index);
Pixel composite(Pixel dst, Pixel src, Blend mode);
float audioEnvelope(const int16_t* samples, std::size_t frames, int channels,
                    double sampleRate, Band band, float gainDB, float threshold);

// Reader returns transparent black outside its image. Interpolation stays premultiplied.
template <typename Reader>
Pixel sample(const Reader& read, float x, float y, bool nearest) {
    if (nearest) return read(static_cast<int>(std::floor(x + .5f)), static_cast<int>(std::floor(y + .5f)));
    const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
    const float fx = x - ix, fy = y - iy;
    const Pixel a = read(ix, iy), b = read(ix + 1, iy), c = read(ix, iy + 1), d = read(ix + 1, iy + 1);
    auto mix = [&](float av, float bv, float cv, float dv) {
        return (av + (bv - av) * fx) * (1 - fy) + (cv + (dv - cv) * fx) * fy;
    };
    return {mix(a.a,b.a,c.a,d.a), mix(a.r,b.r,c.r,d.r), mix(a.g,b.g,c.g,d.g), mix(a.b,b.b,c.b,d.b)};
}

// Each call renders one row of one echo. Temporal checkout and buffer ownership live in the host adapter.
template <typename Reader, typename Writer>
void renderRow(const Settings& s, const EchoState& e, int index, int y, int width,
               float centreX, float centreY, const Reader& read, const Writer& write) {
    const float syScale = std::max(.001f, s.scaleY);
    const float sxScale = std::max(.001f, s.scaleX);
    const auto band = static_cast<int64_t>(std::floor(y / (std::max(1.0f, s.bandHeight) * syScale)));
    const auto tick = static_cast<int64_t>(std::floor(s.seconds * s.rate * 12));
    const float n = noise(s.seed, band, tick, index);
    const float tear = std::abs(n) > .35f ? n * e.bend : 0;
    const float gate = noise(s.seed ^ 0x5bd1e995U, band, tick, index) * .5f + .5f;
    const float dropout = gate < s.dropout * e.damage ? .08f : 1.0f;
    const float phase = static_cast<float>(2 * pi * s.seconds * s.rate + index * .71);
    const float wobble = e.wave * std::sin(y / syScale / std::max(1.0f, s.wavelength) * 2 * pi + phase);
    const float tint = clamp(s.tint + s.audio * s.audioColour);
    const bool blocky = e.blockX > 1.5f || e.blockY > 1.5f;
    for (int x = 0; x < width; ++x) {
        // Rotate in full-resolution, square-pixel space so preview resolution/PAR do not change geometry.
        const float dx = (x - centreX - e.offsetX - tear - wobble) / sxScale * s.pixelAspect;
        const float dy = (y - centreY - e.offsetY) / syScale;
        float u = (e.cosine * dx + e.sine * dy) * e.inverseZoom / s.pixelAspect * sxScale + centreX;
        float v = (-e.sine * dx + e.cosine * dy) * e.inverseZoom * syScale + centreY;
        if (blocky) {
            u = (std::floor(u / e.blockX) + .5f) * e.blockX - .5f;
            v = (std::floor(v / e.blockY) + .5f) * e.blockY - .5f;
        }
        Pixel p = sample(read, u, v, blocky);
        if (e.split > .01f) {
            const Pixel red = sample(read, u + e.split, v, blocky);
            const Pixel blue = sample(read, u - e.split, v, blocky);
            p.r = red.r; p.b = blue.b;
            p.a = std::max({p.a, red.a, blue.a});
        }
        if (p.a > 1e-6f) {
            float r = clamp(p.r / p.a), g = clamp(p.g / p.a), b = clamp(p.b / p.a);
            const float luminance = .2126f * r + .7152f * g + .0722f * b;
            r += (luminance * e.colour.r - r) * tint;
            g += (luminance * e.colour.g - g) * tint;
            b += (luminance * e.colour.b - b) * tint;
            if (e.levels < 65535) {
                r = std::round(r * e.levels) / e.levels;
                g = std::round(g * e.levels) / e.levels;
                b = std::round(b * e.levels) / e.levels;
            }
            p.a *= e.opacity * dropout;
            p.r = r * p.a; p.g = g * p.a; p.b = b * p.a;
        } else { p = {}; }
        write(x, p);
    }
}
} // namespace echo
