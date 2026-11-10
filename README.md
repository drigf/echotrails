# Echo Trails

A native macOS After Effects effect written in C++17. Edit the source in VS Code and build the generated project in Xcode. Creates historical-frame echoes with music-driven motion, colour, and progressive signal damage.

## Start here

1. Build using the commands below (a built bundle is in `build/products/Release/EchoTrails.plugin`).
2. Quit After Effects. Copy **EchoTrails.plugin** into `/Applications/Adobe After Effects 2026/Plug-ins/`, or run `bash scripts/install.sh`. For 2025, pass its Plug-ins directory to the script. Finder may request administrator permission.
3. Restart After Effects. Apply **Effect > Stylize > Echo Trails** to footage or a precomposition.
4. In **Audio Reaction > Music Layer**, select your music layer in the same composition. Start with **Full Spectrum** or **Bass** and adjust **Audio Gain (dB)**.
5. Change controls in Effect Controls; numeric controls and colours support keyframes and expressions.

For a quick example, run **File > Scripts > Run Script File… > `examples/Create Demo.jsx`**. It adds a moving ring precomposition and the effect to your current project without replacing existing items. Choose a music file, or cancel to use a pulsing Manual Drive expression.

**Precompose moving text, shapes, and layers first**, choosing **Move all attributes into the new composition**, then apply Echo Trails to that precomposition. Layer Position/Scale/Rotation are normally applied after effects; precomposing makes that motion available in historical frames. Precompose upstream effects too when their animation needs to appear in the echoes. Leave transparent room around the subject: trails are clipped to the input layer's canvas. Use a comp-sized precomposition for generous bounds.

## Build and edit

Requirements: macOS 12+, full Xcode with command-line tools, CMake 3.24+, and the Adobe After Effects C++ SDK. The default SDK location is `~/Developer/AfterEffectsSDK`.

From the project directory:

```sh
cmake --preset xcode
cmake --build --preset release
ctest --preset release
```

For another SDK location:

```sh
cmake --preset xcode -DAE_SDK_ROOT="/path/to/AfterEffectsSDK"
```

`AE_SDK_ROOT` is the directory containing `Examples/Headers`, not `Examples` itself. SDK files remain external and are not redistributed here.

- **VS Code:** open this folder. Install the recommended C++ and CMake Tools extensions. **Cmd+Shift+B** configures and builds Release; **Tasks: Run Test Task** runs the tests.
- **Xcode:** open `build/EchoTrails.xcodeproj`, select **EchoTrails** (or **ALL_BUILD**) and **My Mac**, then build. Set the scheme's Run build configuration to Release for the release bundle. CMake owns the generated project; change build settings in `CMakeLists.txt` so regeneration preserves them.
- **Output:** `build/products/Release/EchoTrails.plugin`; Debug goes under `build/products/Debug/`.
- **Architectures:** arm64 and x86_64 in one universal bundle, including a compiled PiPL resource and local ad-hoc signature. Public distribution would need your own signing/notarization process.
- After edits, quit AE, rebuild, replace the installed bundle, then restart AE. Keep only one installed copy of the effect.

## Controls

| Group | What you can change |
| --- | --- |
| Audio Reaction | Music layer; full/bass/mid/high frequency bands; gain; noise gate; response window; manual drive; separate audio amounts for opacity, motion, damage, colour |
| Echo Trails | 0–48 echoes; delay in seconds; opacity; retention; X/Y offset; scale and rotation per echo; Normal/Screen/Add blend; source opacity; trails only |
| Trail Colours | Near, middle and far colour swatches; colour mix; animated hue cycling |
| Decay / Circuit Bending | Damage curve; far pixel size; colour bit depth; horizontal band tearing; band height; dropout; RGB separation; sine-wave displacement; wavelength; signal speed; random seed |

**Echo Retention** controls fading: echo `n` uses `Trail Opacity × Retention^(n−1)`. Lower values fade faster; 100% keeps equal opacity. **Damage Falloff Curve** controls where degradation happens: above 1 keeps early echoes cleaner; below 1 damages them sooner. Pixel size, bit depth, bending, RGB separation, modulation and dropout all progressively ramp with echo age. Audio can push that ramp harder.

The three colour swatches form a continuous gradient along the trail. Colour Mix interpolates between the original image and a luminance-tinted echo. Set it to 0, with Audio to Colour also 0, to retain source colours.

**Screen** is the default and keeps echoes visible on opaque footage. **Normal** lays the original above the echoes, which works well for isolated subjects on transparency; an opaque original can cover the trails. **Add** produces a brighter signal. All modes preserve premultiplied transparency. Trail colours do not tint the current source.

Music is analyzed directly through AE's audio callbacks; no Convert Audio to Keyframes step is required. Analysis uses stereo RMS with separate channel energies, approximate one-pole frequency bands, a trailing weighted response window, gain, then a gate. The window smooths the response and introduces some lag; shorten it for sharper transients. Manual Drive supplies a minimum level and can be linked to an Audio Amplitude slider with an expression. The effect changes the **image**, not the soundtrack.

Suggested starting points:

| Look | Settings |
| --- | --- |
| Soft neon echoes | 12 echoes, 90% retention, pixels 4, bend 5, RGB 3, dropout 0%, wave 4 |
| Broken digital signal | 20 echoes, 91% retention, pixels 48, bits 3, bend 100, RGB 28, dropout 30%, wave 35 |
| Bass-driven circuit trails | Bass, gain 18 dB, response 80 ms, audio motion 80%, audio damage 120% |

## Implementation and limits

- `src/EchoTrails.cpp`: native entry points, Effect Controls, audio/history checkouts, 8/16 bpc world adapter.
- `src/EchoDSP.h` / `src/EchoDSP.cpp`: compositing, gradients, audio analysis, spatial modulation and progressive degradation.
- `src/Parameters.h`: stable parameter/disk IDs. Preserve existing IDs if you extend the effect.
- `src/PluginMetadata.h`: shared version and flags used by both code and PiPL; compile-time assertions guard consistency.
- `resources/EchoTrailsPiPL.r`: Adobe effect discovery metadata.
- `tests/`: DSP tests and a simulated AE host exercising parameter registration, audio, temporal checkout, failure cleanup, cancellation and padded 16 bpc buffers.

CPU renderer supporting **8 and 16 bits per channel**. This version does not advertise native 32-bit float/SmartFX support; use an 8/16 bpc composition for predictable results. No GPU acceleration. Reduce Echo Count and use Half/Quarter previews for heavy compositions. 48 echoes can be expensive, especially with upstream effects. Only one historical frame is held at a time; random state is deterministic per time/seed and there is no mutable shared render state, permitting multi-frame rendering.

History is requested in layer time at `current_time − echo_index × delay`. Empty historical buffers are transparent. Signal animation and audio evaluation use layer time too, so offset, stretch and time remapping should be included in host acceptance checks for your projects. The music envelope is evaluated at the current frame and drives the whole trail.

## Validation

`ctest --preset release` runs algorithm and simulated host tests; it is not a substitute for an actual AE render. The plug-in also receives bundle, universal-architecture, exported-symbol and signature checks during delivery. See `VALIDATION.md` for the checks actually performed and remaining host checks.

For an AE acceptance pass: run the demo, select music, scrub and preview, change each group, render at 8 and 16 bpc, compare Full/Half preview geometry, test a time-shifted music layer, save/reopen, and compare multi-frame rendering on/off. Verify transparent output with Trails Only over a checkerboard. At the beginning of a clip, historical echoes may not yet exist.

SDK references: local Adobe SDK headers and Checkout/SDK_Backwards examples, plus the [audio callback guide](https://ae-plugins.docsforadobe.dev/audio/accessing-audio-data/) and [PiPL guide](https://ae-plugins.docsforadobe.dev/intro/pipl-resources/).
