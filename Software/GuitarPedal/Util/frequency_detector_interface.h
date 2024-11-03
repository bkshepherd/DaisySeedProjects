#ifndef FREQUENCY_DETECTOR_INTERFACE_H
#define FREQUENCY_DETECTOR_INTERFACE_H
class FrequencyDetectorInterface {
 public:
  FrequencyDetectorInterface() {};
  virtual ~FrequencyDetectorInterface() {};
  virtual void Init(float sampleRate);
  virtual float Process(float in);
};
#endif