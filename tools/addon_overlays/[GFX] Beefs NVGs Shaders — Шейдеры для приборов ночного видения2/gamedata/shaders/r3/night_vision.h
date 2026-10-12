#include "common.h"
uniform float4  shader_param_8;
uniform float4  shader_param_7;

///////////////////////////////////////////////////////
//      ЭФФЕКТ НОЧНОГО ВИДЕНИЯ НА ОСНОВЕ ШЕЙДЕРА ОТ BEEF //
///////////////////////////////////////////////////////
// Огромная благодарность LVutner с Discord-сервера Anomaly, //
// который буквально научил меня всему, что я знаю, Sky4Ace, //
// чья функция simple_blur была адаптирована для этого шейдера, //
// и Meltac, который дал несколько советов и вдохновил на создание //
// этого шейдера.                                       //
///////////////////////////////////////////////////////
// Примечание: Вы можете свободно распространять и адаптировать //
// этот шейдер и его компоненты, но, пожалуйста, укажите авторство //
// меня и/или вышеупомянутых лиц. Я указал авторство отдельных //
// функций и их оригинальных авторов, где это применимо. - BEEF   //
///////////////////////////////////////////////////////

///////////////////////////////////////////////////////
// ПОЛЬЗОВАТЕЛЬСКИЕ НАСТРОЙКИ ЗДЕСЬ
///////////////////////////////////////////////////////

    ////////// ГЛОБАЛЬНЫЕ НАСТРОЙКИ (ВСЕ ПОКОЛЕНИЯ) ////////
        #define sky_brightness_factor float (1.0f) // Фактор яркости неба. Если небо слишком яркое, слегка уменьшите это значение, если слишком тусклое — слегка увеличьте.

    // ЗНАЧЕНИЯ ЭФФЕКТА ЭЛТ И ШУМА ДЛЯ ПНВ
    // (ИМЕЙТЕ В ВИДУ, ЧТО ШУМ И СЦИНТИЛЛЯЦИЯ УСИЛИВАЮТСЯ С УВЕЛИЧЕНИЕМ УСИЛЕНИЯ ПНВ)

        #define gen_1_crt_effect_factor float(0) // По умолчанию 0.05 - Сила эффекта ЭЛТ для изображения ПНВ (0 = отсутствует, 1 = максимум).
        #define gen_1_nvg_noise_factor float (0.1) // По умолчанию 0.15 - Уровень шума для изображения ПНВ (0 = отсутствует, 0.5 = очень высокий).
        #define gen_1_scintillation_constant float (0.999f) // По умолчанию 0.999 - Чем ближе значение к 1.00000, тем меньше эффект сцинтилляции.
        #define gen_1_luma_sharpen_factor float(0.0f) // По умолчанию 0.0 - Уровень повышения резкости яркости (0 = отсутствует, 1 = максимум).
        #define gen_1_out_vignette float(0.1f) // По умолчанию 0.1 - Ширина внешнего виньетирования ПНВ (0 = отсутствует, 1 = высота экрана).
        #define gen_1_lowest_contrast_lowgain float(0.15f) // По умолчанию 0.3 - Минимальный контраст изображения ПНВ на минимальном уровне усиления (0 = максимальный контраст, 1 = ПНВ непригоден).
        #define gen_1_lowest_contrast_highgain float(0.25f) // По умолчанию 0.3 - Минимальный контраст изображения ПНВ на максимальном уровне усиления (0 = максимальный контраст, 1 = ПНВ непригоден).
        #define gen_1_bloom_multiplier float(1.0f) // По умолчанию 1.0 - Сила воздействия эффекта блюма на яркие области (0 = без блюма, 1 = максимум).
        #define gen_1_scintillation_threshold_lowgain float(0.7f) // По умолчанию 0.7 - Цветовой порог эффекта сцинтилляции на минимальном уровне усиления. Затрагиваются только области темнее этого значения (0 = отсутствует, 1 = белый).
        #define gen_1_scintillation_threshold_highgain float(0.9f) // По умолчанию 0.9 - Цветовой порог эффекта сцинтилляции на максимальном уровне усиления. Затрагиваются только области темнее этого значения (0 = отсутствует, 1 = белый).

        #define gen_2_crt_effect_factor float(0) // По умолчанию 0.07 - Сила эффекта ЭЛТ для изображения ПНВ (0 = отсутствует, 1 = максимум).
        #define gen_2_nvg_noise_factor float (0.08) // По умолчанию 0.15 - Уровень шума для изображения ПНВ (0 = отсутствует, 0.5 = очень высокий).
        #define gen_2_scintillation_constant float (0.9991f) // По умолчанию 0.9991 - Чем ближе значение к 1.00000, тем меньше эффект сцинтилляции.
        #define gen_2_luma_sharpen_factor float(0.25f) // По умолчанию 0.25 - Уровень повышения резкости яркости (0 = отсутствует, 1 = максимум).
        #define gen_2_out_vignette float(0.1f) // По умолчанию 0.1 - Ширина внешнего виньетирования ПНВ.
        #define gen_2_lowest_contrast_lowgain float(0.1f) // По умолчанию 0.2 - Минимальный контраст изображения ПНВ на минимальном уровне усиления.
        #define gen_2_lowest_contrast_highgain float(0.2f) // По умолчанию 0.4 - Минимальный контраст изображения ПНВ на максимальном уровне усиления.
        #define gen_2_bloom_multiplier float(0.7f) // По умолчанию 0.7 - Сила воздействия эффекта блюма на яркие области.
        #define gen_2_scintillation_threshold_lowgain float(0.6f) // По умолчанию 0.6 - Цветовой порог эффекта сцинтилляции на минимальном уровне усиления.
        #define gen_2_scintillation_threshold_highgain float(0.8f) // По умолчанию 0.8 - Цветовой порог эффекта сцинтилляции на максимальном уровне усиления.

        #define gen_3_crt_effect_factor float(0.05) // По умолчанию 0.1 - Сила эффекта ЭЛТ для изображения ПНВ (0 = отсутствует, 1 = максимум).
        #define gen_3_nvg_noise_factor float (0.03) // По умолчанию 0.15 - Уровень шума для изображения ПНВ (0 = отсутствует, 0.5 = очень высокий).
        #define gen_3_scintillation_constant float (0.9992f) // По умолчанию 0.9992 - Чем ближе значение к 1.00000, тем меньше эффект сцинтилляции.
        #define gen_3_luma_sharpen_factor float(0.5f) // По умолчанию 0.5 - Уровень повышения резкости яркости (0 = отсутствует, 1 = максимум).
        #define gen_3_out_vignette float(0.3f) // По умолчанию 0.3 - Ширина внешнего виньетирования ПНВ.
        #define gen_3_lowest_contrast_lowgain float(0.05f) // По умолчанию 0.15 - Минимальный контраст изображения ПНВ на минимальном уровне усиления.
        #define gen_3_lowest_contrast_highgain float(0.1f) // По умолчанию 0.3 - Минимальный контраст изображения ПНВ на максимальном уровне усиления.
        #define gen_3_bloom_multiplier float(0.3f) // По умолчанию 0.3 - Сила воздействия эффекта блюма на яркие области.
        #define gen_3_scintillation_threshold_lowgain float(0.4f) // По умолчанию 0.4 - Цветовой порог эффекта сцинтилляции на минимальном уровне усиления.
        #define gen_3_scintillation_threshold_highgain float(0.6f) // По умолчанию 0.6 - Цветовой порог эффекта сцинтилляции на максимальном уровне усиления.

    // НАСТРОЙКИ ЦВЕТА ПНВ:

        #define gen_1_saturation_color float3 (0.16,0.98,0.34) // Цвет ПНВ 1-го поколения (по умолчанию был 0.16,0.98,0.34) - определяет максимальное количество цвета от 0 до 1 в формате (Красный, Зеленый, Синий).
        #define gen_2_saturation_color float3 (0.5,1,0.4) // Цвет ПНВ 2-го поколения (по умолчанию был 0.1,0.99,0.72) - определяет максимальное количество цвета от 0 до 1 в формате (Красный, Зеленый, Синий).
        #define gen_3_saturation_color float3 (0.2,1,0.9) // Цвет ПНВ 3-го поколения (по умолчанию был 0.6,0.6,0.6) - определяет максимальное количество цвета от 0 до 1 в формате (Красный, Зеленый, Синий). 

        // ПРИМЕРЫ ЦВЕТОВЫХ СХЕМ:
        //  (0.7,1,0.6) - мягкий зеленый для 1-го поколения
        //  (0.5,1,0.4) - насыщенный зеленый для 2-го поколения
        //  (0.2,1,0.9) - голубоватый для 3-го поколения
        //  (1.0,1.0,1.0) - черно-белый
        //  (1.0,0.7,0.1) - желтый или янтарный цвет
		
///////////////////////////////////////////////////////
// PROBABLY DON'T CHANGE STUFF BELOW HERE UNLESS YOU KNOW WHAT YOU'RE DOING
///////////////////////////////////////////////////////
										
	// Constants
		#define tube_radius float (frac(shader_param_7.y))
		
		#define single_tube_centered float2(0.5f, -0.5f + (floor(shader_param_7.x) / 100))
		#define single_tube_offset_left float2(0.25f, -0.5f + (floor(shader_param_7.x) / 100))		// Single tube screen position (0.5, 0.5 is centered)
		#define single_tube_offset_right float2(0.75f, -0.5f + (floor(shader_param_7.x) / 100))	// Single tube screen position (0.5, 0.5 is centered)
		
		#define dual_tube_offset_1 float2(0.25f, -0.5f + (floor(shader_param_7.x) / 100))			// Offset for dual tube left eye
		#define dual_tube_offset_2 float2(0.75f, -0.5f + (floor(shader_param_7.x) / 100))			// Offset for dual tube right eye
		
		#define quad_tube_offset_1 float2(0.05f, -0.5f + (floor(shader_param_7.x) / 100))			// Offset for quad tube left outer tube
		#define quad_tube_offset_2 float2(0.3f, -0.5f + (floor(shader_param_7.x) / 100))			// Offset for quad tube left inner tube
		#define quad_tube_offset_3 float2(0.7f, -0.5f + (floor(shader_param_7.x) / 100))			// Offset for quad tube right inner tube
		#define quad_tube_offset_4 float2(0.95f, -0.5f + (floor(shader_param_7.x) / 100))			// Offset for quad tube right outer tube
	
	#define luma_conversion_coeff float3 (0.299, 0.587, 0.114)// When we convert to YUV, these are the coefficients for Y (since we discard UV)
	#define farthest_depth float (25.0f) 						// The farthest far place that we can reach in regards to DOF effects
	
	
	// LUA PACKING
		//	local x_1 = tostring(nvg_generation)                           -- Generation (1,2,3) - outputs 1.x to 3.x
		//	local x_2 = tostring(nvg_num_tubes)                            -- Num Tubes (1,2,4,11,12) outputs x.1, x.2, x.4, x.11, or x.12
		//	local y_1 = tostring(math.floor(nvg_gain_current * 10))        -- Gain Adjust (0.1 to 3) -- outputs 1.y to 30.y in 1. increment
		//	local y_2 = tostring(math.floor(nvg_washout_thresh * 10))      -- Washout Thresh (0.0 - 0.9) - outputs y.0 to y.9 in .1 increment
		//	local z_1 = tostring(math.floor(vignette_current * 100))       -- Vignette Amount (0.0 to 1.0) outputs 0.z to 100.z in 1. increment
		//	local z_2 = tostring(math.floor(glitch_power * 10))            -- Glitch Power (0.0 - 0.9) - outputs z.0 to z.9 in .1 increment
		//	local w_1 = tostring((math.floor(nvg_gain_offset * 10)) )      -- Gain Offset (0.5 to 3.0) - outputs 5.w to 30.w in 1. increment
		//	local w_2 = tostring(nvg_mode)                  		         -- Mode (0,1) - outputs w.0 or w.1 depending on mode

	// SHADER UNPACKING
		// float lua_param_nvg_generation = floor(shader_param_8.x);           	  // 1, 2, or 3
		// float lua_param_nvg_num_tubes = frac(shader_param_8.x) * 10.0f;   	  // 1, 2, 4, 1.1, or 1.2
		// float lua_param_nvg_gain_current = floor(shader_param_8.y) / 10.0f;	  // 0.0 to 3.0
		// float lua_param_nvg_washout_thresh = frac(shader_param_8.y);    		  // 0.0 to 0.9
		// float lua_param_vignette_current = floor(shader_param_8.z) / 100.0f;   // 0.0 to 1.0
		// float lua_param_glitch_power = frac(shader_param_8.z);        		  // 0.0 to 0.9
		// float lua_param_nvg_gain_offset = floor(shader_param_8.w) / 10.0f;	  // 0.5 to 3.0
		// float lua_param_nvg_mode = frac(shader_param_8.w) * 10;                // 0 to 9 in 1.0 increment 
			// mode 0: blurred background (default)
			// mode 1: black background
			// mode 2: image overlay
			// mode 3: no changes (clear vision)
			
			
		// float lua_param_flip_down = floor(shader_param_7.x);
		
		
		// float lua_param_nvg_radius = frac(shader_param_7.y);

// linear rise from low_value (at current_gain = 0.5) to high_value (at current_gain = 2.0)
// IMPORTANT: if nvg_gain_min or nvg_gain_max are changed in z_beefs_nvgs.script, this function is to be remade!
float gain_dynamic_value(float low_value, float high_value, float current_gain)
{
	float offset = low_value * 4 / 3 - high_value / 3;
	float scale = -low_value * 2 / 3 + high_value * 2 / 3;
	return offset + current_gain * scale;
}

// 0, 0 <= color <= A; quadratic rise from 0 to 1, A <= color <= B
float uprise_zero(float color, float A, float B)
{
	if (color >= A)
	{
		float a = 1 / (A-B) / (A-B);
		float b = -2*A / (A-B) / (A-B);
		float c = 1 + (2*A*B - B*B) / (A-B) / (A-B);
		return a * color * color + b * color + c;
	}
	return 0;
}

// color, 0 <= color <= A; quadratic rise from A to 1, A <= color <= B
float uprise(float color, float A, float B)
{
	if (color >= A)
	{
		float a = (1-B) / (A-B) / (A-B);
		float b = (A*A - 2*A + B*B) / (A-B) / (A-B);
		float c = A*A * a;
		return a * color * color + b * color + c;
	}
	return color;
}

// linear rise from A to 1, 0 <= color <= 1
float low_up(float color, float A)
{
	float a = 1 - A;
	float b = A;
	return a * color + b;
}
	
///////////////////////////////////////////////////////
// DEFINE NVG MASK (Credit to LVutner for huge assistance in designing the functions)
///////////////////////////////////////////////////////
float compute_lens_mask(float2 masktc, float num_tubes)
{
	float lua_param_flip_down = floor(shader_param_7.x);
	lua_param_flip_down = clamp(5 - (lua_param_flip_down / 20.0f),1.0f,5.0f);
	masktc.y = masktc.y * lua_param_flip_down;
	
	if (num_tubes > 0.99f && num_tubes < 1.01f) // One tube centered
		{
			return step(distance(masktc,single_tube_centered), tube_radius);
		}
	else if (num_tubes > 1.09f && num_tubes < 1.11f) // One tube left offset
		{
			return step(distance(masktc,single_tube_offset_left), tube_radius);
		}
	else if (num_tubes > 1.19f && num_tubes < 1.21f) // One tube right offset
		{
			return step(distance(masktc,single_tube_offset_right), tube_radius);
		}
	else if (num_tubes > 1.99f && num_tubes < 2.01f) // Two tubes
		{
			if ( (step(distance(masktc,dual_tube_offset_1), tube_radius) == 1) || (step(distance(masktc,dual_tube_offset_2), tube_radius) == 1))
				{
				return 1.0f;
				}
			else
				{
				return 0.0f;
				}
		}
	else if (num_tubes > 3.99f && num_tubes < 4.01f) // Four Tubes
		{
		if  (((step(distance(masktc,quad_tube_offset_1), tube_radius) == 1) || (step(distance(masktc,quad_tube_offset_2), tube_radius) == 1)) || ((step(distance(masktc,quad_tube_offset_3), tube_radius) == 1) || (step(distance(masktc,quad_tube_offset_4), tube_radius) == 1)))
				{
				return 1.0f;
				}
			else
				{
				return 0.0f;
				}
		}
	else
	{
		return 0.0f;
	}
}

///////////////////////////////////////////////////////
// ASPECT RATIO CORRECTION (Credit LVutner)
///////////////////////////////////////////////////////
float2 aspect_ratio_correction (float2 tc)
	{
	tc.x -= 0.5f;
    tc.x *= (screen_res.x / screen_res.y);
    tc.x += 0.5f;
	return tc;
	}


///////////////////////////////////////////////////////
// CRT EFFECT (adapted from MattiasCRT on ShaderToy, credit Mattias)
///////////////////////////////////////////////////////
float2 curve_texturecoords(float2 curved_tc)
{
	curved_tc = (curved_tc - 0.5) * 2.0;
	curved_tc *= 1.1;	
	curved_tc.x *= 1.0 + pow((abs(curved_tc.y) / 5.0), 2.0);
	curved_tc.y *= 1.0 + pow((abs(curved_tc.x) / 4.0), 2.0);
	curved_tc  = (curved_tc / 2.0) + 0.5;
	curved_tc =  curved_tc *0.92 + 0.04;
	return curved_tc;
}

float3 make_crt_ified(float3 fragColor, float2 tc )
{
	//float2 screen_res = screen_res.xy; // define screen res variable
    float2 uv = curve_texturecoords(tc);
    float3 col;
	float x =  sin(0.3*timers.x+uv.y*21.0)*sin(0.7*timers.x+uv.y*29.0)*sin(0.3+0.33*timers.x+uv.y*31.0)*0.0017;

	col.r = fragColor.r;
	col.g = fragColor.g;
	col.b = fragColor.b;
	
	col.r += 0.08f * fragColor.r;
	col.g += 0.05f * fragColor.g;
   	col.b += 0.08f * fragColor.b;

    col = clamp(col*0.6+0.4*col*col*1.0,0.0,1.0);

    float vig = (0.0 + 1.0*16.0*uv.x*uv.y*(1.0-uv.x)*(1.0-uv.y));
	col *= float(pow(abs(vig),0.3));

    col *= float3(0.95,1.05,0.95);
	col *= 2.8;
	
	float scans = clamp( 0.35+0.35*sin(uv.y*screen_res.y*2.0), 0.0, 1.0);
	
	float s = pow(scans,1.7);
	col = col*float( 0.4+0.7*s) ;

    col *= 1.0+0.01*sin(110.0*timers.x);
	if (uv.x < 0.0 || uv.x > 1.0)
		col *= 0.0;
	if (uv.y < 0.0 || uv.y > 1.0)
		col *= 0.0;
	
	col*=1.0-0.65*float(clamp(((tc.x % 2.0)-1.0)*2.0,0.0f,1.0f));
	
    float comp = smoothstep( 0.1, 0.9, sin(timers.x) );
 
    fragColor = col;
	return fragColor;
}

///////////////////////////////////////////////////////
// VIGNETTE CALCULATOR (USED IN NVG AS WELL AS BLOOM PHASES TO DARKEN EDGES OF SHIT)
///////////////////////////////////////////////////////
float calc_vignette (float num_tubes, float2 tc, float vignette_amount)
{
	float vignette;
	float2 corrected_texturecoords = aspect_ratio_correction(tc);
	
	float lua_param_flip_down = floor(shader_param_7.x);
	lua_param_flip_down = clamp(5 - (lua_param_flip_down / 20.0f),1.0f,5.0f);
	
	corrected_texturecoords.y = corrected_texturecoords.y * lua_param_flip_down;
	
	if (num_tubes > 0.99f && num_tubes < 1.01f)
	{
		float gen1_vignette = smoothstep(tube_radius,tube_radius-vignette_amount, distance(corrected_texturecoords,single_tube_centered));
		vignette = 1.0 - (1.0 - gen1_vignette); // apply vignette
	}
	else if (num_tubes > 1.09f && num_tubes < 1.11f)
	{
		float gen1_vignette = smoothstep(tube_radius,tube_radius-vignette_amount, distance(corrected_texturecoords,single_tube_offset_left));
		vignette = 1.0 - (1.0 - gen1_vignette); // apply vignette
	}
	
	else if (num_tubes > 1.19f && num_tubes < 1.21f)
	{
		float gen1_vignette = smoothstep(tube_radius,tube_radius-vignette_amount, distance(corrected_texturecoords,single_tube_offset_right));
		vignette = 1.0 - (1.0 - gen1_vignette); // apply vignette
	}
	else if (num_tubes > 1.99f && num_tubes < 2.01f)
	{
		float gen2_vignette_1 = smoothstep(tube_radius,tube_radius-vignette_amount, distance(corrected_texturecoords,dual_tube_offset_1));
		float gen2_vignette_2 = smoothstep(tube_radius,tube_radius-vignette_amount, distance(corrected_texturecoords,dual_tube_offset_2));
		vignette = vignette_amount > 0 ? 1.0 - ((1.0 - gen2_vignette_1) * (1.0 - gen2_vignette_2)) : gen2_vignette_1 * gen2_vignette_2; // apply vignette
	}
	else if (num_tubes > 3.99f && num_tubes < 4.01f)
	{
		float gen3_vignette_1 = smoothstep(tube_radius,tube_radius-vignette_amount, distance(corrected_texturecoords,quad_tube_offset_1));
		float gen3_vignette_2 = smoothstep(tube_radius,tube_radius-vignette_amount, distance(corrected_texturecoords,quad_tube_offset_2));
		float gen3_vignette_3 = smoothstep(tube_radius,tube_radius-vignette_amount, distance(corrected_texturecoords,quad_tube_offset_3));
		float gen3_vignette_4 = smoothstep(tube_radius,tube_radius-vignette_amount, distance(corrected_texturecoords,quad_tube_offset_4));
		vignette = vignette_amount > 0 ? 1.0 - ((1.0 - gen3_vignette_1) * (1.0 - gen3_vignette_2) * (1.0 - gen3_vignette_3) * (1.0 - gen3_vignette_4))
		            : gen3_vignette_1 * gen3_vignette_2 * gen3_vignette_3 * gen3_vignette_4; // apply vignette
	}
	if (vignette_amount > 0)
	{
		vignette = pow(vignette,2);
	}
	return vignette;
}			



///// blur functions
float IGN_calc(float2 uv)
{
    uv = frac(uv);
    float f = 0.06711056 * uv.x + 0.00583715 * uv.y;
    return 6.28*(frac(52.9829189 * frac(f)));
}

float2x2 rot(float a)
{
    float c;
    float s;
	sincos(a,s,c);
    return float2x2 (c, s, -s, c);
}


///////////////////////////////////////////////////////
// LUMA SHARPEN	(adapted from  Simple Luma Sharpen on ShaderToy, credit xwize)
///////////////////////////////////////////////////////

float3 YUVFromRGB( float3 color)
{
	return float3 (color.r * 0.299 + color.g * 0.587 + color.b * 0.114,
					color.r * -0.147 + color.g * -0.289 + color.b * 0.436,
					color.r * 0.615 + color.g * -0.515 + color.b * -0.100);
}

float3 RGBFromYUV ( float3 color)
{
	return float3 (color.r * 1.0 + color.b * 1.140,
					color.r * 1.0 + color.g * -0.395 + color.b * 0.581 ,
					color.r * 1.0 + color.g * 2.032);
}

float extractLuma(float3 c)
{
    return (c.r * 0.299 + c.g * 0.587 + c.b * 0.114);
}

float3 luma_sharpen(float3 image, float2 uv)
{
    float3 yuv = YUVFromRGB(image);
    
    float2 imgSize = screen_res.xy;
    
    float accumY = 0.0; 
    for(int i = -1; i <= 1; ++i) {
        for(int j = -1; j <= 1; ++j) {
            float2 offset = float2(i,j) / imgSize;
            
            float s = extractLuma(s_blur_2.SampleLevel(smp_rtlinear,uv + offset,0).rrr);
            float notCentre = min(float(i*i + j*j),1.0);
            accumY += s * (9.0 - notCentre*10.0);
        }
    }
    
    accumY /= 9.0;
    
    float gain = 0.9;
    accumY = (accumY + yuv.x)*gain;
    
		image = RGBFromYUV (float3(accumY,yuv.y,yuv.z)); // sharpened
		return image;   
}



///////////////////////////////////////////////////////
// DEPTH BLUR - LITERALLY BLURS THE DEPTH VALUE IN S_POSITION
///////////////////////////////////////////////////////
float blurred_depth (float2 tc)
{
    float Pi = 6.28318530718; // Pi*2
    
    float Directions = 12.0; // BLUR DIRECTIONS (Default 16.0 - More is better but slower)
    float Quality = 4.0; // BLUR QUALITY (Default 4.0 - More is better but slower)
	float Size = 6;
	float2 Radius = Size/screen_res.xy;

	// how far away is the center of our COC
   	uint depth_width, depth_height;
#ifdef USE_MSAA
    uint depth_samples;
    s_position.GetDimensions(depth_width, depth_height, depth_samples);
#else
    s_position.GetDimensions(depth_width, depth_height);
#endif
    float2 depth_size = float2(depth_width, depth_height);
    float center_depth = s_position.Load( int3( (tc)*depth_size, 0 ), 0 ).z;
		if (center_depth == 0) { center_depth = 10000; }
	// set our average depth to be center depth
	float depth_average = 0;
	// where we store the depth_sample
	float depth_sample;
	// where we store the weighted value from sample
	float depth_tap;
	// where we store our weighted
	float weight = 0.0;
	// where we store the total weighted
	float total_weight = 0.0;
	
	// Blur calculations
	for(float i=1.0; i<=Quality; i++) // how far away are we
    {
		for( float d=0.0; d<Pi; d+=Pi/Directions) // where are we around the circle
		{
			// pull depth at our sample point
			depth_sample = s_position.Load( int3( ((tc+float2(cos(d),sin(d))*Radius*i) * depth_size), 0 ), 0 ).z;
			
			// if we hit the sky, give it a depth of 10k
			if (depth_sample == 0) {depth_sample = 10000.0f; }
			
			// if the point we hit is closer than our (center? average), then that point should add blurring to our centerpoint
			if (depth_sample <= center_depth ) 
			{
				weight = Quality - pow((i - 1),0.5);
				depth_average += depth_sample * weight;
				total_weight += weight;
			}
			// but if the point we hit is farther than our (center? average?) then that point should not be adding blur to our centerpoint

        }
    }
	depth_average /= total_weight;
    return depth_average;
}


////////////////////////////////////////////////////////////////////
//////// GLITCH EFFECT /////////////////

float3 glitchEffect( float3 image, float2 tc, float power)
{
    //tc.y = 1.0 - tc.y;
	
	float time = fmod(timers.x, 32.0); // + modelmat[0].x + modelmat[0].z;

	float GLITCH = saturate(power);

	float rnd0 = rand(timers.x);
	float r0 = saturate((1.0-GLITCH)*0.7 + rnd0);
	float rnd1 = rand(floor(tc.x*10.0f * r0) / (10.0f * r0) * time);
	float r1 = 0.5 - (0.5 * GLITCH) + rnd1;
	r1 = 1.0 - max( 0.0, r1);
	float rnd2 = rand(floor(tc.y*40.0f * r1) / (40.0f * r1) * time);
	float r2 = saturate(rnd2);
	float rnd3 = rand(floor(tc.y*10.0f * r0) / (10.0f * r0) * time);
	float r3 = (1.0 - saturate(rnd3 + 0.8)) - 0.1f;

	float pxrnd = rand( dot(tc.x,tc.y) + time );

	float test_result;
	if (rnd0 > 0.5f)
	{ test_result = 1.0f; }
	else
	{test_result = -1.0f; }
	
	float ofs = 0.05 * r2 * GLITCH * test_result;
	ofs += 0.5 * pxrnd * ofs;

	tc.y += 0.1 * r3 * GLITCH;

    int NUM_SAMPLES = 10;
    float RCP_NUM_SAMPLES_F = 1.0 / (float)NUM_SAMPLES;
    
	float3 sum = 0.0f;
	float3 wsum = 0.0f;
	float t0 = 0.0f;
	
	for( int i=0; i<NUM_SAMPLES; ++i )
	{
		float t = (float)i * RCP_NUM_SAMPLES_F;
		tc.x = saturate(tc.x + ofs * t);
		float3 samplecol = s_blur_2.SampleLevel(smp_rtlinear, tc, 0).rgb;
//		float3 s = spectrum_offset( t );
//		samplecol *= s;
		sum += samplecol;
		wsum += 1.0f;
	}
return 	sum.rgb /= wsum;

}