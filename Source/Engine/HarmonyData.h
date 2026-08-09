#pragma once

#include <array>

namespace ndlr::harmony
{
struct Scale
{
    std::array<int, 8> intervals;
    int length;
};

inline constexpr std::array<const char*, 15> scaleNames {{
    "Major", "Dorian", "Phrygian", "Lydian", "Mixolydian",
    "Minor (Aeolian)", "Locrian", "Gypsy Min", "Harmonic Minor",
    "Minor Pentatonic", "Whole Tone", "Tonic 2nds", "Tonic 3rds",
    "Tonic 4ths", "Tonic 6ths"
}};

inline constexpr std::array<const char*, 9> colourNames {{
    "Triads", "7ths", "9ths", "11ths", "13ths", "Suspended",
    "6ths & 7ths", "Minor 7ths & 9ths", "Suspended 4ths, 7ths & 9ths"
}};

inline constexpr std::array<const char*, 12> padVoicingNames {{
    "None", "Dynamic", "Grouping", "Open 1 Voicing", "Open 2 Voicing",
    "Open 3 Voicing", "Guitar Voicing", "Drop 2 Voicing", "Drop 3 Voicing",
    "Drop 4 Voicing", "Drop 2 & 3 Voicing", "Voicing Lock"
}};

inline constexpr std::array<const char*, 7> padInversionNames {{
    "No Inversion", "1st Inversion", "2nd Inversion", "Drop 2",
    "Drop 2 & 3", "7ths Inverted", "Octaves & Inverted 3rds"
}};

inline constexpr std::array<Scale, 15> scales {{
    Scale { { 0, 2, 4, 5, 7, 9, 11, -1 }, 7 },
    Scale { { 0, 2, 3, 5, 7, 9, 10, -1 }, 7 },
    Scale { { 0, 1, 3, 5, 7, 8, 10, -1 }, 7 },
    Scale { { 0, 2, 4, 6, 7, 9, 11, -1 }, 7 },
    Scale { { 0, 2, 4, 5, 7, 9, 10, -1 }, 7 },
    Scale { { 0, 2, 3, 5, 7, 8, 10, -1 }, 7 },
    Scale { { 0, 1, 3, 5, 6, 8, 10, -1 }, 7 },
    Scale { { 0, 2, 3, 6, 7, 8, 10, -1 }, 7 },
    Scale { { 0, 2, 3, 5, 7, 8, 11, -1 }, 7 },
    Scale { { 0, 3, 5, 7, 10, -1, -1, -1 }, 5 },
    Scale { { 0, 2, 4, 6, 8, 10, -1, -1 }, 6 },
    Scale { { 0, 2, -1, -1, -1, -1, -1, -1 }, 2 },
    Scale { { 0, 4, -1, -1, -1, -1, -1, -1 }, 2 },
    Scale { { 0, 5, -1, -1, -1, -1, -1, -1 }, 2 },
    Scale { { 0, 9, -1, -1, -1, -1, -1, -1 }, 2 }
}};
}
