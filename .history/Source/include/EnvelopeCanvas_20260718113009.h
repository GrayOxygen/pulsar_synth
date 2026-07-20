#pragma once

#include <array>
#include <atomic>
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * 可绘制的包络编辑器组件
 * - X轴：2048个点，对应pulsar phase (0-1)
 * - Y轴：频率调制百分比，范围可配置 (默认 0.1 - 10)
 */
class EnvelopeCanvas : public juce::Component, public juce::ChangeBroadcaster {
public:
  static constexpr int ENVELOPE_SIZE = 2048;

  EnvelopeCanvas();
  ~EnvelopeCanvas() override = default;

  void paint(juce::Graphics &g) override;
  void resized() override;

  // 鼠标交互绘制
  void mouseDown(const juce::MouseEvent &event) override;
  void mouseDrag(const juce::MouseEvent &event) override;
  void mouseUp(const juce::MouseEvent &event) override;

  // 包络数据访问
  void setEnvelopeData(const std::array<float, ENVELOPE_SIZE> &data);
 
  // Y轴范围设置
  void setYAxisRange(float minVal, float maxVal);
  std::pair<float, float> getYAxisRange() const { return {yMin, yMax}; }

  // 工具方法
  void clearEnvelope(float value = 1.0f);
  void resetToDefault();
  void randomize();

private:
  std::array<float, ENVELOPE_SIZE> envelopeData{};
  std::atomic<bool> isDragging{false};
  int lastEditedIndex = -1; // 记录上一次鼠标编辑的索引，用于插值填充中间点
  
  float yMin = 0.1f;  
  float yMax = 10.0f;

  // 坐标转换
  float xToPhase(int x) const;
  int phaseToX(float phase) const;
  float yToValue(int y) const;
  int valueToY(float value) const;

  // 绘制辅助
  void drawGrid(juce::Graphics &g);
  void drawEnvelope(juce::Graphics &g);
  void updateEnvelopeFromMouse(const juce::MouseEvent &event);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeCanvas)
};
