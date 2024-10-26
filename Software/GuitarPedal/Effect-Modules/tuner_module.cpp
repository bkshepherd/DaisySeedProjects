#include "tuner_module.h"

#include <q/fx/lowpass.hpp>
#include <q/fx/signal_conditioner.hpp>
#include <q/pitch/pitch_detector.hpp>
#include <q/support/pitch_names.hpp>

using namespace bkshepherd;

using namespace daisy;
using namespace daisysp;
using namespace cycfi::q;

const char k_notes[12][3] = {"C",  "C#", "D",  "D#", "E",  "F",
                             "F#", "G",  "G#", "A",  "A#", "B"};

static const ParameterMetaData s_metaData[0] = {};

// Default Constructor
TunerModule::TunerModule() : BaseEffectModule() {
  // Setup the meta data reference for this Effect
  m_paramMetaData = s_metaData;

  // Initialize Parameters for this Effect
  this->InitParams(0);

  m_name = "TuneQ";
}

// Destructor
TunerModule::~TunerModule() {
  delete m_pitchDetector;
  m_pitchDetector = nullptr;

  delete m_preProcessor;
  m_preProcessor = nullptr;

  delete m_smoothingFilter;
  m_smoothingFilter = nullptr;
}

void TunerModule::Init(float sample_rate) {
  BaseEffectModule::Init(sample_rate);

  // The frequency detection bounds;
  frequency lowest_frequency = pitch_names::C[2];
  frequency highest_frequency = pitch_names::C[5];

  m_pitchDetector = new pitch_detector{lowest_frequency, highest_frequency,
                                       sample_rate, lin_to_db(0)};

  signal_conditioner::config preprocessor_config;
  m_preProcessor = new signal_conditioner{preprocessor_config, lowest_frequency,
                                          highest_frequency, sample_rate};

  m_smoothingFilter = new dynamic_smoother{20, sample_rate};
}

float Pitch(uint8_t note) { return 440.0f * pow(2.0f, (note - 'E') / 12.0f); }

float Cents(float frequency, uint8_t note) {
  return 1200.0f * log(frequency / Pitch(note)) / log(2.0f);
}

uint8_t Note(float frequency) {
  return round(12.0f * (log(frequency / 440.0f) / log(2.0f))) + 'E';
}

uint8_t Octave(float frequency) { return Note(frequency) / 12.0f - 1.0f; }

void TunerModule::ProcessMono(float in) {
  // Pre-process the signal for pitch detection
  float preProcessedSignal = m_preProcessor->operator()(in);

  // Send the processed sample through the pitch detector
  const bool ready = m_pitchDetector->operator()(preProcessedSignal);

  // If result is ready, get the detected frequency
  if (ready) {
    m_currentFrequency = m_pitchDetector->get_frequency();

    // Run a smoothing filter on the detected frequency
    m_currentFrequency = m_smoothingFilter->operator()(m_currentFrequency);
  }

  m_note = Note(m_currentFrequency);
  m_octave = Octave(m_currentFrequency);
  m_cents = Cents(m_currentFrequency, m_note);
}

void TunerModule::ProcessStereo(float inL, float inR) { ProcessMono(inL); }

void TunerModule::DrawUI(OneBitGraphicsDisplay& display, int currentIndex,
                         int numItemsTotal, Rectangle boundsToDrawIn,
                         bool isEditing) {
  char currentNote[12];
  sprintf(currentNote, "%s%u", k_notes[m_note % 12], m_octave);

  char strbuffNote[64];
  sprintf(strbuffNote, "%s", currentNote);
  display.WriteStringAligned(strbuffNote, Font_16x26, boundsToDrawIn,
                             Alignment::topCentered, true);

  // This has to be an odd number so the middle block is "in tune"
  const uint8_t blockCount = 15;
  const uint8_t inTuneBlockIndex = (blockCount - 1) / 2;
  const uint8_t numBlocksOutOfTune = (blockCount - 1) / 2;

  // Thresholds for tuning accuracy mapping to blocks
  const float closeThreshold = 1.0f;
  const float farLimit = 45.0f;

  // 0 is in tune, 1.0f is max out of tune we display, cents sign is used to
  // determine sharp or flat
  float percentage = std::abs(m_cents) / farLimit;
  percentage = std::clamp(percentage, 0.0f, 1.0f);

  const uint8_t numBlocksOutOfTuneToDisplay =
      static_cast<float>(numBlocksOutOfTune) * std::abs(percentage);

  int width = boundsToDrawIn.GetWidth();
  int blockWidth = (width / blockCount);
  int top = 30;

  bool blockActive[blockCount] = {false};

  // The center block is always active
  blockActive[inTuneBlockIndex] = true;

  // Counter used to set the blocks (gets decremented as we set them active)
  // We cap this to always show at least 1, and we will remove that 1 in the
  // next step if our tuning threshold is met
  uint8_t blockChangeCount =
      std::max(numBlocksOutOfTuneToDisplay, static_cast<uint8_t>(1));

  // Only show no blocks out of tune if we are within the close threshold
  if (std::abs(m_cents) < closeThreshold) {
    blockChangeCount = 0;
  }

  if (m_cents < 0) {
    // Set the flat blocks state
    for (int i = inTuneBlockIndex - 1; i >= 0; i--) {
      if (blockChangeCount > 0) {
        blockActive[i] = true;
        blockChangeCount--;
      } else {
        break;
      }
    }
  } else {
    // Set the sharp blocks state
    for (int i = inTuneBlockIndex + 1; i < blockCount; i++) {
      if (blockChangeCount > 0) {
        blockActive[i] = true;
        blockChangeCount--;
      } else {
        break;
      }
    }
  }

  // Display all of the blocks
  int x = 0;
  for (int block = 0; block < blockCount; block++) {
    Rectangle r(x, top, blockWidth, blockWidth);
    display.DrawRect(r, true, blockActive[block]);
    x += blockWidth;
  }

  char strbuffFreq[64];
  sprintf(strbuffFreq, FLT_FMT(2), FLT_VAR(2, m_currentFrequency));
  display.WriteStringAligned(strbuffFreq, Font_7x10, boundsToDrawIn,
                             Alignment::bottomCentered, true);
}