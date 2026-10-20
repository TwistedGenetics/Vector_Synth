/*
 *  File: header.c
 *
 *  VectorFM - microKORG 2 oscillator unit header
 *  Parameters keep the same order as the NTS-1 mkII version.
 */

#include "unit.h"
#include "runtime.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x5477476EU,  // 'TwGn' (TwistedGenetics)
    .unit_id = 0x1U,
    .version = 0x00010100U,  // 1.1.0
    .name = "VectorFM",
    .num_presets = 0,
    .num_params = 13,
    .params = {
        // min, max, center, default, type, frac, frac mode, reserved, name

        // Page 1
        {0, 1023, 0, 0, k_unit_param_type_none, 0, 0, 0, {"Scan"}},
        {0, 1023, 0, 0, k_unit_param_type_none, 0, 0, 0, {"FM"}},
        {0, 8, 0, 1, k_unit_param_type_strings, 0, 0, 0, {"FM Ratio"}},

        // Page 2
        {0, 100, 0, 50, k_unit_param_type_percent, 0, 0, 0, {"FM Index"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"Wavefold"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"Morph"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"Detune"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"Saturate"}},

        // Page 3
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"Movement"}},
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"Mode"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}}};
