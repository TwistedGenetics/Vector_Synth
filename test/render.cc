// Desktop test harness for VectorFM.
// Renders the DSP without the NTS-1 runtime, checks for NaN/clipping/DC,
// sweeps every parameter, and writes WAV demos to test/out/.
//
// Build & run:  ./test/run.sh

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include "synth.h"

static const int kSR = 48000;
static const int kBlock = 64;

static float noteW0(float note) { return 440.f * std::pow(2.f, (note - 69.f) / 12.f) / kSR; }

static void writeWav(const std::string &path, const std::vector<float> &x) {
  FILE *f = fopen(path.c_str(), "wb");
  if (!f) return;
  const uint32_t n = x.size(), data = n * 2, riff = 36 + data;
  const uint16_t pcm = 1, ch = 1, bits = 16, align = 2;
  const uint32_t sr = kSR, br = kSR * 2, fmt = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
  fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
  for (float v : x) {
    int16_t s = (int16_t)std::lrint(std::fmax(-1.f, std::fmin(1.f, v)) * 32767.f);
    fwrite(&s, 2, 1, f);
  }
  fclose(f);
}

struct Stats { float peak = 0, dc = 0; bool bad = false; };

static Stats stats(const std::vector<float> &x) {
  Stats s; double sum = 0;
  for (float v : x) {
    if (!std::isfinite(v)) s.bad = true;
    s.peak = std::fmax(s.peak, std::fabs(v));
    sum += v;
  }
  s.dc = sum / x.size();
  return s;
}

// Render `seconds` of audio, calling `automate(t)` once per block for param changes.
template <typename F>
static std::vector<float> render(VectorFM &v, float note, float seconds, F automate, float lfo_hz = 0.f) {
  std::vector<float> out(int(seconds * kSR));
  v.setPitch(noteW0(note));
  v.noteOn();
  for (size_t i = 0; i < out.size(); i += kBlock) {
    const float t = float(i) / kSR;
    automate(t / seconds);
    v.setShapeLfo(lfo_hz > 0.f ? std::sin(6.2831853f * lfo_hz * t) : 0.f);
    v.process(&out[i], std::min<size_t>(kBlock, out.size() - i));
  }
  return out;
}

// Rough aliasing measure: energy in DFT bins that are not near a harmonic.
static float inharmonicRatio(const std::vector<float> &x, float f0) {
  const int N = 8192;
  const int off = x.size() > (size_t)N ? (x.size() - N) / 2 : 0;
  double harm = 0, other = 0;
  for (int k = 1; k < N / 2; k += 1) {
    double re = 0, im = 0;
    const double w = 2 * M_PI * k / N;
    for (int n = 0; n < N; n += 1) {
      const double win = 0.5 - 0.5 * std::cos(2 * M_PI * n / N);
      re += x[off + n] * win * std::cos(w * n);
      im -= x[off + n] * win * std::sin(w * n);
    }
    const double p = re * re + im * im;
    const double f = double(k) * kSR / N;
    const double h = f / f0;
    if (std::fabs(h - std::round(h)) * f0 < 3.0 * kSR / N) harm += p; else other += p;
  }
  return float(other / (harm + other + 1e-20));
}

int main() {
  VectorFM v;
  int failures = 0;
  auto check = [&](const char *name, const std::vector<float> &x) {
    Stats s = stats(x);
    const bool ok = !s.bad && s.peak <= 1.0f && std::fabs(s.dc) < 0.05f && s.peak > 0.01f;
    printf("%-34s peak %.3f  dc %+.4f  %s\n", name, s.peak, s.dc, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
  };

  if (system("mkdir -p test/out") != 0) return 1;

  // 1. Scan sweep across all six shapes
  v.init();
  auto scan = render(v, 45, 4.f, [&](float p) { v.setParam(VectorFM::kScan, int(p * 1023)); });
  check("scan sweep", scan); writeWav("test/out/01_scan_sweep.wav", scan);

  // 2. FM intensity sweep at each ratio, classic mode
  for (int r = 0; r < VectorFM::kNumRatios; ++r) {
    v.init();
    v.setParam(VectorFM::kRatio, r);
    v.setParam(VectorFM::kIndex, 100);
    v.setParam(VectorFM::kScan, 400);
    auto x = render(v, 45, 1.f, [&](float p) { v.setParam(VectorFM::kFm, int(p * 1023)); });
    char name[64]; snprintf(name, sizeof name, "FM sweep ratio %s", VectorFM::ratioName(r));
    check(name, x);
    if (r == 3) writeWav("test/out/02_fm_sweep_ratio2.wav", x);
  }

  // 3. Each mode, full-on stress (max FM, fold, sat, detune, morph, movement) at low and high notes
  for (int m = 0; m < VectorFM::kNumModes; ++m) {
    for (float note : {24.f, 45.f, 72.f, 96.f, 120.f}) {
      v.init();
      v.setParam(VectorFM::kMode, m);
      v.setParam(VectorFM::kRatio, 4);
      for (int id = VectorFM::kScan; id <= VectorFM::kMove; ++id)
        if (id != VectorFM::kRatio) v.setParam(id, id <= VectorFM::kFm ? 1023 : 100);
      auto x = render(v, note, 0.5f, [](float) {}, 0.7f);
      char name[64]; snprintf(name, sizeof name, "stress %s note %d", VectorFM::modeName(m), int(note));
      check(name, x);
    }
  }

  // 4. Musical demo per mode: a bass note with FM + morph automation
  for (int m = 0; m < VectorFM::kNumModes; ++m) {
    v.init();
    v.setParam(VectorFM::kMode, m);
    v.setParam(VectorFM::kRatio, 3);
    v.setParam(VectorFM::kIndex, 60);
    v.setParam(VectorFM::kDetune, 40);
    v.setParam(VectorFM::kSat, 30);
    v.setParam(VectorFM::kMove, 30);
    v.setParam(VectorFM::kFold, m == VectorFM::kFoldFm ? 40 : 15);
    auto x = render(v, 36, 4.f, [&](float p) {
      v.setParam(VectorFM::kFm, int(512 + 511 * std::sin(6.2831853f * p)));
      v.setParam(VectorFM::kMorph, int(100 * p));
      v.setParam(VectorFM::kScan, int(1023 * p));
    }, 0.25f);
    char name[64]; snprintf(name, sizeof name, "demo %s", VectorFM::modeName(m));
    check(name, x);
    char path[64]; snprintf(path, sizeof path, "test/out/03_demo_%s.wav", VectorFM::modeName(m));
    writeWav(path, x);
  }

  // 5. Aliasing: saw (scan to SAW) at a high note with no FM should be mostly harmonic
  for (float note : {84.f, 96.f, 108.f}) {
    v.init();
    v.setParam(VectorFM::kScan, int(1023 * 2 / 5));  // SAW
    auto x = render(v, note, 0.5f, [](float) {});
    const float f0 = 440.f * std::pow(2.f, (note - 69.f) / 12.f);
    const float r = inharmonicRatio(x, f0);
    printf("alias saw note %-3d (%.0f Hz)        inharmonic energy %.5f  %s\n", int(note), f0, r, r < 0.01f ? "ok" : "FAIL");
    if (r >= 0.01f) ++failures;
  }

  // 6. Zipper check: sweeping morph should never step harder than either end does statically
  {
    auto maxStep = [](const std::vector<float> &x) {
      float m = 0;
      for (size_t i = 1; i < x.size(); ++i) m = std::fmax(m, std::fabs(x[i] - x[i - 1]));
      return m;
    };
    float ref = 0;
    for (int morph : {0, 100}) {
      v.init(); v.setParam(VectorFM::kMorph, morph);
      ref = std::fmax(ref, maxStep(render(v, 33, 0.5f, [](float) {})));
    }
    v.init();
    auto x = render(v, 33, 1.f, [&](float p) { v.setParam(VectorFM::kMorph, int(100 * p)); });
    const float d = maxStep(x);
    printf("morph sweep max step %.4f (static %.4f) %s\n", d, ref, d <= ref * 1.1f ? "ok" : "FAIL");
    if (d > ref * 1.1f) ++failures;
  }

  // 7. Tuning: A4 with everything off should be 440 Hz (count zero crossings over 1 s)
  {
    v.init();
    auto x = render(v, 69, 1.f, [](float) {});
    int zc = 0;
    for (size_t i = 1; i < x.size(); ++i) zc += (x[i - 1] < 0.f && x[i] >= 0.f);
    printf("tuning A4                          %d Hz  %s\n", zc, std::abs(zc - 440) <= 1 ? "ok" : "FAIL");
    if (std::abs(zc - 440) > 1) ++failures;
  }

  printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
