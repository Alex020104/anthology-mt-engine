#pragma once
#include <cstddef>

namespace anthology
{
struct BufferBindingRange
{
    std::size_t first, end;
    bool changed() const { return first < end; }
};

template<class Ref, class Buffer, class Native>
BufferBindingRange UpdateConstantBufferBindings(Ref* current, Buffer* const* desired,
    Native** native, std::size_t count)
{
    BufferBindingRange range{count, 0};
    for (std::size_t i = 0; i < count; ++i)
    {
        if (current[i]._get() != desired[i])
        {
            // The incoming shader table owns desired throughout this update.
            // Keep unchanged references alive without release/reacquire traffic.
            current[i]._set(desired[i]);
            if (range.first == count) range.first = i;
            range.end = i + 1;
        }
        native[i] = desired[i] ? desired[i]->GetBuffer() : nullptr;
    }
    return range;
}
}
