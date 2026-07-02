// [ SETTINGS ] [ PUDDLES ]

// Keep this override with the scope package so ScreenSpaceShaders cannot
// replace the user's always-on puddle preset through MO2 priority.

#define G_PUDDLES_GLOBAL_SIZE                1.0f
#define G_PUDDLES_SIZE                       0.8f
#define G_PUDDLES_BORDER_HARDNESS            0.7f
#define G_PUDDLES_TERRAIN_EXTRA_WETNESS      0.15f
#define G_PUDDLES_REFLECTIVITY               0.4f
#define G_PUDDLES_TINT                       float3(0.66f, 0.63f, 0.6f)

#define G_PUDDLES_RIPPLES
#define G_PUDDLES_RIPPLES_SCALE              1.0f
#define G_PUDDLES_RIPPLES_INTENSITY          1.0f
#define G_PUDDLES_RIPPLES_RAINING_INT        0.1f
#define G_PUDDLES_RIPPLES_SPEED              1.0f

#define G_PUDDLES_RAIN_RIPPLES_INTENSITY     1.0f
#define G_PUDDLES_RAIN_RIPPLES_SCALE         1.0f

#define G_PUDDLES_REFRACTION_INTENSITY       1.0f

#define G_PUDDLES_ALLWAYS
