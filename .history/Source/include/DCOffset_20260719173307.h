#pragma once

class DCBlocker {
public:
  void reset() {
    x1 = 0.0;
    y1 = 0.0;
  }

  float process(float x) {
    const double y = x - x1 + R * y1; 
    x1 = x;
    y1 = y; 
    return static_cast<float>(y);
  }

private:
  static constexpr double R = 0.99995f; // 0.995~0.9999
  double x1 = 0.0;
  double y1 = 0.0;
};