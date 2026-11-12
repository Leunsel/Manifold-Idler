#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Idler::Json
{
    enum class Kind
    {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object
    };

    struct Value
    {
        Kind Type = Kind::Null;
        bool Flag = false;
        std::string Text;
        std::vector<Value> Items;
        std::vector<std::string> Keys;

        bool Is(Kind kind) const { return Type == kind; }
        const Value* Find(std::string_view key) const;
        std::optional<long long> Integer() const;
    };

    std::optional<Value> Parse(std::string_view text, std::string& problem);
    std::string Quoted(std::string_view text);
}
