#include "../include/EnvelopeCanvas.h"

EnvelopeCanvas::EnvelopeCanvas() { resetToDefault(); }

void EnvelopeCanvas::paint(juce::Graphics &g) {
  g.fillAll(juce::Colours::darkgrey.darker(0.3f));

  drawGrid(g);
  drawEnvelope(g);
}

void EnvelopeCanvas::resized() { repaint(); }

void EnvelopeCanvas::mouseDown(const juce::MouseEvent &event) {
  isDragging = true;
  updateEnvelopeFromMouse(event);
}

void EnvelopeCanvas::mouseDrag(const juce::MouseEvent &event) {
  if (isDragging) {
    updateEnvelopeFromMouse(event);
  }
}

void EnvelopeCanvas::mouseUp(const juce::MouseEvent &event) { isDragging = false; }

void EnvelopeCanvas::setEnvelopeData(const std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> &data) {
  envelopeData = data;
  repaint();
  sendChangeMessage();
}

std::array<float, EnvelopeCanvas::ENVELOPE_SIZE> EnvelopeCanvas::getEnvelopeData() const { return envelopeData; }

float EnvelopeCanvas::getEnvelopeValueAtPhase(float phase) const {
  // phase: 0.0 - 1.0, clamp and wrap
  phase = std::fmod(std::abs(phase), 1.0f);

  constexpr int size = ENVELOPE_SIZE;
  float indexF = phase * (size - 1);
  int index = static_cast<int>(indexF);
  float frac = indexF - index;

  // 线性插值
  float val1 = envelopeData[std::min(index, size - 1)];
  float val2 = envelopeData[std::min(index + 1, size - 1)];

  return val1 + frac * (val2 - val1);
}

void EnvelopeCanvas::setYAxisRange(float minVal, float maxVal) {
  yMin = std::min(minVal, maxVal);
  yMax = std::max(minVal, maxVal);
  repaint();
}

void EnvelopeCanvas::clearEnvelope() {
  envelopeData.fill(1.0f); // 默认值为1 (无调制)
  repaint();
  sendChangeMessage();
}

void EnvelopeCanvas::resetToDefault() {
  // 默认平直包络 (值为1，表示无调制)
  envelopeData.fill(1.0f);
}

// 坐标转换
float EnvelopeCanvas::xToPhase(int x) const {
  float width = static_cast<float>(getWidth());
  return juce::jlimit(0.0f, 1.0f, static_cast<float>(x) / width);
}

int EnvelopeCanvas::phaseToX(float phase) const { return static_cast<int>(phase * getWidth()); }

float EnvelopeCanvas::yToValue(int y) const {
  float height = static_cast<float>(getHeight());
  float normY = 1.0f - juce::jlimit(0.0f, 1.0f, static_cast<float>(y) / height);
  return yMin + normY * (yMax - yMin);
}

int EnvelopeCanvas::valueToY(float value) const {
  float normVal = (value - yMin) / (yMax - yMin);
  return static_cast<int>((1.0f - normVal) * getHeight());
}

void EnvelopeCanvas::drawGrid(juce::Graphics &g) {
  g.setColour(juce::Colours::grey.withAlpha(0.3f));

  constexpr int size = ENVELOPE_SIZE;
  // 垂直线 (每256个点一条)
  for (int i = 0; i < size; i += 256) {
    float x = static_cast<float>(i) / size * getWidth();
    g.drawVerticalLine(static_cast<int>(x), 0.0f, static_cast<float>(getHeight()));
  }

  // 水平线 (中间)
  int midY = valueToY((yMin + yMax) / 2.0f);
  g.drawHorizontalLine(midY, 0.0f, static_cast<float>(getWidth()));

  // 画边框
  g.drawRect(getLocalBounds().toFloat(), 1.0f);
}

void EnvelopeCanvas::drawEnvelope(juce::Graphics &g) {
  g.setColour(juce::Colours::cyan);

  juce::Path path;
  bool first = true;

  constexpr int size = ENVELOPE_SIZE;
  for (int i = 0; i < size; ++i) {
    float x = static_cast<float>(i) / (size - 1) * getWidth();
    float y = static_cast<float>(valueToY(envelopeData[i]));

    if (first) {
      path.startNewSubPath(x, y);
      first = false;
    } else {
      path.lineTo(x, y);
    }
  }

  g.strokePath(path, juce::PathStrokeType(2.0f));

  // 填充区域
  path.lineTo(static_cast<float>(getWidth()), static_cast<float>(getHeight()));
  path.lineTo(0.0f, static_cast<float>(getHeight()));
  path.closeSubPath();
  g.setColour(juce::Colours::cyan.withAlpha(0.2f));
  g.fillPath(path);
}

void EnvelopeCanvas::updateEnvelopeFromMouse(const juce::MouseEvent &event) {
  float phase = xToPhase(event.x);
  float value = yToValue(event.y);

  // 将值限制在范围内
  value = juce::jlimit(yMin, yMax, value);

  constexpr int size = ENVELOPE_SIZE;
  // 计算影响的索引范围（简单的圆形笔刷）
  int centerIndex = static_cast<int>(phase * (size - 1));
  int brushRadius = 10; // 影响半径

  for (int i = -brushRadius; i <= brushRadius; ++i) {
    int idx = centerIndex + i;
    if (idx >= 0 && idx < size) {
      // 距离衰减
      float dist = std::abs(i) / static_cast<float>(brushRadius);
      float influence = 1.0f - dist * dist; // 二次衰减

      // 混合当前值和新值
      envelopeData[idx] = envelopeData[idx] * (1.0f - influence) + value * influence;
    }
  }

  repaint();
  sendChangeMessage();
}
