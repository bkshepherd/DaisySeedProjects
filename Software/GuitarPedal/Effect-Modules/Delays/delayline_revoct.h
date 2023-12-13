#pragma once
#ifndef DELAYLINE_REVOCT_H
#define DELAYLINE_REVOCT_H
#include <stdlib.h>
#include <stdint.h>
//namespace daisysp
//{
/** Simple Delay line.
November 2019

Converted to Template December 2019

declaration example: (1 second of floats)

DelayLine<float, SAMPLE_RATE> del;

By: shensley
*/
template <typename T, size_t max_size>
class DelayLineRevOct
{
  public:
    DelayLineRevOct() {}
    ~DelayLineRevOct() {}
    /** initializes the delay line by clearing the values within, and setting delay to 1 sample.
    */
    void Init() { Reset(); }
    /** clears buffer, sets write ptr to 0, and delay to 1 sample.
    */
    void Reset()
    {
        for(size_t i = 0; i < max_size; i++)
        {
            line_[i] = T(0);
        }
        write_ptr_ = 0;
        delay_     = 1;
        speed      = 1;
        delay_secondTap = 1;
        delay_float = 1.0;
        secondTapFraction = 0.0;
    }

    inline void setOctave(bool isOctave)
    {
        if (isOctave) {
            speed = 2;
        } else {
            speed = 1;
        }
    }

    // Sets the fractional length of the 2nd tap 
    //   2/3 (0.66667) = Triplett
    //   3/4 (0.75) = Dotted Eighth
    inline void set2ndTapFraction(float tapFraction)
    {
        secondTapFraction = tapFraction;

        // For secondTap (if this changes we want to recalculate the second tap based on current delay setting)
        int32_t int_delay_secondTap = static_cast<int32_t>(delay_float * secondTapFraction);
        frac_secondTap             = delay_float * secondTapFraction - static_cast<float>(int_delay_secondTap);
        delay_secondTap = static_cast<size_t>(int_delay_secondTap) < max_size ? int_delay_secondTap
                                                           : max_size - 1;
    }



    /** sets the delay time in samples
        If a float is passed in, a fractional component will be calculated for interpolating the delay line.
    */
    inline void SetDelay(size_t delay)
    {
        frac_  = 0.0f;
        delay_ = delay < max_size ? delay : max_size - 1;
    }

    /** sets the delay time in samples
        If a float is passed in, a fractional component will be calculated for interpolating the delay line.
    */
    inline void SetDelay(float delay)
    {
        int32_t int_delay = static_cast<int32_t>(delay);
        frac_             = delay - static_cast<float>(int_delay);
        delay_ = static_cast<size_t>(int_delay) < max_size ? int_delay
                                                           : max_size - 1;

        // For secondTap
        delay_float = delay;
        int32_t int_delay_secondTap = static_cast<int32_t>(delay_float * secondTapFraction);
        frac_secondTap             = delay_float * secondTapFraction - static_cast<float>(int_delay_secondTap);
        delay_secondTap = static_cast<size_t>(int_delay_secondTap) < max_size ? int_delay_secondTap
                                                           : max_size - 1;
    }

    /** writes the sample of type T to the delay line, and advances the write ptr
    */
    inline void Write(const T sample)
    {
        line_[write_ptr_] = sample;
        write_ptr_        = (write_ptr_ - 1 + max_size) % max_size;
    }

    /** returns the next sample of type T in the delay line, interpolated if necessary.
    */
    inline const T Read() const
    {
        T a = line_[(write_ptr_ * speed + delay_) % max_size];
        T b = line_[(write_ptr_ * speed + delay_ + 1) % max_size];
        return a + (b - a) * frac_;
    }

    inline const T ReadSecondTap() const
    {

        T c = line_[(write_ptr_ * speed + delay_ + delay_secondTap) % max_size];    // TODO IS pointer correct? was  "write_ptr_ * speed + delay_secondTap"
        T d = line_[(write_ptr_ * speed + delay_ + delay_secondTap  + 1) % max_size];
        return c + (d - c) * frac_secondTap;
    }

    /** Read from a set location */
    inline const T Read(float delay) const
    {
        int32_t delay_integral   = static_cast<int32_t>(delay);
        float   delay_fractional = delay - static_cast<float>(delay_integral);
        const T a = line_[(write_ptr_ + delay_integral) % max_size];
        const T b = line_[(write_ptr_ + delay_integral + 1) % max_size];
        return a + (b - a) * delay_fractional;
    }

    inline const T ReadHermite(float delay) const
    {
        int32_t delay_integral   = static_cast<int32_t>(delay);
        float   delay_fractional = delay - static_cast<float>(delay_integral);

        int32_t     t     = (write_ptr_ + delay_integral + max_size);
        const T     xm1   = line_[(t - 1) % max_size];
        const T     x0    = line_[(t) % max_size];
        const T     x1    = line_[(t + 1) % max_size];
        const T     x2    = line_[(t + 2) % max_size];
        const float c     = (x1 - xm1) * 0.5f;
        const float v     = x0 - x1;
        const float w     = c + v;
        const float a     = w + v + (x2 - x0) * 0.5f;
        const float b_neg = w + a;
        const float f     = delay_fractional;
        return (((a * f) - b_neg) * f + c) * f + x0;
    }

    inline const T Allpass(const T sample, size_t delay, const T coefficient)
    {
        T read  = line_[(write_ptr_ + delay) % max_size];
        T write = sample + coefficient * read;
        Write(write);
        return -write * coefficient + read;
    }

  private:
    float  frac_;
    size_t write_ptr_;
    size_t delay_;
    T      line_[max_size];
    int    speed;  // Either 1 or 2

    float  frac_secondTap;
    size_t delay_secondTap;
    float  delay_float; // Saves the last delay float setting for recalculating the second tap if needed
    float secondTapFraction;

};
//} // namespace daisysp
#endif
