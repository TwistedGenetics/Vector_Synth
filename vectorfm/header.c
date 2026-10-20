/*
 *  File: header.c
 *
 *  VectorFM - NTS-1 mkII oscillator unit header
 */

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,      // TODO: replace with a registered developer ID before public release
    .unit_id = 0x1U,
    .version = 0x00010001U,  // 1.0.1
    .name = "VectorFM",
    .num_params = 10,
    .params = {
        // min, max, center, default, type, frac, frac mode, reserved, name
        // Knob A / Knob B
        {0, 1023, 0, 0, k_unit_param_type_none, 0, 0, 0, {"SCAN"}},
        {0, 1023, 0, 0, k_unit_param_type_none, 0, 0, 0, {"FM"}},
        // Edit 1-8
        {0, 8, 0, 1, k_unit_param_type_strings, 0, 0, 0, {"FM RATIO"}},
        {0, 100, 0, 50, k_unit_param_type_percent, 0, 0, 0, {"FM INDEX"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"WAVEFOLD"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"MORPH"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"SATURATE"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"MOVEMENT"}},
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MODE"}}},
};
