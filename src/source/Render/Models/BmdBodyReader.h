#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace Render::Models
{
class BmdBodyReader
{
public:
    explicit BmdBodyReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    std::span<const std::uint8_t> Take(std::size_t count)
    {
        if (count > bytes_.size())
            throw std::runtime_error("truncated BMD body");
        auto result = bytes_.first(count);
        bytes_ = bytes_.subspan(count);
        return result;
    }
    template <class T> T Read()
    {
        T result;
        const auto bytes = Take(sizeof(T));
        std::memcpy(&result, bytes.data(), sizeof(T));
        return result;
    }
    int Count(int maximum)
    {
        const int value = Read<std::int16_t>();
        if (value < 0 || value > maximum)
            throw std::runtime_error("invalid BMD count");
        return value;
    }
    void Floats(std::size_t count)
    {
        if (count > bytes_.size() / sizeof(float))
            throw std::runtime_error("truncated BMD floats");
        for (std::size_t i = 0; i < count; ++i)
            if (!std::isfinite(Read<float>()))
                throw std::runtime_error("non-finite BMD value");
    }
    bool Flag()
    {
        const auto value = Read<std::uint8_t>();
        if (value > 1)
            throw std::runtime_error("invalid BMD flag");
        return value != 0;
    }
    bool Empty() const
    {
        return bytes_.empty();
    }

private:
    std::span<const std::uint8_t> bytes_;
};
} // namespace Render::Models
