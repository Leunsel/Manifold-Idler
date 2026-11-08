#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Idler::Vdf
{
    struct Node
    {
        std::string Key;
        std::string Value;
        std::vector<Node> Children;

        const Node* Find(std::string_view key) const;
    };

    void ParseBinary(std::string_view bytes, Node& root);

    bool Parse(std::string_view text, Node& root, std::string* error = nullptr);
}
