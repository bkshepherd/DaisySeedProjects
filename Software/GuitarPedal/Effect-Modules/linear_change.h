#pragma once
#ifndef LINEAR_CHANGE_H
#define LINEAR_CHANGE_H

namespace bkshepherd {

class LinearChange {
  public:
    LinearChange() : active(false) {}
    
    void activate(float startValue, float endValue, int steps) {
        start = startValue;
        end = endValue;
        totalSteps = steps;
        currentStep = 0;
        active = true;
    }
    float getNextValue() {
        if (!active) {
            return end;
        }
        if (currentStep >= totalSteps) {
            active = false;
            return end;
        }
        float value = start + ((end - start) * ((float)currentStep / (float)totalSteps));
        currentStep++;
        return value;
    }

    bool isActive() const { return active; }
    void deactivate() { active = false; }

  private:
    float start;
    float end;
    int currentStep;
    int totalSteps;
    bool active;
};

} // namespace bkshepherd

#endif // LINEAR_CHANGE_H