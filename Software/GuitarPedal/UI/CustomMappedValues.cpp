#include "CustomMappedValues.h"
#include <cmath>
#include <cstring>

namespace daisy
{
// ==========================================================================

// ==========================================================================

MyMappedFloatValue::MyMappedFloatValue(float       min,
                                   float       max,
                                   float       defaultValue,
                                   Mapping     mapping,
                                   const char* unitStr,
                                   uint8_t     numDecimals,
                                   bool        forceSign, 
								   float coarseStepSize0to1_,
				                   float fineStepSize0to1_    )
: value_(defaultValue),
  min_(min),
  max_(max),
  default_(defaultValue),
  mapping_(mapping),
  unitStr_(unitStr),
  numDecimals_(numDecimals),
  forceSign_(forceSign),
  coarseStepSize0to1_(coarseStepSize0to1_),
  fineStepSize0to1_(fineStepSize0to1_)   
{
}

void MyMappedFloatValue::Set(float newValue)
{
    value_ = std::max(min_, std::min(max_, newValue));
}

void MyMappedFloatValue::AppentToString(FixedCapStrBase<char>& string) const
{
    string.AppendFloat(value_, numDecimals_, false, forceSign_);
    string.Append(unitStr_);
}

void MyMappedFloatValue::ResetToDefault()
{
    value_ = default_;
}

float MyMappedFloatValue::GetAs0to1() const
{
    switch(mapping_)
    {
        case Mapping::lin: return (value_ - min_) / (max_ - min_);
        case Mapping::log:
        {
            const float a = 1.0f / (log10f(max_ / min_));
            return std::max(0.0f, std::min(1.0f, a * log10f(value_ / min_)));
        }
        case Mapping::pow2:
        {
            const float valueSq = (value_ - min_) / (max_ - min_);
            return std::max(0.0f, std::min(1.0f, sqrtf(valueSq)));
        }
        default: return 0.0f;
    }
}

void MyMappedFloatValue::SetFrom0to1(float normalizedValue0to1)
{
    float v;
    switch(mapping_)
    {
        case Mapping::lin:
            v = normalizedValue0to1 * (max_ - min_) + min_;
            break;
        case Mapping::log:
        {
            const float a = 1.0f / log10f(max_ / min_);
            v             = min_ * powf(10, normalizedValue0to1 / a);
        }
        break;
        case Mapping::pow2:
        {
            const float valueSq = normalizedValue0to1 * normalizedValue0to1;
            v                   = min_ + valueSq * (max_ - min_);
        }
        break;
        default: value_ = 0.0f; return;
    }
    value_ = std::max(min_, std::min(max_, v));
}

void MyMappedFloatValue::Step(int16_t numSteps, bool useCoarseStepSize)
{
    const auto mapped = GetAs0to1();
    const auto step
        = numSteps
          * (useCoarseStepSize ? coarseStepSize0to1_ : fineStepSize0to1_);
    SetFrom0to1(std::max(0.0f, std::min(mapped + step, 1.0f)));
}

} // namespace daisy
