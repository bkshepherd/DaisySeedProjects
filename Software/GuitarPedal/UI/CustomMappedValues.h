#pragma once
#include <stdint.h>

#include "daisysp.h"
#include "daisy.h"

namespace daisy
{
	
/** @brief An override of `MappedValue` that maps a float value using various mapping functions, but gives the end user the flexibilty to change the step size.
 *  @author jelliesen, modified by achijos
 *  @addtogroup utility
 */
class MyMappedFloatValue : public MappedValue
{
  public:
    /** The availablke mapping functions */
    enum class Mapping
    {
        /** The value is mapped linearly between min and max. */
        lin,
        /** The value is mapped logarithmically. Note that the valid 
         *  values must be strictly larger than zero, so: min > 0, max > 0
         */
        log,
        /** The value is mapped with a square law */
        pow2
    };

    /** Creates a MyMappedFloatValue.
     * @param min           The lower end of the range of possible values
     * @param max           The upper end of the range of possible values
     * @param defaultValue  The default value
     * @param mapping       The `Mapping` to use. Note that for `Mapping::log`
     *                      `min`, `max` and ``defaultValue` must be > 0
     * @param unitStr       A string for the unit, e.g. "Hz"
     * @param numDecimals   Controls the number of decimals in `AppendToString()`
     * @param forceSign     Controls whether `AppendToString()` always prints the sign, 
     *                      even for positive numbers
    */
    MyMappedFloatValue(float       min,
                     float       max,
                     float       defaultValue,
                     Mapping     mapping       = Mapping::lin,
                     const char* unitStr       = "",
                     uint8_t     numDecimals   = 1,
                     bool        forceSign     = false,
				     float coarseStepSize0to1_ = 0.05f,
				     float fineStepSize0to1_   = 0.01f
					 );

    ~MyMappedFloatValue() override {}

    /** Returns the current value. */
    float Get() const { return value_; }

    /** Returns a const pointer to the current value. */
    const float* GetPtr() const { return &value_; }

    /** Sets the value, clamping it to the valid range. */
    void Set(float newValue);

    /** Returns the current value. */
    operator float() const { return value_; }

    /** Sets the value, clamping it to the valid range. */
    MyMappedFloatValue& operator=(float val)
    {
        Set(val);
        return *this;
    }

    // inherited form MappedValue
    void AppentToString(FixedCapStrBase<char>& string) const override;

    // inherited form MappedValue
    void ResetToDefault() override;

    // inherited form MappedValue
    float GetAs0to1() const override;

    // inherited form MappedValue
    void SetFrom0to1(float normalizedValue0to1) override;

    /** Steps the 0..1 normalized representation of the value up or down
     *  in 1% or 5% steps.
     */
    void Step(int16_t numStepsUp, bool useCoarseStepSize) override;
	
	void SetCoarseStepSize(float f) { coarseStepSize0to1_ = f;}
	
	void SetFineStepSize(float f) { fineStepSize0to1_ = f;}

  private:
    float                  value_;
    const float            min_;
    const float            max_;
    const float            default_;
    Mapping                mapping_;
    const char*            unitStr_;
    const uint8_t          numDecimals_;
    const bool             forceSign_;
    float coarseStepSize0to1_ = 0.05f;
    float fineStepSize0to1_   = 0.01f;
};

}
