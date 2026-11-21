#include "ModGrid.h"

namespace ui
{
namespace
{
const char* const kShortDest[Mod::numDestinations] = {
    "IN", "FRQ", "REZ", "SPL", "SPR", "EX1", "EX2", "FZ", "BIA", "VOL", "MIX", "A1", "A2", "A3"
};
const char* const kShortSource[Mod::numSources] = { "LFO 1", "LFO 2", "LFO 3", "LFO 4", "ENV 1", "ENV 2", "FOL 1", "FOL 2" };
} // namespace

ModGrid::ModGrid (FuzzyFilterProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    lastTargets.fill (-1);
}

juce::RangedAudioParameter& ModGrid::amountParam (int row, int col) const
{
    return *proc.apvts.getParameter (ModParamIds::amount (row, col));
}

juce::String ModGrid::destinationName (int col, bool full) const
{
    if (col < Mod::numFixedDestinations)
        return full ? juce::String (Mod::slotNames[(size_t) col]) : juce::String (kShortDest[col]);
    const int a = col - Mod::numFixedDestinations;
    const int choice = juce::roundToInt (proc.apvts.getRawParameterValue (Mod::assignTargetIds[(size_t) a])->load());
    const int slot = Mod::assignChoiceToSlot (choice);
    const juce::String target = slot < 0 ? "nothing" : juce::String (Mod::slotNames[(size_t) slot]);
    return full ? juce::String (Mod::assignNames[(size_t) a]) + " (" + target + ")"
                : juce::String (kShortDest[col]) + " " + (slot < 0 ? juce::String ("--") : target.toUpperCase());
}

void ModGrid::resized()
{
    auto b = getLocalBounds().toFloat();
    readoutArea = b.removeFromTop (26.0f);
    b.removeFromTop (4.0f);
    cellW = std::floor ((b.getWidth() - labelWidth) / (float) Mod::numDestinations);
    cellH = std::floor (std::min (21.0f, (b.getHeight() - headerH) / (float) Mod::numSources));
    gridArea = juce::Rectangle<float> (b.getX(), b.getY(), labelWidth + cellW * Mod::numDestinations,
                                       headerH + cellH * Mod::numSources);
}

juce::Rectangle<float> ModGrid::cellBounds (int row, int col) const
{
    return { gridArea.getX() + labelWidth + cellW * (float) col, gridArea.getY() + headerH + cellH * (float) row, cellW, cellH };
}

ModGrid::Cell ModGrid::cellAt (juce::Point<int> p) const
{
    const float x = (float) p.x - gridArea.getX() - labelWidth;
    const float y = (float) p.y - gridArea.getY() - headerH;
    if (x < 0.0f || y < 0.0f)
        return {};
    const int col = (int) (x / cellW), row = (int) (y / cellH);
    if (col >= Mod::numDestinations || row >= Mod::numSources)
        return {};
    return { row, col };
}

int ModGrid::assignHeaderAt (juce::Point<int> p) const
{
    const float y = (float) p.y - gridArea.getY();
    if (y < 0.0f || y >= headerH)
        return -1;
    const int col = (int) (((float) p.x - gridArea.getX() - labelWidth) / cellW);
    return col >= Mod::numFixedDestinations && col < Mod::numDestinations ? col - Mod::numFixedDestinations : -1;
}

void ModGrid::refresh()
{
    bool changed = false;
    for (int s = 0; s < Mod::numSources; ++s)
    {
        for (int d = 0; d < Mod::numDestinations; ++d)
        {
            const float v = proc.apvts.getRawParameterValue (ModParamIds::amount (s, d))->load();
            auto& last = lastAmounts[(size_t) (s * Mod::numDestinations + d)];
            changed = changed || ! juce::exactlyEqual (v, last);
            last = v;
        }
        // Row labels show how active each source is.
        const float sv = std::abs (proc.getSourceValue (s));
        changed = changed || std::abs (sv - lastSources[(size_t) s]) > 0.02f;
        lastSources[(size_t) s] = sv;
    }
    for (int a = 0; a < Mod::numAssigns; ++a)
    {
        const int t = juce::roundToInt (proc.apvts.getRawParameterValue (Mod::assignTargetIds[(size_t) a])->load());
        changed = changed || t != lastTargets[(size_t) a];
        lastTargets[(size_t) a] = t;
    }
    if (changed)
        repaint();
}

void ModGrid::paint (juce::Graphics& g)
{
    // Readout of the selected crossing, e.g. "LFO 1 > FRQ +60%".
    {
        const float amount = lastAmounts[(size_t) (selected.row * Mod::numDestinations + selected.col)];
        const auto text = juce::String (kShortSource[selected.row]) + "  >  " + destinationName (selected.col, false)
                          + "  " + (amount > 0.0f ? "+" : "") + juce::String (juce::roundToInt (amount * 100.0f)) + "%";
        g.setFont (fonts::label (11.0f, true, 1.0f));
        juce::GlyphArrangement ga;
        ga.addLineOfText (g.getCurrentFont(), text, 0.0f, 0.0f);
        const float tw = ga.getBoundingBox (0, -1, true).getWidth() + 16.0f;
        auto tag = readoutArea.withLeft (readoutArea.getRight() - tw).withSizeKeepingCentre (tw, 20.0f);
        g.setColour (colours::ink);
        g.fillRect (tag);
        g.setColour (colours::cream);
        g.drawText (text, tag, juce::Justification::centred, false);
        g.setColour (colours::ink);
        g.setFont (fonts::title (26.0f));
        g.drawText ("MOD GRID", readoutArea.withRight (tag.getX() - 6.0f), juce::Justification::centredLeft, true);
    }

    // Column headers.
    g.setFont (fonts::label (9.0f, true));
    for (int d = 0; d < Mod::numDestinations; ++d)
    {
        auto r = juce::Rectangle<float> (gridArea.getX() + labelWidth + cellW * (float) d, gridArea.getY(), cellW, headerH);
        const bool assign = d >= Mod::numFixedDestinations;
        const bool assigned = assign && lastTargets[(size_t) (d - Mod::numFixedDestinations)] > 0;
        if (assign)
        {
            g.setColour (assigned ? colours::red : colours::ink.withAlpha (0.15f));
            g.fillRect (r.reduced (1.0f, 0.0f));
        }
        g.setColour (assigned ? colours::cream : colours::ink);
        g.drawText (kShortDest[d], r, juce::Justification::centred, false);
    }

    // Rows.
    for (int s = 0; s < Mod::numSources; ++s)
    {
        auto label = juce::Rectangle<float> (gridArea.getX(), gridArea.getY() + headerH + cellH * (float) s, labelWidth, cellH);
        g.setColour (colours::ink);
        g.setFont (fonts::label (10.0f, true, 0.5f));
        g.drawText (kShortSource[s], label.withTrimmedRight (4.0f), juce::Justification::centredLeft, false);
        const float activity = juce::jlimit (0.0f, 1.0f, lastSources[(size_t) s]);
        g.setColour (colours::red);
        g.fillRect (juce::Rectangle<float> (label.getX(), label.getBottom() - 3.0f, (labelWidth - 8.0f) * activity, 2.0f));

        for (int d = 0; d < Mod::numDestinations; ++d)
        {
            auto r = cellBounds (s, d);
            const bool sel = s == selected.row && d == selected.col;
            if (sel)
            {
                g.setColour (colours::ink);
                g.fillRect (r);
            }
            const float a = lastAmounts[(size_t) (s * Mod::numDestinations + d)];
            const auto c = r.getCentre();
            const float maxSize = std::min (r.getWidth(), r.getHeight()) - 3.0f;
            if (std::abs (a) < 0.005f)
            {
                g.setColour (sel ? colours::cream.withAlpha (0.6f) : colours::ink.withAlpha (0.35f));
                g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (c));
            }
            else
            {
                const float size = std::min (maxSize, 6.0f + std::abs (a) * 10.0f);
                if (a > 0.0f)
                {
                    g.setColour (colours::red);
                    g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (c));
                }
                else
                {
                    g.setColour (sel ? colours::cream : colours::ink);
                    g.fillRect (juce::Rectangle<float> (size, std::max (3.0f, size * 0.3f)).withCentre (c));
                }
            }
        }
    }
}

void ModGrid::setAmount (int row, int col, float amount)
{
    auto& p = amountParam (row, col);
    p.setValueNotifyingHost (p.convertTo0to1 (juce::jlimit (-1.0f, 1.0f, amount)));
}

void ModGrid::mouseDown (const juce::MouseEvent& e)
{
    if (onTouchTap != nullptr && e.source.isTouch())
    {
        onTouchTap();
        return;
    }
    if (const int a = assignHeaderAt (e.getPosition()); a >= 0)
    {
        selected.col = Mod::numFixedDestinations + a;
        repaint();
        showAssignMenu (a);
        return;
    }
    const auto cell = cellAt (e.getPosition());
    if (cell.row < 0)
        return;
    selected = cell;
    auto& p = amountParam (cell.row, cell.col);
    dragStart = p.convertFrom0to1 (p.getValue());
    p.beginChangeGesture();
    dragging = true;
    repaint();
}

void ModGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    const float sensitivity = dragPixelsPerUnit * (e.mods.isShiftDown() ? 4.0f : 1.0f);
    float v = dragStart - (float) e.getDistanceFromDragStartY() / sensitivity;
    if (std::abs (v) < 0.02f)
        v = 0.0f; // a little detent at zero
    setAmount (selected.row, selected.col, v);
    refresh();
}

void ModGrid::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        amountParam (selected.row, selected.col).endChangeGesture();
    dragging = false;
}

void ModGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto cell = cellAt (e.getPosition());
    if (cell.row < 0)
        return;
    auto& p = amountParam (cell.row, cell.col);
    p.beginChangeGesture();
    setAmount (cell.row, cell.col, 0.0f);
    p.endChangeGesture();
    refresh();
}

void ModGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto cell = cellAt (e.getPosition());
    if (cell.row < 0)
        return;
    selected = cell;
    auto& p = amountParam (cell.row, cell.col);
    const float step = (wheel.deltaY > 0.0f ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.01f : 0.05f);
    p.beginChangeGesture();
    setAmount (cell.row, cell.col, p.convertFrom0to1 (p.getValue()) + step);
    p.endChangeGesture();
    refresh();
    repaint();
}

void ModGrid::showAssignMenu (int assign)
{
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (Mod::assignTargetIds[(size_t) assign]));
    const int current = param->getIndex();
    juce::PopupMenu menu;
    menu.addSectionHeader (juce::String (Mod::assignNames[(size_t) assign]) + " controls");
    for (int i = 0; i < param->choices.size(); ++i)
        menu.addItem (i + 1, param->choices[i], true, i == current);
    const auto header = juce::Rectangle<float> (gridArea.getX() + labelWidth + cellW * (float) (Mod::numFixedDestinations + assign),
                                                gridArea.getY(), cellW, headerH);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (
                            localAreaToGlobal (header.toNearestInt())),
                        [safe = juce::Component::SafePointer<ModGrid> (this), param] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;
                            param->beginChangeGesture();
                            param->setValueNotifyingHost (param->convertTo0to1 ((float) (result - 1)));
                            param->endChangeGesture();
                            safe->refresh();
                        });
}

juce::String ModGrid::getTooltip()
{
    const auto pos = getMouseXYRelative();
    if (const int a = assignHeaderAt (pos); a >= 0)
        return "Choose what " + juce::String (Mod::assignNames[(size_t) a]) + " controls";
    const auto cell = cellAt (pos);
    if (cell.row < 0)
        return {};
    const float amount = lastAmounts[(size_t) (cell.row * Mod::numDestinations + cell.col)];
    return juce::String (Mod::sourceNames[(size_t) cell.row]) + " > " + destinationName (cell.col, true) + ": "
           + juce::String (juce::roundToInt (amount * 100.0f)) + "%. Drag to set, double-click to clear.";
}
} // namespace ui
