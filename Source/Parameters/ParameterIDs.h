#pragma once

namespace alr::params
{
// Global & Output
inline constexpr const char* bypass       = "bypass";
inline constexpr const char* autoGainComp = "auto_gain_comp";
inline constexpr const char* outputGain   = "output_gain";
inline constexpr const char* globalMix    = "global_mix";

// Voice Color Engine
inline constexpr const char* body         = "vc_body";
inline constexpr const char* warmth       = "vc_warmth";
inline constexpr const char* presence     = "vc_presence";
inline constexpr const char* air          = "vc_air";
inline constexpr const char* clarity      = "vc_clarity";
inline constexpr const char* density      = "vc_density";
inline constexpr const char* smooth       = "vc_smooth";
inline constexpr const char* color        = "vc_color";

// ALR Pitch Engine
inline constexpr const char* pitchEnabled = "pitch_enabled";
inline constexpr const char* pitchRoot    = "pitch_root";
inline constexpr const char* pitchScale   = "pitch_scale";
inline constexpr const char* retuneSpeed  = "pitch_retune_speed";
inline constexpr const char* humanize     = "pitch_humanize";
inline constexpr const char* flex         = "pitch_flex";
inline constexpr const char* pitchRange   = "pitch_range";
inline constexpr const char* pitchMix     = "pitch_mix";
} // namespace alr::params
