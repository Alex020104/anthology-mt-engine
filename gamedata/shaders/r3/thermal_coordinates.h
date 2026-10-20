#ifndef ANTHOLOGY_THERMAL_COORDINATES_H
#define ANTHOLOGY_THERMAL_COORDINATES_H

// Native lens pixels and the scene G-buffer can have different resolutions.
float2 thermal_position_size()
{
    uint width, height;
#ifdef USE_MSAA
    uint samples;
    s_position.GetDimensions(width, height, samples);
#else
    s_position.GetDimensions(width, height);
#endif
    return float2(width, height);
}

#endif
