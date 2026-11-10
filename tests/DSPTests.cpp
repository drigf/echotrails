#include "EchoDSP.h"
#include <cstdlib>
#include <iostream>
#include <vector>

static void require(bool pass, const char* label) {
    if (!pass) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
static bool close(float a, float b, float tolerance = .001f) { return std::abs(a-b) < tolerance; }

int main() {
    echo::Settings s;
    float previousDamage = -1, previousOpacity = 2;
    for (int i = 1; i <= s.count; ++i) {
        const auto e = echo::prepare(s, i);
        require(e.damage > previousDamage, "damage grows with echo age");
        require(e.opacity < previousOpacity, "opacity decays with echo age");
        previousDamage = e.damage; previousOpacity = e.opacity;
    }
    const auto near = echo::prepare(s, 1), far = echo::prepare(s, s.count);
    require(close(near.colour.g, s.nearColour.g), "near gradient endpoint");
    require(close(far.colour.r, s.farColour.r), "far gradient endpoint");
    s.audio = 1;
    const auto reactive = echo::prepare(s, 1);
    require(reactive.offsetX > near.offsetX && reactive.damage > near.damage && reactive.opacity > near.opacity,
            "audio modulates motion, damage, opacity");
    s.audio = 0;
    s.decay = 0;
    require(close(echo::prepare(s, 1).opacity, s.opacity) && echo::prepare(s, 2).opacity == 0,
            "zero retention keeps first echo only");
    require(echo::noise(42, 3, 12, 2) == echo::noise(42, 3, 12, 2), "random-access deterministic glitch");
    require(echo::noise(42, 3, 12, 2) != echo::noise(43, 3, 12, 2), "seed changes glitch");
    for (auto mode : {echo::Blend::Over, echo::Blend::Screen, echo::Blend::Add}) {
        const auto p = echo::composite({.5f,.5f,.1f,0}, {.5f,0,.2f,.5f}, mode);
        require(close(p.a, .75f), "coverage union");
        require(p.r <= p.a && p.g <= p.a && p.b <= p.a, "premultiplied compositing");
        const auto transparent = echo::composite({}, {}, mode);
        require(transparent.a == 0 && transparent.r == 0, "transparent stays transparent");
    }
    std::vector<int16_t> samples(44100);
    auto tone = [&](double frequency) {
        for (std::size_t i = 0; i < samples.size()/2; ++i) {
            samples[i*2] = static_cast<int16_t>(16000 * std::sin(2 * echo::pi * frequency * i / 22050));
            samples[i*2+1] = -samples[i*2];
        }
    };
    auto envelope = [&](echo::Band band) { return echo::audioEnvelope(samples.data(), samples.size()/2, 2, 22050, band, 0, 0); };
    require(envelope(echo::Band::Full) == 0, "silence is zero");
    tone(80);
    require(envelope(echo::Band::Full) > .3f, "stereo phase cancellation avoided");
    require(envelope(echo::Band::Bass) > envelope(echo::Band::High) * 5, "bass band isolates low tone");
    tone(7000);
    require(envelope(echo::Band::High) > envelope(echo::Band::Bass) * 5, "high band isolates high tone");
    require(echo::audioEnvelope(samples.data(), samples.size()/2, 2, 22050, echo::Band::Full, 0, .9f) == 0,
            "noise gate rejects quiet signal");
    require(echo::audioEnvelope(nullptr, 100, 2, 22050, echo::Band::Full, 12, 0) == 0, "missing audio");

    // Render an isolated premultiplied sprite: spatial shift, empty edges, no dark alpha fringe.
    s = {}; s.count = 1; s.offsetX = 3; s.offsetY = 0; s.opacity = 1; s.tint = 0;
    s.pixelSize = 1; s.bits = 16; s.bend = s.rgb = s.wave = s.dropout = 0;
    std::vector<echo::Pixel> pixels(16);
    auto reader = [](int x, int y) -> echo::Pixel { return x == 4 && y == 0 ? echo::Pixel{.5f,.5f,0,0} : echo::Pixel{}; };
    echo::renderRow(s, echo::prepare(s,1), 1, 0, 16, 7.5f, 0, reader,
                    [&](int x, echo::Pixel p) { pixels[x] = p; });
    require(close(pixels[7].a,.5f) && close(pixels[7].r,.5f), "spatial offset preserves premultiplication");
    require(pixels[4].a == 0 && pixels[0].a == 0, "shift vacates origin and keeps edges empty");
    std::cout << "DSP checks passed: decay, colours, modulation, audio bands, alpha, sampling.\n";
}
