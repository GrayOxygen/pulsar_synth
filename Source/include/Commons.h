#include <vector>
#include <ostream>
#include <sstream>
#include <string>
#include <random>
#include "JuceHeader.h"

namespace why
{
    enum class StateEnum
    {
        Pulse,
        IntraSilence, //silence inside one pular cycle
        InterTrainSilence //silence of train interval
    };

    enum class ImpulseSwitchEnum
    {
        Off = 0,
        Template = 1,
        Sample = 2
    };

    enum class MaskOptionEnum
    {
        Off = 0,
        BurstMask = 1,
        EuclidMask = 2,
        StochasticMask = 3
    };

    juce::StringArray get_impulse_switch_array();

    juce::StringArray get_mask_option_array();

    std::string euclidean_rhythm(int steps, int pulses);
    void setRandomIndexToOne(std::string& binaryString);
    std::string generateBinaryString(int length);

    std::string get_thread_id_str();
}