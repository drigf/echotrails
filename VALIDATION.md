# Validation — 7 November 2026

## 1.0.1 crash-handling investigation

The reported 10::30 failure happens when applying the effect. The AE 26.5 loading log recognizes the installed plug-in. A scripted baseline test applied the installed effect to a 64×64 solid and exported PNGs at 0 and 0.5 seconds successfully; this did not reproduce the reported UI crash, so its exact trigger remains unconfirmed.

Two unsafe paths were corrected: audio cleanup passed a null handle back to AE when checkout returned no audio, and the catch-all exception handler converted even a thrown Adobe error/cancellation into `PF_Err_INTERNAL_STRUCT_DAMAGED`. Version 1.0.1 checks in only acquired audio handles, prevents cleanup exceptions escaping destructors, preserves thrown Adobe error codes, and reports unknown exceptions as an operation error rather than asserting structural corruption. Parameter IDs and the Signal Forge match name are unchanged.

The new regression test failed on the previous implementation with `never check in a null audio handle`. After the fix, both test executables pass, including the thrown-cancellation test. The universal Release bundle and signature validate. **The 1.0.1 bundle has not yet been installed or tested in AE; the reported crash still needs a host retest.**

## Completed

- Generated `build/EchoTrails.xcodeproj` with CMake's Xcode generator.
- Compiled the Release plug-in and test executables for both **arm64** and **x86_64** using Xcode / Apple Clang.
- Ran `ctest --preset release` on this Apple Silicon Mac: **2 of 2 test executables passed**. Assertions use explicit failure checks and remain active in Release.
- DSP coverage: progressive damage and opacity decay, gradient endpoints, audio modulation, deterministic glitches, zero retention, premultiplied compositing, stereo polarity, frequency bands, silence/gate, spatial offset and transparent boundaries.
- Simulated AE host coverage: parameter count and stable IDs, balanced groups, matching global/PiPL flags, temporal sampling, stereo audio request format, resource check-in on errors/cancellation, zero-echo identity, zero-delay sampling, empty history, padded 16 bpc rows.
- Verified the bundle contains a data-fork PiPL resource with both architecture entry points. Inspected with `xcrun DeRez -useDF -only PiPL`.
- Verified the universal Mach-O and exported `EffectMain` / `PluginDataEntryFunction2` entry points.
- Verified the bundle's local ad-hoc signature with `codesign --verify --strict` and its plist with `plutil -lint`.
- Checked installer shell syntax with `bash -n scripts/install.sh`.

## Not yet verified in the Adobe host

This delivery has **not been installed or rendered inside After Effects**. The automated host tests simulate Adobe callbacks; they cannot verify Adobe's actual layer/audio time mapping, effect discovery, UI, saved-project compatibility, or multi-frame scheduling. Intel code is compiled, but tests have run on Apple Silicon only.

After installation, use `examples/Create Demo.jsx` and the acceptance steps in `README.md` to confirm those behaviors. In particular, test audio-layer offsets/time remapping, preview resolution, long timelines, save/reopen, and multi-frame rendering. Native 32-bit float rendering is not implemented.
