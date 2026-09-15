#include "SQBAudioPreamp/PreampEngine.hpp"
#include <cassert>
#include <cmath>

int main() {
    sqb::audio::AudioBuffer pcm(2, 3840, 384000);
    for (std::size_t i = 0; i < pcm.frames(); ++i) { const float x = .2F + .7F * std::sin(float(i) * .1F); pcm.at(i, 0) = pcm.at(i, 1) = x; }
    sqb::audio::PreampEngine engine;
    auto result = engine.conditionFile(std::move(pcm), {.targetSampleRate = 96000});
    assert(result.sampleRate() == 96000 && result.frames() == 960);
    for (float x : result.samples()) assert(std::isfinite(x) && std::abs(x) <= 1.F);
}
