//
// Created by Mr. Wang on 2025/4/20.
//
#include "../include/Commons.h"

namespace why {
// 1: 1 beat , 16: 1/16 * 1 beat, etc
std::atomic<int> beatDivision(1);

std::atomic<float> bpm(120);
std::atomic<double> sampleRate(96000.0);
std::atomic<double> blockSize(512);

// id from 1(=item id in combobox), The display order is sorted by id
std::map<int, juce::String> resourceIdToName = {
    {1, "11L-hey.wav"},
    {2, "11L-tea in chineses.wav"},
    {3, "18366 water drop.wav"},
    {4, "387982 car door.wav"},
    {5, "388005 battle swish.wav"},
    {6, "388055 sax.wav"},
    {7, "388534 zigzag.wav"},
    {8, "388604 machine motion2.wav"},
    {9, "388607 machine motion.wav"},
    {10, "388679 ding.wav"},
    {11, "389036 noisy scratch.wav"},
    {12, "389185 metal drop.wav"},
    {13, "389662 flip book.wav"},
    {14, "389997 drop water.wav"},
    {15, "390150 reverse.wav"},
    {16, "390667 bubble.wav"},
    {17, "391154 cough.wav"},
    {18, "391245 finger dot.wav"},
    {19, "391281 crack.wav"},
    {20, "391468 fart.wav"},
    {21, "391508 sweep spiral.wav"},
    {22, "391520 frog.wav"},
    {23, "391547 girl laugh.wav"},
    {24, "391962 machine.wav"},
    {25, "392026 bark2.wav"},
    {26, "392029 donkey.wav"},
    {27, "393363 hey.wav"},
    {28, "393659 noisy hit.wav"},
    {29, "394131 piano.wav"},
    {30, "394317 phone ring.wav"},
    {31, "394433 whoosh.wav"},
    {32, "394889 knock door.wav"},
    {33, "395166 violin.wav"},
    {34, "395283 small wood block.wav"},
    {35, "395502 bell vibra.wav"},
    {36, "396286 bell ding.wav"},
    {37, "396287 bell.wav"},
    {38, "396893 footstep.wav"},
    {39, "397471 sweep.wav"},
    {40, "398885 bark.wav"},
    {41, "398921 squeak.wav"},
    {42, "398990 scrape.wav"},
    {43, "399003 bottle cap.wav"},
    {44, "399523 string.wav"},
    {45, "462035 chime.wav"},
    {46, "462069 storm.wav"},
    {47, "462241 sword.wav"},
    {48, "462345 elec guitar.wav"},
    {49, "462362 crowd clap.wav"},
    {50, "487463 trumpet low.wav"},
    {51, "487464 trumpet hight.wav"},
    {52, "545951 battle.wav"},
    {53, "546085 thanks.wav"},
    {54, "546199 background.wav"},
    {55, "546257 hiccup.wav"},
    {56, "576648 crash.wav"},
    {57, "576707 snare.wav"},
    {58, "576753 tom.wav"},
    {59, "576961 snap.wav"},
    {60, "577050 swish.wav"},
    {61, "577442 paper.wav"},
    {62, "577443 zip.wav"},
    {63, "577454 wood.wav"},
    {64, "I love you in chinese.wav"},
};
 

juce::StringArray getImpulseSwitchArray() {
  juce::StringArray names = {"Off", "Template", "Sample Source"};
  return names;
}

juce::StringArray getMaskOptionArray() {
  juce::StringArray names = {"Off", "BurstMask", "EuclidMask", "StochasticMask"};
  return names;
}

std::string generateEuclidRhythm(int steps, int hits) {
  if (hits <= 0) {
    return "0";
  }
  if (steps <= hits) {
    return "1";
  }
  int base = steps / hits;
  std::vector<int> group = {1};
  for (int j = 0; j < base - 1; ++j) {
    group.push_back(0);
  }

  // vector contains 1
  std::vector<std::vector<int>> ones(hits, group);
  // vector contains 0，remainders
  std::vector<std::vector<int>> remainders(steps - (steps / hits) * hits, std::vector<int>{0});

  size_t i = 0;
  while (remainders.size() > 1) {
    size_t count = std::min(ones.size(), remainders.size());

    // 把 remainders[i] 中的元素逐个插入到 ones[i] 中
    for (size_t j = 0; j < count; ++j) {
      ones[j].insert(ones[j].end(), remainders[j].begin(), remainders[j].end());
    }

    // 重新分配zeros
    remainders.erase(remainders.begin(), remainders.begin() + count);
    if (count < ones.size()) {
      remainders.insert(remainders.end(), ones.begin() + count, ones.end());
      ones.erase(ones.begin() + count, ones.end());
    }
  }

  // 最终合并（flatten）所有子序列
  std::vector<std::vector<int>> total;
  total.insert(total.end(), ones.begin(), ones.end());
  total.insert(total.end(), remainders.begin(), remainders.end());

  std::vector<int> result;
  for (size_t i = 0; i < total.size(); i++) {
    result.insert(result.end(), total[i].begin(), total[i].end());
  }

  std::ostringstream oss;
  for (size_t i = 0; i < result.size(); ++i) {
    oss << result[i];
    //            if (i != result.size() - 1) {
    //                oss << "";
    //            }
  }
  return oss.str();
}

void setRandomIndexToOne(std::string &binaryString) {
  if (binaryString.empty())
    return;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, binaryString.size() - 1);

  int randomIndex = dis(gen);
  binaryString[randomIndex] = '1';
}

std::string generateBinaryString(int length) {
  if (length <= 0) {
    return "";
  }

  std::string result;
  result.reserve(length);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 1);
  bool existOne = false;
  for (int i = 0; i < length; ++i) {
    char temp = dis(gen) ? '1' : '0';
    result += temp;
    if (temp == '1') {
      existOne = true;
    }
  }
  if (!existOne) {
    setRandomIndexToOne(result);
  }
  return result;
}

PerlinNoise::PerlinNoise() {
  int p[256] = {151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190, 6,   148, 247, 120, 234, 75,  0,
                26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 9600,  87,  174, 20,  125, 136, 171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146,
                158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,
                169, 200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227,
                47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253, 19,  98,  108, 110, 79,
                113, 224, 232, 178, 185, 112, 104, 218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199,
                106, 157, 184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,  222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180};
  for (int i = 0; i < 256; ++i) {
    permutation[i] = p[i];
    permutation[256 + i] = p[i];
  }
}

float PerlinNoise::fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

float PerlinNoise::lerp(float t, float a, float b) { return a + t * (b - a); }

float PerlinNoise::grad(int hash, float x) {
  int h = hash & 15;
  float grad = 1.0f + (h & 7); // Gradient value 1..8
  if ((h & 8) != 0)
    grad = -grad; // Set random sign
  return grad * x;
}

float PerlinNoise::noise(float x) const {
  int X = static_cast<int>(std::floor(x)) & 255; // 整数
  float xf = x - std::floor(x);                  // 小数
  float u = fade(xf);
  return lerp(u, grad(permutation[X], xf), grad(permutation[X + 1], xf - 1.0f)) * 2.0f;
}
} // namespace why
