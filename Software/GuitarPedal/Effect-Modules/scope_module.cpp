#include "scope_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

static const ParameterMetaData s_metaData[0] = {};

// Default Constructor
ScopeModule::ScopeModule() : BaseEffectModule()
{
  // Set the name of the effect
  m_name = "Scope";

  // Setup the meta data reference for this Effect
  m_paramMetaData = s_metaData;

  // Initialize Parameters for this Effect
  this->InitParams(0);
}

// Destructor
ScopeModule::~ScopeModule()
{
  // No Code Needed
}

void ScopeModule::Init(float sample_rate)
{
  BaseEffectModule::Init(sample_rate);

  m_bufferIndex = 0;
  for (uint16_t i = 0; i < ScreenWidth; i++) {
    m_scopeBuffer[i] = 0;
  }
}

void ScopeModule::ProcessMono(float in)
{
  BaseEffectModule::ProcessMono(in);

  m_scopeBuffer[m_bufferIndex] = in;
  if (++m_bufferIndex >= ScreenWidth)
    m_bufferIndex = 0;

  m_audioRight = m_audioLeft = in;
}

void ScopeModule::ProcessStereo(float inL, float inR)
{
  BaseEffectModule::ProcessStereo(inL, inR);

  m_scopeBuffer[m_bufferIndex] = (inL + inR) / 2.0f;
  if (++m_bufferIndex >= ScreenWidth)
    m_bufferIndex = 0;

  m_audioLeft = inL;
  m_audioRight = inR;
}

void ScopeModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing)
{
  BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);
  uint16_t center = boundsToDrawIn.GetHeight() / 2;

  for (uint16_t i = 0; i < ScreenWidth; i++) {
    display.DrawPixel(i, m_scopeBuffer[i] * center + center, true);
  }
}