// wave.h // interpolated lookup table
#ifndef WAVE

#include <functional>

namespace soundmath
{
	const int TABSIZE = 2048;

	template <typename T> class Wave
	{
	public:
		Wave() { }
		~Wave() { }

		Wave(std::function<T(T)> shape, T left = 0, T right = 1, bool periodic = true)
		{
			this->shape = shape;
			this->left = left;
			this->right = right;
			this->periodic = periodic;

			for (int i = 0; i < TABSIZE; i++)
			{
				T phase = (T) i / TABSIZE;
				table[i] = shape((1 - phase) * left + phase * right);
			}

			this->endpoint = shape(right);
		}

	#ifdef FUNCTIONAL 
		T lookup(T input)
		{
			return shape(input);
		}
	#else
		T lookup(T input)
		{
			T phase = (input - left) / (right - left);
			
			// get value at endpoint if input is out of bounds
			if (!periodic && (phase < 0 || phase >= 1))
			{
				if (phase < 0)
					return none(0);
				else
					return endpoint;
			}
			else
			{
				phase += 1;
				phase -= int(phase);

				int center = (int)(phase * TABSIZE) % TABSIZE;
				int after = (center + 1) % TABSIZE;

				T disp = (phase * TABSIZE - center);
				disp -= int(disp);

				return linear(center, after, disp);
			}
		}
	#endif

		T operator()(T phase)
		{
			return lookup(phase);
		}

	protected:
		T table[TABSIZE];

	private:
		T left; // input phases are interpreted as lying in [left, right)
		T right;
		bool periodic;

		T endpoint; // if (this->periodic == false), provides a value for (*this)(right)

		std::function<T(T)> shape;

		T none(int center)
		{
			return table[center];
		}

		T linear(int center, int after, T disp)
		{
			return table[center] * (1 - disp) + table[after] * disp;
		}

	};
}

#define WAVE
#endif