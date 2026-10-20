#pragma once
/*
 * VectorFM - wavetable / phase-modulation oscillator for the Korg NTS-1 mkII
 *
 * Signal flow (per oscillator):
 *   sine modulator (ratio x pitch) --PM--> 6-shape scanning wavetable
 * Two oscillators (detuned, osc 2 reads an offset scan position) are
 * crossfaded by MORPH, then wavefolded and soft-saturated.
 *
 * This header has no hardware dependencies so it can be rendered on a
 * desktop machine for testing (see test/render.cc).
 */

#include <stdint.h>
#include "utils/float_math.h"
#include "tables.h"

// Anonymous namespace: keeps every DSP symbol internal to the unit so the
// .nts1mkiiunit only exports the logue SDK callbacks (like Korg's examples).
namespace {

constexpr float kRatioValues[9] = {0.5f, 1.f, 1.5f, 2.f, 3.f, 4.f, 5.f, 7.f, 8.f};

class VectorFM {
 public:
  enum Param {
    kScan = 0,   // Knob A
    kFm,         // Knob B
    kRatio,      // Edit 1
    kIndex,      // Edit 2
    kFold,       // Edit 3
    kMorph,      // Edit 4
    kDetune,     // Edit 5
    kSat,        // Edit 6
    kMove,       // Edit 7
    kMode,       // Edit 8
    kNumParams
  };

  enum Mode { kClassic = 0, kFeedback, kCross, kFoldFm, kNumModes };

  static constexpr int kNumRatios = 9;

  // switch rather than pointer tables: no data relocations in the unit
  static const char *ratioName(int i) {
    switch (i) {
      case 0: return "0.5";
      case 1: return "1";
      case 2: return "1.5";
      case 3: return "2";
      case 4: return "3";
      case 5: return "4";
      case 6: return "5";
      case 7: return "7";
      default: return "8";
    }
  }
  static const char *modeName(int i) {
    switch (i) {
      case 0: return "CLSC";
      case 1: return "FDBK";
      case 2: return "CROS";
      default: return "FOLD";
    }
  }

  void init() {
    // explicit stores (a loop here gets turned into a memset call)
    param_[kScan] = param_[kFm] = param_[kRatio] = param_[kIndex] = param_[kFold] = 0.f;
    param_[kMorph] = param_[kDetune] = param_[kSat] = param_[kMove] = param_[kMode] = 0.f;
    ratio_idx_ = 1;
    mode_ = kClassic;
    param_[kIndex] = 0.5f;
    w0_ = 440.f / 48000.f;
    lfo_ = 0.f;
    move_phase_ = 0.f;
    scan_z_ = depth_z_ = morph_z_ = fold_z_ = sat_z_ = 0.f;
    first_block_ = true;
    reset();
  }

  void reset() {
    ph_[0] = ph_[1] = 0.f;
    mph_[0] = mph_[1] = 0.f;
    fb_[0][0] = fb_[0][1] = fb_[1][0] = fb_[1][1] = 0.f;
    prev_[0] = prev_[1] = 0.f;
  }

  void noteOn() { reset(); }

  // w0: fundamental in cycles per sample
  void setPitch(float w0) { w0_ = w0; }
  // lfo: -1..1
  void setShapeLfo(float lfo) { lfo_ = lfo; }

  void setParam(int id, int32_t value) {
    switch (id) {
      case kScan:
      case kFm:
        param_[id] = value * (1.f / 1023.f);
        break;
      case kRatio:
        ratio_idx_ = clampi(value, 0, kNumRatios - 1);
        break;
      case kMode:
        mode_ = clampi(value, 0, kNumModes - 1);
        break;
      default:
        if (id > kFm && id < kNumParams) param_[id] = value * 0.01f;
        break;
    }
  }

  void process(float *out, uint32_t frames) {
    if (frames == 0) return;
    const float inv_frames = 1.f / frames;
    const float ratio = kRatioValues[ratio_idx_];
    const int mode = mode_;

    // --- Pitch / detune (osc 1 down, osc 2 up, up to +-25 cents) ---
    // 2^(cents/1200) via a short exp series: exact enough for tuning
    // (the SDK's fasterpow2f is ~3% off, which is half a semitone).
    const float dy = param_[kDetune] * 25.f * (0.6931472f / 1200.f);
    const float dr = 1.f + dy * (1.f + dy * (0.5f + dy * (1.f / 6.f)));
    float w1 = w0_ / dr;
    float w2 = w0_ * dr;
    if (mode == kCross) w2 *= ratio;  // in CROSS, osc 2 is the modulator
    w1 = clampf(w1, 0.f, 0.49f);
    w2 = clampf(w2, 0.f, 0.49f);
    const float mw1 = clampf(w1 * ratio, 0.f, 0.49f);
    const float mw2 = clampf(w2 * ratio, 0.f, 0.49f);

    // --- FM depth (in cycles) ---
    const float b = param_[kFm];
    const float b2 = b * b;  // finer control at low settings
    float depth_t;
    float fb_amt = 0.f;
    if (mode == kFeedback) {
      depth_t = b2 * 1.2f;
      fb_amt = param_[kIndex] * 0.25f;
    } else {
      depth_t = b2 * param_[kIndex] * 1.5f;
    }

    // --- Mip level selection, accounting for FM sidebands ---
    const float spread = 1.f + 6.2831853f * depth_t * (mode == kCross ? 1.f : ratio) + fb_amt * 8.f;
    const float lvl1 = mipLevel(w1 * spread);
    const float lvl2 = mipLevel((mode == kCross ? w2 : w2 * spread));
    int l1 = (int)lvl1;
    const float l1f = lvl1 - l1;
    int l2 = (int)lvl2;
    const float l2f = lvl2 - l2;

    // --- Movement LFO (0.05 - 2 Hz, depth up to half the scan range) ---
    const float mv = param_[kMove];
    float move_t = 0.f;
    if (mv > 0.f) {
      const float rate = 0.05f * fasterpow2f(mv * 5.321928f);  // 0.05 * 40^mv
      move_phase_ += rate * frames * (1.f / 48000.f);
      move_phase_ -= (int)move_phase_;
      move_t = sinTab(move_phase_) * mv * 0.5f;
    }

    // --- Parameter ramps for this block ---
    const float scan_t = param_[kScan] + lfo_ * 0.5f + move_t;
    const float morph_t = param_[kMorph];
    float fold_t = param_[kFold];
    const float sat_t = param_[kSat];
    if (first_block_) {
      scan_z_ = scan_t;
      depth_z_ = depth_t;
      morph_z_ = morph_t;
      fold_z_ = fold_t;
      sat_z_ = sat_t;
      first_block_ = false;
    }
    float scan = scan_z_, depth = depth_z_, morph = morph_z_, fold = fold_z_, sat = sat_z_;
    const float d_scan = (scan_t - scan) * inv_frames;
    const float d_depth = (depth_t - depth) * inv_frames;
    const float d_morph = (morph_t - morph) * inv_frames;
    const float d_fold = (fold_t - fold) * inv_frames;
    const float d_sat = (sat_t - sat) * inv_frames;

    // FOLD-FM mode drives the folder harder
    const float fold_drive_base = (mode == kFoldFm) ? 2.f : 1.f;
    const float fold_drive_range = (mode == kFoldFm) ? 6.f : 4.f;
    const float fold_min_wet = (mode == kFoldFm) ? 0.35f : 0.f;

    float ph1 = ph_[0], ph2 = ph_[1];
    float mph1 = mph_[0], mph2 = mph_[1];
    float p1 = prev_[0], p2 = prev_[1];

    for (uint32_t i = 0; i < frames; ++i) {
      scan += d_scan;
      depth += d_depth;
      morph += d_morph;
      fold += d_fold;
      sat += d_sat;

      const float s1 = reflect01(scan);
      const float s2 = reflect01(scan + 0.4f);

      // --- phase modulation offsets (in cycles) ---
      float pm1, pm2;
      switch (mode) {
        case kFeedback: {
          const float m1 = sinTab(mph1 + fb_amt * 0.5f * (fb_[0][0] + fb_[0][1]));
          const float m2 = sinTab(mph2 + fb_amt * 0.5f * (fb_[1][0] + fb_[1][1]));
          fb_[0][1] = fb_[0][0];
          fb_[0][0] = m1;
          fb_[1][1] = fb_[1][0];
          fb_[1][0] = m2;
          pm1 = depth * m1;
          pm2 = depth * m2;
        } break;
        case kCross:
          pm1 = depth * p2;          // osc 2 modulates osc 1
          pm2 = depth * 0.35f * p1;  // a little back the other way
          break;
        default:
          pm1 = depth * sinTab(mph1);
          pm2 = depth * sinTab(mph2);
          break;
      }

      const float o1 = scanRead(ph1 + pm1, s1, l1, l1f);
      const float o2 = scanRead(ph2 + pm2, s2, l2, l2f);
      p1 = o1;
      p2 = o2;

      float y = o1 + morph * (o2 - o1);

      // --- wavefold: blend towards a driven sine fold ---
      const float fw = fold_min_wet + (1.f - fold_min_wet) * fold;
      if (fw > 0.f) {
        const float g = fold_drive_base + fold * fold_drive_range;
        const float folded = fastersinfullf(1.5707963f * g * y);
        y += fw * (folded - y);
      }

      // --- soft saturation, gain compensated ---
      if (sat > 0.f) {
        const float drive = 1.f + sat * 7.f;
        const float s = softclip(drive * y) / softclip(drive);
        const float wet = clampf(sat * 5.f, 0.f, 1.f);
        y += wet * (s - y);
      }

      out[i] = limit(y);

      ph1 = wrap01(ph1 + w1);
      ph2 = wrap01(ph2 + w2);
      mph1 = wrap01(mph1 + mw1);
      mph2 = wrap01(mph2 + mw2);
    }

    ph_[0] = ph1;
    ph_[1] = ph2;
    mph_[0] = mph1;
    mph_[1] = mph2;
    prev_[0] = p1;
    prev_[1] = p2;
    scan_z_ = scan_t;
    depth_z_ = depth_t;
    morph_z_ = morph_t;
    fold_z_ = fold_t;
    sat_z_ = sat_t;
  }

 private:
  static inline int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }
  static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

  static inline float wrap01(float x) {
    x -= (int32_t)x;
    if (x < 0.f) x += 1.f;
    return x;
  }

  // Ping-pong a value into 0..1 so scan never sticks at the ends
  static inline float reflect01(float x) {
    if (x < 0.f) x = -x;
    const int32_t k = (int32_t)(x * 0.5f);
    x -= 2.f * k;
    if (x > 1.f) x = 2.f - x;
    return x;
  }

  // Rational tanh approximation, hard-limited at +-1
  static inline float softclip(float x) {
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
  }

  // Output limiter: transparent below 0.8, soft knee above, never exceeds 1
  static inline float limit(float x) {
    const float a = x < 0.f ? -x : x;
    if (a <= 0.8f) return x;
    const float s = 0.8f + 0.2f * softclip((a - 0.8f) * 5.f);
    return x < 0.f ? -s : s;
  }

  // Choose a fractional mip level for an (effective) frequency in cycles/sample
  static inline float mipLevel(float w) {
    // level 0 holds 128 harmonics; level n holds 128 >> n
    const float x = 128.f * w * (1.f / 0.45f);
    if (x <= 1.f) return 0.f;
    const float l = fasterlog2f(x);
    return clampf(l, 0.f, (float)(VFM_NUM_LEVELS - 1) - 0.001f);
  }

  static inline float readTable(const int16_t *t, float phase) {
    const float p = phase * VFM_TABLE_SIZE;
    const int32_t i = (int32_t)p;
    const float f = p - i;
    const float a = t[i];
    const float b = t[i + 1];
    return (a + f * (b - a)) * (1.f / 32767.f);
  }

  // Sine from the level-0 sine table, phase in cycles (any range)
  static inline float sinTab(float phase) {
    return readTable(vfm_tables[0], wrap01(phase));
  }

  // Read the scanning wavetable: interpolate between adjacent shapes and mip levels
  static inline float scanRead(float phase, float scan, int lvl, float lvl_frac) {
    phase = wrap01(phase);
    const float sp = scan * (VFM_NUM_SHAPES - 1);
    int si = (int)sp;
    if (si > VFM_NUM_SHAPES - 2) si = VFM_NUM_SHAPES - 2;
    const float sf = sp - si;
    const int16_t *a0 = vfm_tables[si * VFM_NUM_LEVELS + lvl];
    const int16_t *b0 = vfm_tables[(si + 1) * VFM_NUM_LEVELS + lvl];
    float va = readTable(a0, phase);
    float vb = readTable(b0, phase);
    if (lvl_frac > 0.001f && lvl < VFM_NUM_LEVELS - 1) {
      const float va1 = readTable(a0 + (VFM_TABLE_SIZE + 1), phase);
      const float vb1 = readTable(b0 + (VFM_TABLE_SIZE + 1), phase);
      va += lvl_frac * (va1 - va);
      vb += lvl_frac * (vb1 - vb);
    }
    return va + sf * (vb - va);
  }

  float param_[kNumParams];
  int ratio_idx_;
  int mode_;
  float w0_;
  float lfo_;
  float move_phase_;
  float scan_z_, depth_z_, morph_z_, fold_z_, sat_z_;
  bool first_block_;
  float ph_[2], mph_[2];
  float fb_[2][2];
  float prev_[2];
};

}  // namespace
