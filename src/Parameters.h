#pragma once

// These IDs are also the persistent disk IDs. Append new controls; do not reorder released IDs.
enum Param {
    Input = 0,
    AudioGroup, AudioLayer, AudioBand, AudioGain, AudioThreshold, AudioWindow, ManualDrive,
    AudioOpacity, AudioMotion, AudioDamage, AudioColour, AudioEnd,
    EchoGroup, EchoCount, EchoDelay, EchoOpacity, EchoDecay, EchoOffsetX, EchoOffsetY,
    EchoZoom, EchoRotation, EchoBlend, SourceOpacity, TrailsOnly, EchoEnd,
    ColourGroup, NearColour, MidColour, FarColour, TintMix, HueSpeed, ColourEnd,
    DamageGroup, DamageCurve, PixelSize, BitDepth, CircuitBend, CircuitBandHeight,
    Dropout, RGBSplit, WaveAmount, WaveLength, SignalRate, RandomSeed, DamageEnd,
    ParamCount
};
