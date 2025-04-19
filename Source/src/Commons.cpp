#include "Commons.h"

namespace why
{
    juce::StringArray get_impulse_switch_array()
    {
        juce::StringArray names = {"Off", "Template", "Sample"};
        return names;
    }

    juce::StringArray get_mask_option_array()
    {
        juce::StringArray names = {"Off", "BurstMask", "EuclidMask", "StochasticMask"};
        return names;
    }

    /**
     * 欧几里得节奏
     * @param steps 总步数
     * @param pulses 划分的等分
     * @return
     */
    std::string euclidean_rhythm(int steps, int pulses)
    {
        //如果是全是0或全是1，就没有意义构成数组
        if (pulses <= 0)
        {
            return "0";
        }
        if (steps <= pulses)
        {
            return "1";
        }
        int base = steps / pulses;
        std::vector<int> group = {1};
        for (int j = 0; j < base - 1; ++j)
        {
            group.push_back(0);
        }

        // 1数组
        std::vector<std::vector<int>> ones(pulses, group);
        // 0数组，remainders
        std::vector<std::vector<int>> remainders(steps - (steps / pulses) * pulses, std::vector<int>{0});

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

    std::string get_thread_id_str()
    {
        std::ostringstream oss;
        oss << std::this_thread::get_id();  // 将线程ID转为字符串
        return oss.str();
    }
}
