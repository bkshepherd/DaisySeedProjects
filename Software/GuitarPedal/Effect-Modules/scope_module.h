#pragma once
#ifndef SCOPE_MODULE_H
#define SCOPE_MODULE_H

#include "daisysp.h"
#include "base_effect_module.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file scope_module.h */

using namespace daisysp;

namespace bkshepherd
{

const uint16_t ScreenWidth = 128;

class ScopeModule : public BaseEffectModule
{
public:
  ScopeModule();
  ~ScopeModule();

  void Init(float sample_rate) override;
  void ProcessMono(float in) override;
  void ProcessStereo(float inL, float inR) override;
  void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing) override;

private:
  float m_scopeBuffer[ScreenWidth];
  uint16_t m_bufferIndex;
};
} // namespace bkshepherd
#endif
#endif
