#pragma once

#include <cstddef>

// HLSL matrix constants store the selected columns of the engine matrix.
inline void PackConstantBufferMatrix(float* destination, const float* rowMajor, std::size_t rows)
{
    for (std::size_t row = 0; row < rows; ++row)
        for (std::size_t column = 0; column < 4; ++column)
            destination[row * 4 + column] = rowMajor[column * 4 + row];
}
