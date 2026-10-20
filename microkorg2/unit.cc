/*
 *  File: unit.cc
 *
 *  VectorFM - logue SDK callbacks for the microKORG 2 (polyphonic)
 *
 *  One VectorFM instance per voice. Parameters are shared, the movement LFO is
 *  shared, and the virtual patch can modulate Scan, FM, Morph and Fold per voice.
 */

#include "unit_osc.h"
#include "utils/mk2_utils.h"
#include "synth.h"

namespace {

enum { kDestScan = 0, kDestFm, kDestMorph, kDestFold, kNumDest };

// Same output level as Korg's microKORG 2 example oscillators
constexpr float kOutputTrim = 0.3f;

VectorFM s_voices[kMk2MaxVoices];
float s_mod[kNumDest][kMk2MaxVoices];
int32_t s_cached[VectorFM::kNumParams];
unit_runtime_desc_t s_desc;

inline const unit_runtime_osc_context_t *context() {
  return static_cast<const unit_runtime_osc_context_t *>(s_desc.hooks.runtime_context);
}

inline int voiceCount(const unit_runtime_osc_context_t *ctx) {
  const int n = ctx->voiceLimit;
  return n < 0 ? 0 : (n > kMk2MaxVoices ? kMk2MaxVoices : n);
}

void writeName(char *dst, const char *name) {
  int i = 0;
  for (; name[i] && i < 8; ++i) dst[i] = name[i];
  dst[i] = 0;
}

}  // namespace

__attribute__((used)) int8_t unit_init(const unit_runtime_desc_t *desc) {
  if (!desc) return k_unit_err_undef;
  if (desc->target != unit_header.target) return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
  if (desc->samplerate != 48000) return k_unit_err_samplerate;

  s_desc = *desc;

  for (int v = 0; v < kMk2MaxVoices; ++v) {
    s_voices[v].init();
    for (int d = 0; d < kNumDest; ++d) s_mod[d][v] = 0.f;
  }
  for (int id = 0; id < VectorFM::kNumParams; ++id) {
    s_cached[id] = static_cast<int32_t>(unit_header.params[id].init);
    for (int v = 0; v < kMk2MaxVoices; ++v) s_voices[v].setParam(id, s_cached[id]);
  }
  return k_unit_err_none;
}

__attribute__((used)) void unit_teardown() {}

__attribute__((used)) void unit_reset() {
  for (int v = 0; v < kMk2MaxVoices; ++v) s_voices[v].reset();
}

__attribute__((used)) void unit_resume() {}

__attribute__((used)) void unit_suspend() {}

__attribute__((used)) void unit_render(const float *in, float *out, uint32_t frames) {
  (void)in;
  const unit_runtime_osc_context_t *ctx = context();
  const int n = voiceCount(ctx);
  const float move_phase = s_voices[0].movePhase();

  for (int v = 0; v < n; ++v) {
    VectorFM &voice = s_voices[v];
    const float pitch = ctx->pitch[v];
    const uint8_t note = static_cast<uint8_t>(pitch);
    const float frac = pitch - static_cast<float>(note);
    voice.setPitch(osc_w0f_for_note(note, static_cast<uint8_t>(frac * 255.f)));
    voice.setShapeLfo(s_mod[kDestScan][v]);
    voice.setMod(s_mod[kDestFm][v], s_mod[kDestMorph][v], s_mod[kDestFold][v]);
    voice.setMovePhase(move_phase);  // all voices share one movement LFO

    const uint32_t offset = GetBufferOffset(ctx, v, frames);
    voice.process(out + offset + (v & 3), frames, ctx->outputStride, kOutputTrim);
  }
}

__attribute__((used)) void unit_set_param_value(uint8_t id, int32_t value) {
  if (id >= VectorFM::kNumParams) return;
  value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
  s_cached[id] = value;
  for (int v = 0; v < kMk2MaxVoices; ++v) s_voices[v].setParam(id, value);
}

__attribute__((used)) int32_t unit_get_param_value(uint8_t id) {
  if (id >= VectorFM::kNumParams) return 0;
  return s_cached[id];
}

__attribute__((used)) const char *unit_get_param_str_value(uint8_t id, int32_t value) {
  switch (id) {
    case VectorFM::kRatio:
      return VectorFM::ratioName(value);
    case VectorFM::kMode:
      return VectorFM::modeName(value);
    default:
      return nullptr;
  }
}

__attribute__((used)) void unit_set_tempo(uint32_t tempo) { (void)tempo; }

// Virtual patch: modulation data and destination names
__attribute__((used)) void unit_platform_exclusive(uint8_t messageId, void *data, uint32_t dataSize) {
  (void)dataSize;
  switch (messageId) {
    case kMk2PlatformExclusiveModData: {
      const unit_runtime_osc_context_t *ctx = context();
      const int n = voiceCount(ctx);
      const int32_t *index = GetModIndex(data);
      const float *depth = GetModDepth(data);
      for (int d = 0; d < kNumDest; ++d)
        for (int v = 0; v < kMk2MaxVoices; ++v) s_mod[d][v] = 0.f;
      for (int src = 0; src < kNumMk2ModSrc; ++src) {
        const int32_t dest = index[src];
        if (dest < 0 || dest >= kNumDest) continue;
        const float *mod = GetModSourceData(data, src, ctx->voiceLimit, 0);
        for (int v = 0; v < n; ++v) s_mod[dest][v] += mod[v] * depth[src];
      }
      break;
    }
    case kMk2PlatformExclusiveModDestName: {
      char *name = GetModDestNameData(data);
      switch (name[0]) {
        case kDestScan: writeName(name + 1, "Scan"); break;
        case kDestFm: writeName(name + 1, "FM"); break;
        case kDestMorph: writeName(name + 1, "Morph"); break;
        case kDestFold: writeName(name + 1, "Fold"); break;
        default: break;
      }
      break;
    }
    default:
      break;
  }
}
