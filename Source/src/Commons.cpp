//
// Created by Mr. Wang on 2025/4/20.
//
#include "Commons.h"

namespace why
{
    std::atomic<float> bpm(120);
    // std::atomic<double> sampleRate(44100.0);

    //id from 1(=item id in combobox), The display order is sorted by id
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

    juce::StringArray getPlayModeArray()
    {
        juce::StringArray names = {"Off", "Auto", "Midi"};
        return names;
    }

    juce::StringArray getImpulseSwitchArray()
    {
        juce::StringArray names = {"Off", "Template", "Sample"};
        return names;
    }

    juce::StringArray getMaskOptionArray()
    {
        juce::StringArray names = {"Off", "BurstMask", "EuclidMask", "StochasticMask"};
        return names;
    }

    std::string generateEuclidRhythm(int steps, int hits)
    {
        if (hits <= 0)
        {
            return "0";
        }
        if (steps <= hits)
        {
            return "1";
        }
        int base = steps / hits;
        std::vector<int> group = {1};
        for (int j = 0; j < base - 1; ++j)
        {
            group.push_back(0);
        }

        // vector contains 1
        std::vector<std::vector<int>> ones(hits, group);
        // vector contains 0，remainders
        std::vector<std::vector<int>> remainders(steps - (steps / hits) * hits, std::vector<int>{0});

        size_t i = 0;
        while (remainders.size() > 1)
        {
            size_t count = std::min(ones.size(), remainders.size());

            // 把 remainders[i] 中的元素逐个插入到 ones[i] 中
            for (size_t j = 0; j < count; ++j)
            {
                ones[j].insert(ones[j].end(), remainders[j].begin(), remainders[j].end());
            }

            //重新分配zeros
            remainders.erase(remainders.begin(), remainders.begin() + count);
            if (count < ones.size())
            {
                remainders.insert(remainders.end(), ones.begin() + count, ones.end());
                ones.erase(ones.begin() + count, ones.end());
            }
        }

        // 最终合并（flatten）所有子序列
        std::vector<std::vector<int>> total;
        total.insert(total.end(), ones.begin(), ones.end());
        total.insert(total.end(), remainders.begin(), remainders.end());

        std::vector<int> result;
        for (size_t i = 0; i < total.size(); i++)
        {
            result.insert(result.end(), total[i].begin(), total[i].end());
        }

        std::ostringstream oss;
        for (size_t i = 0; i < result.size(); ++i)
        {
            oss << result[i];
            //            if (i != result.size() - 1) {
            //                oss << "";
            //            }
        }
        return oss.str();
    }

    void setRandomIndexToOne(std::string& binaryString)
    {
        if (binaryString.empty()) return;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, binaryString.size() - 1);

        int randomIndex = dis(gen);
        binaryString[randomIndex] = '1';
    }

    std::string generateBinaryString(int length)
    {
        std::string result;
        result.reserve(length);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 1);
        bool existOne = false;
        for (int i = 0; i < length; ++i)
        {
            char temp = dis(gen) ? '1' : '0';
            result += temp;
            if (temp == '1')
            {
                existOne = true;
            }
        }
        if (!existOne)
        {
            setRandomIndexToOne(result);
        }
        return result;
    }

    std::string getThreadIdStr()
    {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        return oss.str();
    }
}
