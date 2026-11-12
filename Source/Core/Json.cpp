#include "Core/Json.h"

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <system_error>
#include <utility>

namespace Idler::Json
{
    namespace
    {
        constexpr int kMaxDepth = 64;

        bool IsDigit(char c)
        {
            return c >= '0' && c <= '9';
        }

        int HexDigit(char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        }

        void AppendUtf8(std::string& out, std::uint32_t code)
        {
            if (code < 0x80)
            {
                out.push_back(static_cast<char>(code));
            }
            else if (code < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else if (code < 0x10000)
            {
                out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0 | (code >> 18)));
                out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
        }

        class Parser
        {
        public:
            explicit Parser(std::string_view text) : m_text(text) {}

            std::optional<Value> Document(std::string& problem)
            {
                Value value;
                SkipSpace();
                if (!ParseValue(value, 0) || !End())
                {
                    problem = m_problem;
                    return std::nullopt;
                }
                return value;
            }

        private:
            bool End()
            {
                SkipSpace();
                return AtEnd() || Fail("there is more after the end");
            }

            bool Fail(const char* what)
            {
                if (m_problem.empty())
                {
                    int line = 1;
                    int column = 1;
                    for (std::size_t index = 0; index < m_at && index < m_text.size(); ++index)
                    {
                        if (m_text[index] == '\n')
                        {
                            ++line;
                            column = 1;
                        }
                        else
                        {
                            ++column;
                        }
                    }
                    m_problem = "line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + what;
                }
                return false;
            }

            bool AtEnd() const
            {
                return m_at >= m_text.size();
            }

            char Peek() const
            {
                return m_text[m_at];
            }

            void SkipSpace()
            {
                while (!AtEnd() && (Peek() == ' ' || Peek() == '\t' || Peek() == '\n' || Peek() == '\r'))
                    ++m_at;
            }

            bool Word(std::string_view word)
            {
                if (m_text.substr(m_at, word.size()) != word)
                    return Fail("an unknown word");
                m_at += word.size();
                return true;
            }

            bool ParseValue(Value& value, int depth)
            {
                if (depth > kMaxDepth)
                    return Fail("nested too deeply");
                if (AtEnd())
                    return Fail("a value is missing");
                switch (Peek())
                {
                case '{':
                    return ParseObject(value, depth);
                case '[':
                    return ParseArray(value, depth);
                case '"':
                    value.Type = Kind::String;
                    return ParseString(value.Text);
                case 't':
                    value.Type = Kind::Boolean;
                    value.Flag = true;
                    return Word("true");
                case 'f':
                    value.Type = Kind::Boolean;
                    value.Flag = false;
                    return Word("false");
                case 'n':
                    value.Type = Kind::Null;
                    return Word("null");
                default:
                    if (Peek() == '-' || IsDigit(Peek()))
                        return ParseNumber(value);
                    return Fail("a value is missing");
                }
            }

            bool Digits()
            {
                if (AtEnd() || !IsDigit(Peek()))
                    return false;
                while (!AtEnd() && IsDigit(Peek()))
                    ++m_at;
                return true;
            }

            bool ParseNumber(Value& value)
            {
                const std::size_t start = m_at;
                if (Peek() == '-')
                    ++m_at;
                if (AtEnd() || !IsDigit(Peek()))
                    return Fail("a number has no digits");
                if (Peek() == '0')
                    ++m_at;
                else
                    Digits();
                if (!AtEnd() && Peek() == '.')
                {
                    ++m_at;
                    if (!Digits())
                        return Fail("a number has no digits after its point");
                }
                if (!AtEnd() && (Peek() == 'e' || Peek() == 'E'))
                {
                    ++m_at;
                    if (!AtEnd() && (Peek() == '+' || Peek() == '-'))
                        ++m_at;
                    if (!Digits())
                        return Fail("a number has no digits in its exponent");
                }
                value.Type = Kind::Number;
                value.Text.assign(m_text.substr(start, m_at - start));
                return true;
            }

            bool Hex4(std::uint32_t& code)
            {
                if (m_text.size() - m_at < 4)
                    return Fail("an escape has too few digits");
                code = 0;
                for (std::size_t index = 0; index < 4; ++index)
                {
                    const int digit = HexDigit(m_text[m_at + index]);
                    if (digit < 0)
                        return Fail("an escape has a wrong digit");
                    code = code * 16 + static_cast<std::uint32_t>(digit);
                }
                m_at += 4;
                return true;
            }

            bool ParseString(std::string& out)
            {
                ++m_at;
                while (true)
                {
                    if (AtEnd())
                        return Fail("a string is not closed");
                    const char c = Peek();
                    if (c == '"')
                    {
                        ++m_at;
                        return true;
                    }
                    if (static_cast<unsigned char>(c) < 0x20)
                        return Fail("a string holds a control character");
                    ++m_at;
                    if (c != '\\')
                    {
                        out.push_back(c);
                        continue;
                    }
                    if (AtEnd())
                        return Fail("a string is not closed");
                    const char escape = Peek();
                    ++m_at;
                    switch (escape)
                    {
                    case '"':
                        out.push_back('"');
                        break;
                    case '\\':
                        out.push_back('\\');
                        break;
                    case '/':
                        out.push_back('/');
                        break;
                    case 'b':
                        out.push_back('\b');
                        break;
                    case 'f':
                        out.push_back('\f');
                        break;
                    case 'n':
                        out.push_back('\n');
                        break;
                    case 'r':
                        out.push_back('\r');
                        break;
                    case 't':
                        out.push_back('\t');
                        break;
                    case 'u':
                    {
                        std::uint32_t code = 0;
                        if (!Hex4(code))
                            return false;
                        if (code >= 0xD800 && code <= 0xDBFF)
                        {
                            std::uint32_t low = 0;
                            if (m_text.substr(m_at, 2) != "\\u")
                                return Fail("a surrogate stands alone");
                            m_at += 2;
                            if (!Hex4(low))
                                return false;
                            if (low < 0xDC00 || low > 0xDFFF)
                                return Fail("a surrogate stands alone");
                            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                        }
                        else if (code >= 0xDC00 && code <= 0xDFFF)
                        {
                            return Fail("a surrogate stands alone");
                        }
                        AppendUtf8(out, code);
                        break;
                    }
                    default:
                        return Fail("a string holds an unknown escape");
                    }
                }
            }

            bool ParseArray(Value& value, int depth)
            {
                value.Type = Kind::Array;
                ++m_at;
                SkipSpace();
                if (!AtEnd() && Peek() == ']')
                {
                    ++m_at;
                    return true;
                }
                while (true)
                {
                    SkipSpace();
                    Value item;
                    if (!ParseValue(item, depth + 1))
                        return false;
                    value.Items.push_back(std::move(item));
                    SkipSpace();
                    if (AtEnd())
                        return Fail("a list is not closed");
                    if (Peek() == ']')
                    {
                        ++m_at;
                        return true;
                    }
                    if (Peek() != ',')
                        return Fail("a comma or ] is missing");
                    ++m_at;
                }
            }

            bool ParseObject(Value& value, int depth)
            {
                value.Type = Kind::Object;
                ++m_at;
                SkipSpace();
                if (!AtEnd() && Peek() == '}')
                {
                    ++m_at;
                    return true;
                }
                while (true)
                {
                    SkipSpace();
                    if (AtEnd() || Peek() != '"')
                        return Fail("a name in quotes is missing");
                    std::string key;
                    if (!ParseString(key))
                        return false;
                    for (const std::string& known : value.Keys)
                    {
                        if (known == key)
                            return Fail("a name appears twice");
                    }
                    SkipSpace();
                    if (AtEnd() || Peek() != ':')
                        return Fail("a colon is missing");
                    ++m_at;
                    SkipSpace();
                    Value item;
                    if (!ParseValue(item, depth + 1))
                        return false;
                    value.Keys.push_back(std::move(key));
                    value.Items.push_back(std::move(item));
                    SkipSpace();
                    if (AtEnd())
                        return Fail("an object is not closed");
                    if (Peek() == '}')
                    {
                        ++m_at;
                        return true;
                    }
                    if (Peek() != ',')
                        return Fail("a comma or } is missing");
                    ++m_at;
                }
            }

            std::string_view m_text;
            std::size_t m_at = 0;
            std::string m_problem;
        };

        void Escape(std::string& out, std::uint32_t unit)
        {
            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(unit));
            out += buffer;
        }

        std::uint32_t NextCode(std::string_view text, std::size_t& at)
        {
            const auto lead = static_cast<unsigned char>(text[at]);
            const int length = lead >= 0xF0 && lead <= 0xF4 ? 4 : lead >= 0xE0 && lead <= 0xEF ? 3 : lead >= 0xC2 && lead <= 0xDF ? 2 : 0;
            if (length == 0 || at + static_cast<std::size_t>(length) > text.size())
            {
                ++at;
                return 0xFFFD;
            }
            std::uint32_t code = lead & (length == 2 ? 0x1Fu : length == 3 ? 0x0Fu : 0x07u);
            for (int index = 1; index < length; ++index)
            {
                const auto next = static_cast<unsigned char>(text[at + static_cast<std::size_t>(index)]);
                if ((next & 0xC0) != 0x80)
                {
                    ++at;
                    return 0xFFFD;
                }
                code = (code << 6) | (next & 0x3Fu);
            }
            const std::uint32_t lowest = length == 2 ? 0x80u : length == 3 ? 0x800u : 0x10000u;
            if (code < lowest || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
            {
                ++at;
                return 0xFFFD;
            }
            at += static_cast<std::size_t>(length);
            return code;
        }
    }

    const Value* Value::Find(std::string_view key) const
    {
        if (Type != Kind::Object)
            return nullptr;
        for (std::size_t index = 0; index < Keys.size() && index < Items.size(); ++index)
        {
            if (Keys[index] == key)
                return &Items[index];
        }
        return nullptr;
    }

    std::optional<long long> Value::Integer() const
    {
        if (Type != Kind::Number || Text.find_first_of(".eE") != std::string::npos)
            return std::nullopt;
        long long number = 0;
        const char* end = Text.data() + Text.size();
        const auto [stop, error] = std::from_chars(Text.data(), end, number);
        if (error != std::errc() || stop != end)
            return std::nullopt;
        return number;
    }

    std::optional<Value> Parse(std::string_view text, std::string& problem)
    {
        problem.clear();
        Parser parser(text);
        return parser.Document(problem);
    }

    std::string Quoted(std::string_view text)
    {
        std::string out = "\"";
        std::size_t at = 0;
        while (at < text.size())
        {
            const auto byte = static_cast<unsigned char>(text[at]);
            if (byte >= 0x80)
            {
                const std::uint32_t code = NextCode(text, at);
                if (code >= 0x10000)
                {
                    Escape(out, 0xD800 + ((code - 0x10000) >> 10));
                    Escape(out, 0xDC00 + ((code - 0x10000) & 0x3FF));
                }
                else
                {
                    Escape(out, code);
                }
                continue;
            }
            ++at;
            switch (byte)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            default:
                if (byte < 0x20 || byte == 0x7F)
                    Escape(out, byte);
                else
                    out.push_back(static_cast<char>(byte));
                break;
            }
        }
        out.push_back('"');
        return out;
    }
}
