#pragma once
/*
    model_data_nam_a2.h

    NAM A2 model collection for the A2Player runtime. Each model is a file of its own in models/ (the weights, the name
    shown for the MODEL parameter and the output gain); this file only defines the entry type and includes the generated
    models_list.inc, which lists every file in models/.

    All models must use the A2 Nano/Lite architecture: C=3 channels, 23 layers, 1871 weights (kA2WeightCount).

    To add a model:
      1. python3 nam_to_cpp_array.py "<model>.nam" "<name to show>" models/<name>.h
         (The models are listed in the MODEL choices in file-name order. Use letters, digits and _ in the name.)
      2. Rebuild (the Makefile runs update_models.sh, which regenerates models_list.inc; the new model starts at output gain 1.0).
      3. Optional: run Effect-Modules/Nam/update_models.sh by hand and answer y to set kOutputGain in all the models so they give the same level.
    To remove a model, delete its file. Models are chosen by their position, so adding or removing one before the others
    changes which model a saved preset refers to.
*/

#include "nam_a2_runtime.h" // for NAM_A2_MODEL_DATA, NAM_A2_ALIGN32, kA2WeightCount
#include <cstddef>

namespace nam_a2_models
{

struct NamA2ModelEntry
{
    const char*  name;
    const float* weights;
    // Linear output-gain multiplier for loudness matching across models.
    // Exporting raw A2 weights drops NAM's loudness-normalization metadata, so
    // models can differ in perceived level. 1.0f = unity; gain < 1.0f makes a hot
    // model quieter. Set by update_models.sh (the .nam loudness metadata
    // proved unreliable for level-matching across captures from different authors).
    float outputGain;
};

// The models (generated from the files in models/): the models' headers are included here, in the namespace, and kNamA2Models[] is defined.
#include "models_list.inc"

inline constexpr int kNamA2ModelCount = static_cast<int>(sizeof(kNamA2Models) / sizeof(kNamA2Models[0]));

} // namespace nam_a2_models
