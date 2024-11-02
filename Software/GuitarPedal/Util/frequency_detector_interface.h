#ifndef FREQUENCY_DETECTOR_INTERFACE_H
#define FREQUENCY_DETECTOR_INTERFACE_H
class FrequencyDetectorInterface {
 public:
  FrequencyDetectorInterface() {};
  virtual ~FrequencyDetectorInterface() {};
  virtual void Init(float sampleRate);
  virtual void Process(float in);
  virtual float GetFrequency() const;
};
#endif