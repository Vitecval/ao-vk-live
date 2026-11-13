#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>

namespace rvk::zoning {
// User-supplied loading copy, preserved verbatim without the list numbers.
inline constexpr const wchar_t* kLoadingPhrases[] = {
    L"Rubi-Ka has a 27-hour day. Even the planet needs extra time to finish buffing.",
    L"Rubi-Ka has twin suns. One apparently wasn\u2019t enough to illuminate all that desert.",
    L"AO has an official novel: Prophet Without Honour, written by Ragnar T\u00f8rnquist. There\u2019s more to Rubi-Ka than mission terminals.",
    L"Atrox were engineered for demanding work in the notum mines. Carrying your team is simply a career change.",
    L"According to AO\u2019s official novel, Omni-1 is Rubi-Ka\u2019s oldest city. Corporate architecture has impressive staying power.",
    L"Anarchic Hill hides a giant AO logo in the grass. Fly above Upper Stret East Bank at roughly 989, 2254.",
    L"Marvin\u2019s dialogue references The Hitchhiker\u2019s Guide to the Galaxy. Ask him about life, the universe, and everything.",
    L"Look closely at Old Athen\u2019s streets: even the discarded bottles have their own texture artwork. Someone carefully designed that litter.",
    L"Reet\u2019s Retreat in Last Ditch has a glass dance floor. Sometimes the correct dungeon is a nightclub.",
    L"Newland has a bar called Neuters \u2019R\u2019 Us. Rubi-Ka\u2019s business naming regulations appear fairly relaxed.",
    L"Leets actually speak in leetspeak. Their questionable spelling is part of the local wildlife experience.",
    L"Arete\u2019s Leet King needs help finding romance. Flowers and chocolates remain relevant thousands of years into the future.",
    L"The Shady Guy in Arete can reward you with a ground vehicle. His business proposal involves a crowbar.",
    L"Some named reets and leets attack regardless of your level. Being level 220 will not protect your dignity.",
    L"George, a wandering mini-boss in Greater Tir County, drops a shoulder item that boosts Treatment and First Aid.",
    L"Morty\u2019s Whistle-box adds defense. Sometimes survival depends on finding a very small, very angry creature.",
    L"Ding\u2019s Counterfeit Omni Epaulet can change its side requirement when right-clicked. Even shoulder pads have political flexibility.",
    L"Bureaucrat-made Enhanced Coffee boosts initiatives, movement speed, and critical chance. Office supplies can improve combat performance.",
    L"Enhanced Coffee can also be used on another player like a stim. Keep your damage dealer properly caffeinated.",
    L"Lukewarm Menthol Coffee can be held for Intelligence and Psychic bonuses. Some coffee works better when you don\u2019t drink it.",
    L"Extruder Nutrition Bars temporarily add +2 to all abilities. Sometimes the final step in twinking is eating a snack.",
    L"A pair of QL 10 Concrete Cushions adds +16 Strength and Stamina. Rubi-Ka takes pillow fights seriously.",
    L"An OT-Windchaser M06 Quartz rifle can add +10 Treatment. Some guns are more useful before the fight.",
    L"Virral Triumvirate Eggs combine mantis eggs, gold, soul fragments, and robot parts. Jewellery making gets complicated on Rubi-Ka.",
    L"Biodome key-farming encounters can drop a cardboard box that becomes a novelty backpack. Luxury luggage is a matter of perspective.",
    L"Monster Parts can become more valuable Blood Plasma with a Bio-Comminutor and Pharmaceutical Technology. Your loot bag might contain a side business.",
    L"A complete Omni-Med Suit adds +78 Treatment\u2014and despite its name, other factions can wear it.",
    L"Shop Surgery Clinics grant +100 Treatment temporarily. Visit one before declaring your next implant impossible.",
    L"Implant clusters influence which ability an implant requires. A different cluster combination can make fitting it much easier.",
    L"Check other professions\u2019 implant terminals. The bonuses matter more than the profession printed above the shop.",
    L"Missing a little Treatment? Intelligence, Agility, and Sense buffs can provide the last points through trickle-down.",
    L"Belts and NCU memories do not suffer over-equipping penalties when your Computer Literacy buffs expire. Plan your upgrades accordingly.",
    L"A pet that suddenly ignores commands may be over-equipped. Check your nanoskills before questioning its loyalty.",
    L"Meta-Physicists can fly using Quantum Wings. Some professions buy aircraft; others become the aircraft.",
    L"Check your weapon\u2019s effectiveness after temporary buffs expire. A larger weapon can become a smaller upgrade than expected.",
    L"Nano Initiative reduces casting time, but it does not shorten a nano\u2019s recharge. Fast hands still have to wait.",
    L"With enough initiative, you can move toward full defense while maintaining maximum weapon speed. Revisit your Agg/Def slider as you improve.",
    L"Your healer may need nano more than health. A well-timed nano stim can help keep the entire team standing.",
    L"Put your equipped gun on a hotbar to keep an eye on its remaining ammunition.",
    L"Corpses piled on top of each other? Repeated slow left-clicks can cycle through them.",
    L"Equip your Yalmaha before entering the Grid. You\u2019ll be ready to take off when you emerge.",
    L"A lower-QL Yalmaha has lower Vehicle Air requirements. Your first aircraft doesn\u2019t need to be the fanciest one.",
    L"Nascence garden keys replace the need to spend matching insignias at their garden statues. Permanent keys make frequent travel much easier.",
    L"The original Temple of Three Winds has a level-60 entry limit. Finish your shopping list before outgrowing the front door.",
    L"There\u2019s an ICC return shuttle south of the Temple entrance. The journey home doesn\u2019t have to involve dying.",
    L"The Condemned Subway entrance is in ICC HQ. The old city entrances are closed, however convincing they look.",
    L"Many useful items and nanos can be rolled as mission rewards. Check before paying someone else to find them.",
    L"Blitzing means completing a mission objective quickly for its reward. Every enemy does not need a personal introduction.",
    L"Trader shops can pay more for ordinary loot than standard terminals. A trusted Trader can make your junk collection more profitable.",
    L"Wedged into Rubi-Ka\u2019s architecture? Sit down and try /stuck. Sometimes the strongest escape tool is a chat command.",
};
inline constexpr size_t kLoadingPhraseCount = sizeof(kLoadingPhrases) / sizeof(kLoadingPhrases[0]);

// Shuffle a complete deck so every phrase is seen before a repeat. Also avoid
// repeating the last card immediately when the next deck is shuffled.
struct PhraseRotation {
    std::array<size_t, kLoadingPhraseCount> order{};
    size_t next = kLoadingPhraseCount, previous = kLoadingPhraseCount;
    template<class Random> size_t Next(Random& random) {
        if (next == order.size()) {
            std::iota(order.begin(), order.end(), size_t{0});
            std::shuffle(order.begin(), order.end(), random);
            if (order.front() == previous) std::swap(order.front(), order[1]);
            next = 0;
        }
        previous = order[next++];
        return previous;
    }
};
}
