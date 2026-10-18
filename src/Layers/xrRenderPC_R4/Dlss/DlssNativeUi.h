#pragma once
#include <algorithm>
#include <string_view>

namespace dlss
{
inline bool IsWearableScreenGlass(std::string_view name)
{
    // WearableDevices draws its screen widgets directly, but its glass is a
    // separate sorted mesh. Move that glass too so it blends over the native UI.
    return name == "wd\\promin\\promin_screen_glass" ||
        name == "wd\\promin\\promin_screen_glass_broken" ||
        name == "wd\\vektor\\vektor_screen_glass";
}

inline bool IsNativeHudUiTexture(std::string_view name)
{
    return name == "$user$ui" || IsWearableScreenGlass(name);
}

inline bool IsNativeHudOverlayShader(std::string_view pixel, std::string_view vertex)
{
    // Resource names retain skinning/MSAA suffixes, e.g. model_def_lplanes(2).
    pixel = pixel.substr(0, pixel.find('('));
    vertex = vertex.substr(0, vertex.find('('));
    // PRS marks and weapon lasers have no stable scene motion/depth of their
    // own. Reconstructing these thin alpha planes loses the mark and trails it.
    return pixel == "models_reflex_reticle" || pixel == "models_reflex_reticle_3db" ||
        pixel == "models_reflex_reticle_simple" || pixel == "models_reflex_reticle_simple_3db" ||
        pixel == "models_lfo_light_dot_weapons" || pixel == "models_lfo_light_dot_weapons_simple" ||
        pixel == "models_laserbeam" ||
        // Legacy collimators and laser rays share this unlit HUD-plane pair.
        (pixel == "base_lplanes" && vertex == "model_def_lplanes");
}

// The queues retain the original draw packets (including skinning and object
// transforms) until the display-resolution pass in the SAME rendered view.
template<class Queue, class Predicate>
void ExtractNativeUi(Queue& scene, Queue& display, Predicate isUi)
{
    display.clear();
    scene.erase(std::remove_if(scene.begin(), scene.end(), [&](const auto& item)
    {
        if (!isUi(item)) return false;
        display.push_back(item);
        return true;
    }), scene.end());
}

template<class Queue, class DisplayQueue>
void RemoveNativeUiDuplicates(Queue& scene, const DisplayQueue& display)
{
    scene.erase(std::remove_if(scene.begin(), scene.end(), [&](const auto& item)
    {
        return std::any_of(display.begin(), display.end(), [&](const auto& ui)
        {
            return item.pVisual == ui.pVisual && item.pMatrix == ui.pMatrix;
        });
    }), scene.end());
}
}
