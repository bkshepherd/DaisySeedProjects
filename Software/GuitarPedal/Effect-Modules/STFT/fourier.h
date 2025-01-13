// fourier.h
#ifndef FOURIER

#include "wave.h"

namespace soundmath
{
	template <typename T, size_t N> class Fourier
	{
	public:
		void (*processor)(const T* in, T* out);

		// in, middle, out need to be arrays of size (N * laps * 2)
		Fourier(void (*processor)(const T*, T*), ShyFFT<T, N, RotationPhasor>* fft, Wave<T>* window, size_t laps, T* in, T* middle, T* out) 
			: processor(processor), in(in), middle(middle), out(out), fft(fft), window(window), laps(laps), stride(N / laps)
		{
			writepoints = new int[laps * 2];
			readpoints = new int[laps * 2];

			memset(writepoints, 0, sizeof(int) * laps * 2);
			memset(readpoints, 0, sizeof(int) * laps * 2);

			for (int i = 0; i < 2 * (int)laps; i++) // initialize half of writepoints
				writepoints[i] = -i * (int)stride;

			reading = new bool[laps * 2];
			writing = new bool[laps * 2];

			memset(reading, false, sizeof(bool) * laps * 2);
			memset(writing, true, sizeof(bool) * laps * 2);
		}

		~Fourier()
		{
			delete [] writepoints;
			delete [] readpoints;
			delete [] reading;
			delete [] writing;
		}

		// writes a single sample (with windowing) into the in array
		void write(T x)
		{
			for (size_t i = 0; i < laps * 2; i++)
			{
				if (writing[i])
				{
					if (writepoints[i] >= 0)
					{
						T amp = (*window)((T)writepoints[i] / N);
						in[writepoints[i] + N * i] = amp * x;
					}
					writepoints[i]++;

					if (writepoints[i] == N)
					{
						writing[i] = false;
						reading[i] = true;
						readpoints[i] = 0;

						forward(i); // FTs ith in to ith middle buffer
						process(i); // user-defined; ought to move info from ith middle to out buffer
						backward(i); // IFTs ith out to ith in buffer

						current = i;
					}
				}
			}
		}

		inline void forward(const size_t i)
		{
			fft->Direct((in + i * N), (middle + i * N)); // analysis
			// arm_rfft_fast_f32(fft, in + i * N, middle + i * N, 0);
		}

		inline void backward(const size_t i)
		{
			fft->Inverse((out + i * N), (in + i * N)); // synthesis
			// arm_rfft_fast_f32(fft, out + i * N, in + i * N, 1);
		}

		// executes user-defined callback
		inline void process(const size_t i)
		{
			processor((middle + i * N), (out + i * N));
		}

		// read a single reconstructed sample
		T read()
		{
			T accum = 0;

			for (size_t i = 0; i < laps * 2; i++)
			{
				if (reading[i])
				{
					T amp = (*window)((T)readpoints[i] / N);
					accum += amp * in[readpoints[i] + N * i];

					readpoints[i]++;

					if (readpoints[i] == N)
					{
						writing[i] = true;
						reading[i] = false;
						writepoints[i] = 0;
					}
				}
			}

			accum /= N * laps / 2.0;
			return accum;
		}



	private:
		T *in, *middle, *out;

	public:
		ShyFFT<T, N, RotationPhasor>* fft;
		Wave<T>* window;

		size_t laps;
		size_t stride;

		int* writepoints;
		int* readpoints;
		bool* reading;
		bool* writing;

		int current = 0;
	};


	template <typename T, size_t N> class Analyzer
	{
	public:
		int (*processor)(const T* in);

		// in, middle, out need to be arrays of size (N * laps * 2)
		Analyzer(int (*processor)(const T*), ShyFFT<T, N, RotationPhasor>* fft, size_t laps, T* in, T* middle) 
			: processor(processor), in(in), middle(middle), fft(fft), laps(laps), stride(N / laps)
		{
			writepoints = new int[laps];

			memset(writepoints, 0, sizeof(int) * laps);

			for (int i = 0; i < (int)laps; i++) // initialize half of writepoints
				writepoints[i] = -i * (int)stride;

			writing = new bool[laps];
			memset(writing, true, sizeof(bool) * laps);
		}

		~Analyzer()
		{
			delete [] writepoints;
			delete [] writing;
		}

		// writes a single sample (with windowing) into the in array
		void write(T x)
		{
			for (size_t i = 0; i < laps; i++)
			{
				if (writing[i])
				{
					if (writepoints[i] >= 0)
					{
						// T window = halfhann((T)writepoints[i] / N);
						T window = hann((T)writepoints[i] / N);
						in[writepoints[i] + N * i] = window * x;
					}
					writepoints[i]++;

					if (writepoints[i] == N)
					{
						writing[i] = false;

						forward(i); // FTs ith in to ith middle buffer
						process(i); // user-defined; ought to move info from ith middle to out buffer
						current = i;

						size_t next = (i + 1) % (laps);
						writing[next] = true;
						writepoints[next] = 0;
					}
				}
			}
		}

		inline void forward(const size_t i)
		{
			fft->Direct((in + i * N), (middle + i * N)); // analysis
			// arm_rfft_fast_f32(fft, in + i * N, middle + i * N, 0);
		}

		// executes user-defined callback
		inline void process(const size_t i)
		{
			processor((middle + i * N));
		}

	private:
		T *in, *middle;

	public:
		ShyFFT<T, N, RotationPhasor>* fft;

		size_t laps;
		size_t stride;

		int* writepoints;
		bool* writing;

		int current = 0;
	};
}

#define FOURIER
#endif