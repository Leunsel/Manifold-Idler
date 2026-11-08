#include "Core/Vdf.h"

#include "Core/Text.h"

#include <charconv>
#include <cstdint>
#include <cstring>
#include <utility>

namespace Idler::Vdf
{
    namespace
    {
        constexpr int kMaxDepth = 64;

        class Parser
        {
        public:
            explicit Parser(std::string_view text) : m_text(text) {}

            bool Body(Node& parent, int depth)
            {
                if (depth > kMaxDepth)
                    return Fail("nested too deeply");

                while (true)
                {
                    SkipSpace();
                    if (AtEnd())
                        return depth == 0 || Fail("a block is not closed");
                    if (Peek() == '}')
                    {
                        if (depth == 0)
                            return Fail("a block is closed that was never opened");
                        ++m_at;
                        return true;
                    }

                    Node node;
                    if (!Token(node.Key))
                        return false;
                    SkipSpace();
                    if (AtEnd())
                        return Fail("a key has no value");
                    if (Peek() == '{')
                    {
                        ++m_at;
                        const bool closed = Body(node, depth + 1);
                        parent.Children.push_back(std::move(node));
                        if (!closed)
                            return false;
                        continue;
                    }
                    if (!Token(node.Value))
                        return false;
                    parent.Children.push_back(std::move(node));
                }
            }

            const std::string& Error() const { return m_error; }

        private:
            bool AtEnd() const { return m_at >= m_text.size(); }
            char Peek() const { return m_text[m_at]; }

            bool Fail(const char* reason)
            {
                if (m_error.empty())
                    m_error = reason;
                return false;
            }

            void SkipSpace()
            {
                while (!AtEnd())
                {
                    const char c = Peek();
                    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                    {
                        ++m_at;
                    }
                    else if (c == '/' && m_at + 1 < m_text.size() && m_text[m_at + 1] == '/')
                    {
                        const auto end = m_text.find('\n', m_at);
                        m_at = end == std::string_view::npos ? m_text.size() : end + 1;
                    }
                    else
                    {
                        return;
                    }
                }
            }

            bool Token(std::string& out)
            {
                if (Peek() != '"')
                {
                    const auto start = m_at;
                    while (!AtEnd() && Peek() != ' ' && Peek() != '\t' && Peek() != '\r' && Peek() != '\n' && Peek() != '{' && Peek() != '}' && Peek() != '"')
                        ++m_at;
                    if (m_at == start)
                        return Fail("a key or value is missing");
                    out.assign(m_text.substr(start, m_at - start));
                    return true;
                }

                ++m_at;
                while (!AtEnd() && Peek() != '"')
                {
                    char c = m_text[m_at++];
                    if (c == '\\' && !AtEnd())
                    {
                        const char escaped = m_text[m_at++];
                        c = escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped;
                    }
                    out.push_back(c);
                }
                if (AtEnd())
                    return Fail("a string is not closed");
                ++m_at;
                return true;
            }

            std::string_view m_text;
            std::size_t m_at = 0;
            std::string m_error;
        };
    }

    const Node* Node::Find(std::string_view key) const
    {
        for (const Node& child : Children)
        {
            if (EqualsIgnoreCase(child.Key, key))
                return &child;
        }
        return nullptr;
    }

    bool Parse(std::string_view text, Node& root, std::string* error)
    {
        Parser parser(text);
        const bool ok = parser.Body(root, 0);
        if (!ok && error)
            *error = parser.Error();
        return ok;
    }
    namespace
    {
        bool ParseBinaryInto(std::string_view bytes, std::size_t& at, Node& parent, int depth)
        {
            const auto text = [&](std::string& out)
                {
                    const std::size_t end = bytes.find('\0', at);
                    if (end == std::string_view::npos)
                        return false;
                    out.assign(bytes.substr(at, end - at));
                    at = end + 1;
                    return true;
                };
            if (depth > 32)
                return false;
            while (at < bytes.size())
            {
                const auto kind = static_cast<unsigned char>(bytes[at++]);
                if (kind == 8)
                    return true;
                Node node;
                if (!text(node.Key))
                    return false;
                if (kind == 0)
                {
                    const bool whole = ParseBinaryInto(bytes, at, node, depth + 1);
                    parent.Children.push_back(std::move(node));
                    if (!whole)
                        return false;
                    continue;
                }
                if (kind == 1)
                {
                    if (!text(node.Value))
                        return false;
                }
                else if (kind == 2 || kind == 3 || kind == 4 || kind == 6)
                {
                    if (at + 4 > bytes.size())
                        return false;
                    std::uint32_t value = 0;
                    std::memcpy(&value, bytes.data() + at, sizeof(value));
                    at += sizeof(value);
                    if (kind == 2)
                    {
                        node.Value = std::to_string(static_cast<std::int32_t>(value));
                    }
                    else if (kind == 3)
                    {
                        float number = 0.0f;
                        std::memcpy(&number, &value, sizeof(number));
                        char digits[32];
                        const std::to_chars_result written = std::to_chars(digits, digits + sizeof(digits), number);
                        node.Value.assign(digits, written.ec == std::errc() ? written.ptr : digits);
                    }
                    else
                    {
                        node.Value = std::to_string(value);
                    }
                }
                else if (kind == 7 || kind == 10)
                {
                    if (at + 8 > bytes.size())
                        return false;
                    std::uint64_t value = 0;
                    std::memcpy(&value, bytes.data() + at, sizeof(value));
                    at += sizeof(value);
                    node.Value = std::to_string(value);
                }
                else
                {
                    return false;
                }
                parent.Children.push_back(std::move(node));
            }
            return true;
        }
    }

    void ParseBinary(std::string_view bytes, Node& root)
    {
        std::size_t at = 0;
        ParseBinaryInto(bytes, at, root, 0);
    }
}
