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

void EnvelopeCanvas::mouseUp(const juce::MouseEvent &event) {
  isDragging = false;
  lastEditedIndex = -1;
}

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

void EnvelopeCanvas::clearEnvelope(float value) {
  envelopeData.fill(value); // 默认值为1 (无调制)
  repaint();
  sendChangeMessage();
}

void EnvelopeCanvas::resetToDefault() {
  // 默认平直包络 (值为1，表示无调制)
  envelopeData.fill(1.0f);
}

void EnvelopeCanvas::randomize() {
  juce::Random rng;

  // 随机选一种曲线模式
  int mode = rng.nextInt(5);

  // 随机生成几个控制点的频率和相位
  float freq1 = 1.0f + rng.nextFloat() * 3.0f;
  float freq2 = 2.0f + rng.nextFloat() * 5.0f;
  float freq3 = 0.5f + rng.nextFloat() * 2.0f;
  float phase1 = rng.nextFloat() * juce::MathConstants<float>::twoPi;
  float phase2 = rng.nextFloat() * juce::MathConstants<float>::twoPi;
  float phase3 = rng.nextFloat() * juce::MathConstants<float>::twoPi;
  float amp1 = 0.3f + rng.nextFloat() * 0.4f;
  float amp2 = 0.1f + rng.nextFloat() * 0.3f;
  float amp3 = 0.05f + rng.nextFloat() * 0.2f;

  constexpr int size = ENVELOPE_SIZE;
  const float mid = (yMin + yMax) * 0.5f;
  const float halfRange = (yMax - yMin) * 0.5f;

  for (int i = 0; i < size; ++i) {
    float t = static_cast<float>(i) / (size - 1); // 0 -> 1
    float val = 0.0f;

    switch (mode) {
    case 0: // 多谐波正弦叠加
      val = amp1 * std::sin(freq1 * juce::MathConstants<float>::twoPi * t + phase1) + amp2 * std::sin(freq2 * juce::MathConstants<float>::twoPi * t + phase2) +
            amp3 * std::sin(freq3 * juce::MathConstants<float>::twoPi * t + phase3);
      val = mid + val * halfRange;
      break;

    case 1: // ADSR 形状（攻击-衰减-延音-释放）
    {
      float attack = 0.05f + rng.nextFloat() * 0.2f;
      float decay = attack + 0.05f + rng.nextFloat() * 0.15f;
      float sustain = decay + 0.1f + rng.nextFloat() * 0.4f;
      float sustainLevel = 0.4f + rng.nextFloat() * 0.4f;
      if (t < attack)
        val = t / attack;
      else if (t < decay)
        val = 1.0f - (1.0f - sustainLevel) * (t - attack) / (decay - attack);
      else if (t < sustain)
        val = sustainLevel;
      else
        val = sustainLevel * (1.0f - (t - sustain) / (1.0f - sustain));
      val = yMin + val * (yMax - yMin);
    } break;

    case 2: // 锯齿波 + 抖动
      val = std::fmod(freq1 * t + phase1 / juce::MathConstants<float>::twoPi, 1.0f);
      val += 0.05f * (rng.nextFloat() - 0.5f);
      val = yMin + juce::jlimit(0.0f, 1.0f, val) * (yMax - yMin);
      break;

    case 3: // 指数衰减 + 周期性脉冲
    {
      float decay = 2.0f + rng.nextFloat() * 4.0f;
      float base = std::exp(-decay * t);
      float pulse = 0.3f * std::pow(std::max(0.0f, std::sin(freq2 * juce::MathConstants<float>::twoPi * t + phase2)), 8.0f);
      val = yMin + (base * 0.7f + pulse) * (yMax - yMin);
    } break;

    case 4: // 平滑随机游走（低频噪声）
    {
      // 先用粗网格随机点，再平滑
      constexpr int nodes = 16;
      int node = static_cast<int>(t * nodes);
      float nodeFrac = t * nodes - node;
      // 用哈希生成伪随机但确定的节点值
      auto hashVal = [&](int n) {
        n = (n ^ 0x12345678) * 0x9e3779b9;
        return 0.2f + 0.6f * static_cast<float>((n >> 8) & 0xFFFF) / 65535.0f;
      };
      float v0 = hashVal(node + static_cast<int>(phase1 * 100));
      float v1 = hashVal(node + 1 + static_cast<int>(phase1 * 100));
      float smooth = nodeFrac * nodeFrac * (3.0f - 2.0f * nodeFrac); // smoothstep
      val = yMin + (v0 + smooth * (v1 - v0)) * (yMax - yMin);
    } break;
    }

    envelopeData[i] = juce::jlimit(yMin, yMax, val);
  }

  repaint();
  sendChangeMessage();
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

  // 更细的线条
  g.strokePath(path, juce::PathStrokeType(0.1f));

  // 填充区域
  path.lineTo(static_cast<float>(getWidth()), static_cast<float>(getHeight()));
  path.lineTo(0.0f, static_cast<float>(getHeight()));
  path.closeSubPath();
  g.setColour(juce::Colours::cyan.withAlpha(0.15f));
  g.fillPath(path);

  // 绘制数据点圆点，方便精确定位
  // 根据画布宽度决定采样间隔，避免点太密
  int pointSpacing = std::max(1, size / getWidth());
  g.setColour(juce::Colours::white);
  for (int i = 0; i < size; i += pointSpacing) {
    float x = static_cast<float>(i) / (size - 1) * getWidth();
    float y = static_cast<float>(valueToY(envelopeData[i]));
    g.fillEllipse(x - 1.5f, y - 1.5f, 3.0f, 3.0f);
  }
}

void EnvelopeCanvas::updateEnvelopeFromMouse(const juce::MouseEvent &event) {
  float phase = xToPhase(event.x);
  float value = yToValue(event.y);

  // 将值限制在范围内
  value = juce::jlimit(yMin, yMax, value);

  constexpr int size = ENVELOPE_SIZE;
  int centerIndex = static_cast<int>(phase * (size - 1));
  centerIndex = juce::jlimit(0, size - 1, centerIndex);

  // 记录上一次编辑位置，用于插值填充中间点
  if (lastEditedIndex >= 0 && lastEditedIndex != centerIndex) {
    int start = std::min(lastEditedIndex, centerIndex);
    int end = std::max(lastEditedIndex, centerIndex);
    float startVal = envelopeData[start];
    float endVal = value;
    if (end - start > 1) {
      for (int i = start + 1; i < end; ++i) {
        float t = static_cast<float>(i - start) / static_cast<float>(end - start);
        envelopeData[i] = startVal + t * (endVal - startVal);
      }
    }
  }

  envelopeData[centerIndex] = value;
  lastEditedIndex = centerIndex;

  repaint();
  sendChangeMessage();
}
