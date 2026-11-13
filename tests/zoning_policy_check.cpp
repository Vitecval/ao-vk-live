#include "../rvk/zoning_policy.h"
#include "../rvk/zoning_phrases.h"
#include <cstdio>
#include <random>

int main() {
    rvk::zoning::Reveal reveal;
    if (reveal.Visible(0, 100)) return 1;
    reveal.Start(1000);
    if (!reveal.Visible(1001, 900)) return 2;
    if (!reveal.Visible(50000, 900)) return 3; // A stalled loader must not reveal an old frame.
    reveal.Loaded(0);
    for (unsigned i = 0; i < 4; ++i) if (!reveal.Visible(50001, i)) return 4;
    if (reveal.Visible(50001, 4)) return 5;
    reveal.Start(60000); // Consecutive transitions restart readiness.
    if (!reveal.Visible(60001, 900)) return 6;
    reveal.Loaded(900);
    if (!reveal.Visible(60002, 903) || reveal.Visible(60002, 904)) return 7;
    reveal.Start(70000);
    if (!reveal.Visible(159999, 0) || reveal.Visible(160000, 0)) return 8;
    rvk::zoning::Fade fade;
    if (fade.Step(1000, true) != 0) return 9;
    const float halfIn = fade.Step(1090, true);
    if (halfIn < .49f || halfIn > .51f || fade.Step(1180, true) != 1) return 10;
    if (fade.Step(1180, false) != 1 || fade.Step(1340, false) < .49f || fade.Step(1500, false) != 0) return 11;
    fade.Step(1500, true); fade.Step(1590, true);
    if (fade.Step(1590, false) != .5f || fade.Step(1590, true) != .5f) return 12;
    if (fade.Step(2000, true) != 1) return 13;
    static_assert(rvk::zoning::kLoadingPhraseCount == 50);
    for (unsigned seed = 0; seed < 100; ++seed) {
        std::mt19937 random(seed);
        rvk::zoning::PhraseRotation phrases;
        size_t previous = rvk::zoning::kLoadingPhraseCount;
        for (int cycle = 0; cycle < 10; ++cycle) {
            std::array<bool, rvk::zoning::kLoadingPhraseCount> seen{};
            for (size_t i = 0; i < seen.size(); ++i) {
                const size_t index = phrases.Next(random);
                if (index >= seen.size() || seen[index] || index == previous) return 14;
                if (!rvk::zoning::kLoadingPhrases[index][0]) return 15;
                seen[index] = true; previous = index;
            }
        }
    }
    std::puts("50 loading phrases shuffle without repeats, including across deck boundaries");
    std::puts("smooth 180ms fade-in and 320ms fade-out preserve opacity across reversals");
    std::puts("zoning reveal holds through stalls, waits for four fresh world presentations, restarts and times out");
}
