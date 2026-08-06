#include "HarmonyWheel.h"
#include "NdlrLookAndFeel.h"

#include <cmath>

namespace ndlr::ui
{
HarmonyWheel::HarmonyWheel()
{
    // Indique visuellement que les secteurs de la roue sont interactifs.
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void HarmonyWheel::paint (juce::Graphics& g)
{
    // L'ordre des tonalités suit le cercle des quintes utilisé par NDLR Max.
    static const juce::StringArray keys { "C", "G", "D", "A", "E", "B", "Gb", "Db", "Ab", "Eb", "Bb", "F" };
    static const juce::StringArray degrees { "I", "II", "III", "IV", "V", "VI", "VII" };

    // Une zone dediee est reservee sous la roue pour les Scales et Colors courants.
    auto bounds = getLocalBounds().toFloat().reduced (5.0f);
    auto footer = bounds.removeFromBottom (24.0f);

    // Les rayons sont proportionnels à la plus petite dimension : la roue reste
    // circulaire et nette quel que soit le redimensionnement du plugin.
    const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const auto outerRadius = diameter * 0.49f;
    const auto middleRadius = outerRadius * 0.62f;
    const auto centreRadius = outerRadius * 0.31f;
    constexpr auto twoPi = juce::MathConstants<float>::twoPi;

    for (auto i = 0; i < 12; ++i)
    {
        // Anneau extérieur : douze secteurs de tonalité, centrés sur midi pour C.
        const auto start = twoPi * (static_cast<float> (i) - 0.5f) / 12.0f;
        const auto end = twoPi * (static_cast<float> (i) + 0.5f) / 12.0f;
        const auto path = makeSector (centre, middleRadius, outerRadius, start, end);
        g.setColour (i == selectedKey ? green.brighter (0.35f) : juce::Colour { 0xff25282b });
        g.fillPath (path);
        g.setColour (panelEdge);
        g.strokePath (path, juce::PathStrokeType { 1.2f });

        const auto angle = (start + end) * 0.5f;
        const auto textCentre = centre + juce::Point<float> { std::sin (angle), -std::cos (angle) } * ((middleRadius + outerRadius) * 0.5f);
        g.setColour (i == selectedKey ? background : juce::Colours::lightgrey);
        g.setFont (juce::FontOptions { outerRadius * 0.18f });
        g.drawText (keys[i], juce::Rectangle<float> { 44.0f, 24.0f }.withCentre (textCentre), juce::Justification::centred);
    }

    for (auto i = 0; i < 7; ++i)
    {
        // Anneau intérieur : sept degrés diatoniques.
        const auto start = twoPi * (static_cast<float> (i) - 0.5f) / 7.0f;
        const auto end = twoPi * (static_cast<float> (i) + 0.5f) / 7.0f;
        const auto path = makeSector (centre, centreRadius, middleRadius, start, end);
        g.setColour (i == selectedDegree ? green.withAlpha (0.82f) : juce::Colour { 0xff202326 });
        g.fillPath (path);
        g.setColour (panelEdge);
        g.strokePath (path, juce::PathStrokeType { 1.2f });

        const auto angle = (start + end) * 0.5f;
        const auto textCentre = centre + juce::Point<float> { std::sin (angle), -std::cos (angle) } * ((centreRadius + middleRadius) * 0.5f);
        g.setColour (i == selectedDegree ? background : juce::Colours::lightgrey);
        g.setFont (juce::FontOptions { outerRadius * 0.15f });
        g.drawText (degrees[i], juce::Rectangle<float> { 42.0f, 22.0f }.withCentre (textCentre), juce::Justification::centred);
    }

    // Disque central : résumé compact de la tonalité et du degré sélectionnés.
    g.setColour (juce::Colour { 0xff181b1d });
    g.fillEllipse (juce::Rectangle<float> { centreRadius * 1.85f, centreRadius * 1.85f }.withCentre (centre));
    g.setColour (green);
    g.drawEllipse (juce::Rectangle<float> { centreRadius * 1.85f, centreRadius * 1.85f }.withCentre (centre), 1.4f);
    g.setFont (juce::FontOptions { outerRadius * 0.18f, juce::Font::bold });
    g.drawText (keys[selectedKey] + " " + degrees[selectedDegree], juce::Rectangle<float> { centreRadius * 1.7f, 28.0f }.withCentre (centre), juce::Justification::centred);

    // Pied de roue : Scales et Colors courants.
    g.setColour (muted);
    g.setFont (juce::FontOptions { juce::jmax (10.0f, outerRadius * 0.065f) });
    g.drawText (scale + " - " + chord,
                footer,
                juce::Justification::centred);
}

bool HarmonyWheel::hitTest (int x, int y)
{
    // Seuls les deux anneaux peints sont interactifs. Les zones transparentes
    // et le pied de la roue laissent ainsi passer les clics vers les contrôles
    // Harmony qu'un layout personnalisé peut placer dessous.
    auto bounds = getLocalBounds().toFloat().reduced (5.0f);
    bounds.removeFromBottom (24.0f);
    const auto centre = bounds.getCentre();
    const auto outerRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.49f;
    const auto distance = juce::Point<float> { static_cast<float> (x), static_cast<float> (y) }
                              .getDistanceFrom (centre);
    return distance >= outerRadius * 0.31f && distance <= outerRadius;
}

void HarmonyWheel::mouseDown (const juce::MouseEvent& event)
{
    // Le calcul doit utiliser exactement les mêmes limites que paint(), footer exclu.
    auto bounds = getLocalBounds().toFloat().reduced (5.0f);
    bounds.removeFromBottom (24.0f);
    const auto centre = bounds.getCentre();
    const auto outerRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.49f;
    const auto distance = event.position.getDistanceFrom (centre);

    if (distance >= outerRadius * 0.62f && distance <= outerRadius)
    {
        // Clic dans l'anneau extérieur : changement de tonalité.
        selectedKey = sectorFromPoint (event.position, centre, 12);
        if (onKeyChanged) onKeyChanged (selectedKey);
        repaint();
    }
    else if (distance >= outerRadius * 0.31f && distance < outerRadius * 0.62f)
    {
        // Clic dans l'anneau intérieur : changement de degré.
        selectedDegree = sectorFromPoint (event.position, centre, 7);
        if (onDegreeChanged) onDegreeChanged (selectedDegree);
        repaint();
    }
}

void HarmonyWheel::setSelection (int keyIndex, int degreeIndex)
{
    // Appelé par l'éditeur lorsque les paramètres changent depuis le DAW ou un preset.
    keyIndex = juce::jlimit (0, 11, keyIndex);
    degreeIndex = juce::jlimit (0, 6, degreeIndex);
    if (selectedKey == keyIndex && selectedDegree == degreeIndex) return;
    selectedKey = keyIndex;
    selectedDegree = degreeIndex;
    repaint();
}

void HarmonyWheel::setHarmonyNames (juce::String scaleName, juce::String chordName)
{
    if (scale == scaleName && chord == chordName) return;
    scale = std::move (scaleName);
    chord = std::move (chordName);
    repaint();
}

juce::Path HarmonyWheel::makeSector (juce::Point<float> centre, float innerRadius,
                                     float outerRadius, float startAngle, float endAngle)
{
    // Construit une portion d'anneau fermée entre deux arcs concentriques.
    juce::Path path;
    path.addCentredArc (centre.x, centre.y, outerRadius, outerRadius, 0.0f,
                        startAngle, endAngle, true);
    path.lineTo (centre + juce::Point<float> { std::sin (endAngle), -std::cos (endAngle) } * innerRadius);
    path.addCentredArc (centre.x, centre.y, innerRadius, innerRadius, 0.0f,
                        endAngle, startAngle, false);
    path.closeSubPath();
    return path;
}

int HarmonyWheel::sectorFromPoint (juce::Point<float> point, juce::Point<float> centre,
                                   int sectorCount) noexcept
{
    // Convertit la position de souris en angle, avec 0 radian placé à midi.
    auto angle = std::atan2 (point.x - centre.x, centre.y - point.y);
    if (angle < 0.0f) angle += juce::MathConstants<float>::twoPi;
    const auto sectorSize = juce::MathConstants<float>::twoPi / static_cast<float> (sectorCount);
    return static_cast<int> ((angle + sectorSize * 0.5f) / sectorSize) % sectorCount;
}
}
