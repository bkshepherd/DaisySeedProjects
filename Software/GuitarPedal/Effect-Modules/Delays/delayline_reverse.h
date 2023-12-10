// Copyright 2021 Adam Fulford
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.

#pragma once
#ifndef DELAY_REVERSE_H
#define DELAY_REVERSE_H
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
//namespace daisysp
//{
/** Reverse Delay line
By: Adam Fulford
*/
template <typename T, size_t max_size>
class DelayLineReverse
{
  public:
    DelayLineReverse() {}
    ~DelayLineReverse() {}
    /** initializes the delay line by clearing the values within, and setting delay to min time.
    */
    void Init() 
    { 
        Reset(); 
    }
    /** clears buffer, sets write ptr to 0, and delay to 1 sample.
    */
    void Reset()
    {

        delay1_  = 2400; //min Reverse delay time 
        fadetime = 2300;    //in samples
        
        for(size_t i = 0; i < max_size; i++)
        {
            line_[i] = T(0);
        }
        write_ptr_ = 0;
        read_ptr1_ = 0;
        read_ptr2_ = 0;
        headDiff_ = 0;
        playinghead_ = false;
        fadepos_ = 0.0f;
        fading_ = false;
        indexMultiplier_ = 1;
    }

    /** sets the delay time in samples
    */
    inline void SetDelay1(size_t delay)
    {
        frac1_  = 0.0f;
        delay1_ = delay < max_size ? delay : max_size - 1;
    }

    /** sets the delay time in samples
        If a float is passed in, a fractional component will be calculated for interpolating the delay line.
    */
    inline void SetDelay1(float delay)
    {
        int32_t int_delay = static_cast<int32_t>(delay);
        frac1_             = delay - static_cast<float>(int_delay);
        delay1_ = static_cast<size_t>(int_delay) < max_size ? int_delay
                                                           : max_size - 1;
    }


    inline void SetSpeed(unsigned int speed)
    {
        indexMultiplier_ = speed;
    }

    /** writes the sample of type T to the delay line, and advances the write ptr
    */
    inline void Write(const T sample)
    {
        line_[write_ptr_] = sample;
        //advance write ptr in forward direction
        write_ptr_        = (write_ptr_ + 1 + max_size) % max_size; //increment forwards
        
        //increment head difference
        headDiff_        = (headDiff_ + 1 + delay1_) % delay1_;   

        //advance read ptrs in reverse direction
        read_ptr1_ = (read_ptr1_ - 1  + max_size) % max_size;   // KAB TODO I think this is where I would multipy the speed for reverse octave --NOPE
        read_ptr2_ = (read_ptr2_ - 1  + max_size) % max_size;   // KAB TODO I think this is where I would multipy the speed for reverse octave --NOPE high pitch error sounding noise
 
        if (headDiff_ > (delay1_ - fadetime - 1))  //start cross fade region
        {
            if(!fading_)
            {
                fading_ = true; //start fading
                
                if(!playinghead_) 
                {
                    //jump ptr2 to fadetime beyond write position
                    read_ptr2_ = write_ptr_ - 1;
                }
    
                else
                {
                    //jump ptr1 to fadetime beyond write position
                    read_ptr1_ = write_ptr_ - 1; 
                }
            }

            else
            {
                //continue fading
            }
        }

        if(fading_)
        {
            if(!playinghead_)   //head1 playing
            {
                fadepos_ = fadepos_ + (1.0f / (fadetime) ); //increment up to 1.
                if (fadepos_ > 1.0f)
                {
                    fadepos_ = 1.0f;
                    fading_ = false;    //stop fading
                    playinghead_ = true;
                }
            }

            else    //head2 playing
            {
                fadepos_ = fadepos_ - (1.0f / (fadetime) ); //increment down to 0
                if (fadepos_ < 0.0f)
                {
                    fadepos_ = 0.0f;
                    fading_ = false;    //stop fading
                    playinghead_ = false;
                }
            }
        }
        else    //not fading
        {
        }

        
    }

    /** returns the next sample of type T in the delay line, interpolated if necessary.
    */
    inline const T ReadRev() const
    {
        T a1 = line_[read_ptr1_];
        T a2 = line_[(read_ptr2_)];

        //T a1 = line_[read_ptr1_];
        //T a2 = line_[(read_ptr2_)];

        float read1 = a1;
        float read2 = a2;

        float scalar_1, scalar_2;

        //hann                                                 // TODO Try this and see if it sounds different, this is less computationally expensive - sounds fine to me!
        scalar_1 = sinf(fadepos_ * ((float)M_PI * 0.5f));
        scalar_2 = sinf((1.0f - fadepos_) * ((float)M_PI * 0.5f));

        //flattenned hann
        //scalar_1 = 2.0f*(   (9.0f/16.0f)*sinf(float(M_PI)*(1.0f/2.0f)*fadepos_)  +   (1.0f/16.0f)*sinf(float(M_PI)*(3.0f/2.0f)*fadepos_)   );
        //scalar_2 = 2.0f*(   (9.0f/16.0f)*sinf(float(M_PI)*(1.0f/2.0f)*(1.0f - fadepos_))  +   (1.0f/16.0f)*sinf(float(M_PI)*(3.0f/2.0f)*(1.0f - fadepos_))   );

        return (read2 * scalar_1) + (read1 * scalar_2); 
    }






    /** returns the next sample of type T in the delay line, interpolated if necessary.
    */
    inline const T ReadFwd() const  //read forward as feedback signal
    {
        T a = line_[(write_ptr_*indexMultiplier_ + delay1_) % max_size];
        T b = line_[(write_ptr_*indexMultiplier_ + delay1_ + 1) % max_size];   
        return a + (b - a) * frac1_;
    }




  private:
    float  frac1_;
    size_t write_ptr_;
    size_t read_ptr1_;
    size_t read_ptr2_;
    size_t delay1_;
    size_t headDiff_;
    T      line_[max_size];
    size_t fadetime;
    bool playinghead_;
    float fadepos_;
    bool fading_;
    unsigned int indexMultiplier_;
    
};
//} // namespace daisysp
#endif
