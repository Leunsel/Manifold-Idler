#include "Core/SpriteFile.h"

#include "Core/Json.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <utility>

namespace Idler::SpriteFile
{
    namespace
    {
        using Channels = std::array<double, 3>;

        constexpr Channels kAccent = { 0.18, 0.86, 0.62 };
        constexpr Channels kCard = { 0.082, 0.086, 0.100 };
        constexpr Channels kRunningRow = { 0.100, 0.205, 0.176 };

        std::optional<Sprite> Wrong(std::string& problem, std::string text)
        {
            problem = std::move(text);
            return std::nullopt;
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

        bool ReadColor(const Json::Value& value, Color& color)
        {
            color = Color{};
            if (value.Is(Json::Kind::Null))
                return true;
            if (!value.Is(Json::Kind::String))
                return false;
            std::string_view text = value.Text;
            if (text.empty())
                return true;
            if (text == "accent")
            {
                color.Kind = ColorKind::Accent;
                return true;
            }
            if (text.front() == '#')
                text.remove_prefix(1);
            if (text.size() != 6)
                return false;
            std::uint32_t rgb = 0;
            for (const char c : text)
            {
                const int digit = HexDigit(c);
                if (digit < 0)
                    return false;
                rgb = rgb * 16 + static_cast<std::uint32_t>(digit);
            }
            color.Kind = ColorKind::Rgb;
            color.Rgb = rgb;
            return true;
        }

        bool ReadInt(const Json::Value& value, int& number)
        {
            const std::optional<long long> whole = value.Integer();
            if (!whole || *whole < INT_MIN || *whole > INT_MAX)
                return false;
            number = static_cast<int>(*whole);
            return true;
        }

        bool ReadFrames(const Json::Value& value, std::vector<Frame>& frames)
        {
            if (!value.Is(Json::Kind::Array))
                return false;
            for (const Json::Value& frame : value.Items)
            {
                if (!frame.Is(Json::Kind::Array))
                    return false;
                Frame rows;
                for (const Json::Value& row : frame.Items)
                {
                    if (!row.Is(Json::Kind::String))
                        return false;
                    rows.push_back(row.Text);
                }
                frames.push_back(std::move(rows));
            }
            return true;
        }

        bool ReadSteps(const Json::Value& value, std::vector<Step>& steps)
        {
            if (!value.Is(Json::Kind::Array))
                return false;
            for (const Json::Value& item : value.Items)
            {
                if (!item.Is(Json::Kind::Array) || item.Items.size() != 5)
                    return false;
                Step step;
                if (!ReadInt(item.Items[0], step.Frame) || !ReadInt(item.Items[1], step.Lift) || !ReadInt(item.Items[2], step.Shift) || !ReadInt(item.Items[4], step.Ticks))
                    return false;
                const Json::Value& blink = item.Items[3];
                int number = 0;
                if (blink.Is(Json::Kind::Boolean))
                    step.Blink = blink.Flag;
                else if (ReadInt(blink, number))
                    step.Blink = number != 0;
                else
                    return false;
                steps.push_back(step);
            }
            return true;
        }

        std::string ColorText(const Color& color)
        {
            if (color.Kind == ColorKind::Accent)
                return "\"accent\"";
            if (color.Kind == ColorKind::None)
                return "null";
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "\"#%06X\"", static_cast<unsigned>(color.Rgb & 0xFFFFFF));
            return buffer;
        }

        const char* BoolText(bool value)
        {
            return value ? "true" : "false";
        }

        std::optional<Channels> ChannelsOf(const Color& color)
        {
            if (color.Kind == ColorKind::None)
                return std::nullopt;
            if (color.Kind == ColorKind::Accent)
                return kAccent;
            return Channels{ static_cast<double>((color.Rgb >> 16) & 0xFF) / 255.0, static_cast<double>((color.Rgb >> 8) & 0xFF) / 255.0,
                static_cast<double>(color.Rgb & 0xFF) / 255.0 };
        }

        double Luminance(const Channels& color)
        {
            const auto channel = [](double c) { return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
            return 0.2126 * channel(color[0]) + 0.7152 * channel(color[1]) + 0.0722 * channel(color[2]);
        }

        double Contrast(const Channels& one, const Channels& two)
        {
            const double a = Luminance(one);
            const double b = Luminance(two);
            return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
        }

        std::vector<std::uint32_t> CodePoints(std::string_view text)
        {
            std::vector<std::uint32_t> codes;
            std::size_t at = 0;
            while (at < text.size())
            {
                const auto lead = static_cast<unsigned char>(text[at]);
                const int length = lead < 0x80 ? 1 : lead >= 0xF0 && lead <= 0xF4 ? 4 : lead >= 0xE0 && lead <= 0xEF ? 3 : lead >= 0xC2 && lead <= 0xDF ? 2 : 0;
                bool fine = length > 0 && at + static_cast<std::size_t>(length) <= text.size();
                std::uint32_t code = length == 1 ? lead : length == 2 ? lead & 0x1Fu : length == 3 ? lead & 0x0Fu : lead & 0x07u;
                for (int index = 1; fine && index < length; ++index)
                {
                    const auto next = static_cast<unsigned char>(text[at + static_cast<std::size_t>(index)]);
                    fine = (next & 0xC0) == 0x80;
                    code = (code << 6) | (next & 0x3Fu);
                }
                if (!fine)
                {
                    codes.push_back(0xFFFD);
                    ++at;
                    continue;
                }
                codes.push_back(code);
                at += static_cast<std::size_t>(length);
            }
            return codes;
        }

        void AppendCode(std::string& out, std::uint32_t code)
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

        std::string Repr(const std::vector<std::uint32_t>& codes)
        {
            const bool hasSingle = std::find(codes.begin(), codes.end(), static_cast<std::uint32_t>('\'')) != codes.end();
            const bool hasDouble = std::find(codes.begin(), codes.end(), static_cast<std::uint32_t>('"')) != codes.end();
            const char quote = hasSingle && !hasDouble ? '"' : '\'';
            std::string out(1, quote);
            for (const std::uint32_t code : codes)
            {
                if (code == static_cast<std::uint32_t>(quote) || code == '\\')
                {
                    out.push_back('\\');
                    out.push_back(static_cast<char>(code));
                }
                else if (code == '\t')
                {
                    out += "\\t";
                }
                else if (code == '\n')
                {
                    out += "\\n";
                }
                else if (code == '\r')
                {
                    out += "\\r";
                }
                else if (code < 0x20 || (code >= 0x7F && code < 0xA0))
                {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\x%02x", static_cast<unsigned>(code));
                    out += buffer;
                }
                else
                {
                    AppendCode(out, code);
                }
            }
            out.push_back(quote);
            return out;
        }

        std::string Repr(char cell)
        {
            return Repr(std::vector<std::uint32_t>{ static_cast<unsigned char>(cell) });
        }

        std::string N(long long number)
        {
            return std::to_string(number);
        }

        std::string Fixed1(double value)
        {
            const double quarter = value * 4.0;
            const double half = value * 2.0;
            if (std::floor(quarter) == quarter && std::floor(half) != half)
                value = std::nearbyint(value * 10.0) / 10.0;
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.1f", value);
            return buffer;
        }

        bool HasAny(const std::vector<std::uint32_t>& line, std::string_view cells)
        {
            for (const std::uint32_t code : line)
            {
                if (code < 0x80 && cells.find(static_cast<char>(code)) != std::string_view::npos)
                    return true;
            }
            return false;
        }

        int Filled(const Frame& frame)
        {
            int count = 0;
            for (const std::string& line : frame)
                count += static_cast<int>(std::count_if(line.begin(), line.end(), [](char cell) { return cell != '.'; }));
            return count;
        }

        bool UsesAny(const std::vector<Frame>& frames, std::string_view cells)
        {
            for (const Frame& frame : frames)
            {
                for (const std::string& line : frame)
                {
                    if (line.find_first_of(cells) != std::string::npos)
                        return true;
                }
            }
            return false;
        }
    }

    bool IsCell(char cell)
    {
        return cell != '\0' && kCells.find(cell) != std::string_view::npos;
    }

    double LoopSeconds(const Sprite& sprite)
    {
        long long total = 0;
        for (const Step& step : sprite.Steps)
            total += step.Ticks;
        return static_cast<double>(total) * kTickSeconds;
    }

    std::optional<Sprite> Read(std::string_view text, std::string& problem)
    {
        const std::optional<Json::Value> root = Json::Parse(text, problem);
        if (!root)
            return std::nullopt;
        if (!root->Is(Json::Kind::Object))
            return Wrong(problem, "the file holds no object");

        Sprite sprite;
        bool hasKey = false;
        bool hasId = false;
        for (std::size_t index = 0; index < root->Keys.size(); ++index)
        {
            const std::string& name = root->Keys[index];
            const Json::Value& value = root->Items[index];
            if (name == "key" || name == "id")
            {
                if (!value.Is(Json::Kind::String))
                    return Wrong(problem, "\"" + name + "\" must be text");
                (name == "key" ? sprite.Key : sprite.Id) = value.Text;
                (name == "key" ? hasKey : hasId) = true;
            }
            else if (name == "body" || name == "second" || name == "third")
            {
                Color& color = name == "body" ? sprite.Body : name == "second" ? sprite.Second : sprite.Third;
                if (!ReadColor(value, color))
                    return Wrong(problem, "\"" + name + "\" must be null, \"accent\" or a colour such as \"#5CC756\"");
            }
            else if (name == "floats" || name == "swims" || name == "objectLike" || name == "stays")
            {
                if (!value.Is(Json::Kind::Boolean))
                    return Wrong(problem, "\"" + name + "\" must be true or false");
                if (name == "floats")
                    sprite.Floats = value.Flag;
                else if (name == "swims")
                    sprite.Swims = value.Flag;
                else if (name == "objectLike")
                    sprite.ObjectLike = value.Flag;
                else
                    sprite.Stays = value.Flag;
            }
            else if (name == "sleep" || name == "facing")
            {
                if (!ReadInt(value, name == "sleep" ? sprite.Sleep : sprite.Facing))
                    return Wrong(problem, "\"" + name + "\" must be a whole number");
            }
            else if (name == "frames")
            {
                if (!ReadFrames(value, sprite.Frames))
                    return Wrong(problem, "\"frames\" must be a list of frames, each a list of rows in quotes");
            }
            else if (name == "steps")
            {
                if (!ReadSteps(value, sprite.Steps))
                    return Wrong(problem, "\"steps\" must be a list of steps, each [frame, lift, shift, blink, ticks]");
            }
            else
            {
                return Wrong(problem, "the field \"" + name + "\" is unknown");
            }
        }
        if (!hasKey)
            return Wrong(problem, "the field \"key\" is missing");
        if (!hasId)
            return Wrong(problem, "the field \"id\" is missing");
        problem.clear();
        return sprite;
    }

    std::string Write(const Sprite& sprite)
    {
        std::string out = "{\n";
        const auto field = [&](std::string_view name, const std::string& value)
            {
                out += "  " + Json::Quoted(name) + ": " + value + ",\n";
            };
        field("key", Json::Quoted(sprite.Key));
        field("id", Json::Quoted(sprite.Id));
        field("body", ColorText(sprite.Body));
        field("second", ColorText(sprite.Second));
        field("third", ColorText(sprite.Third));
        field("floats", BoolText(sprite.Floats));
        if (sprite.Swims)
            field("swims", BoolText(*sprite.Swims));
        field("objectLike", BoolText(sprite.ObjectLike));
        field("sleep", N(sprite.Sleep));
        field("facing", N(sprite.Facing));
        field("stays", BoolText(sprite.Stays));

        out += "  \"frames\": [\n";
        for (std::size_t index = 0; index < sprite.Frames.size(); ++index)
        {
            const Frame& frame = sprite.Frames[index];
            out += "    [\n";
            for (std::size_t row = 0; row < frame.size(); ++row)
                out += "      " + Json::Quoted(frame[row]) + (row + 1 < frame.size() ? ",\n" : "\n");
            out += index + 1 < sprite.Frames.size() ? "    ],\n" : "    ]\n";
        }
        out += "  ],\n";
        out += "  \"steps\": [\n";
        for (std::size_t index = 0; index < sprite.Steps.size(); ++index)
        {
            const Step& step = sprite.Steps[index];
            out += "    [" + N(step.Frame) + ", " + N(step.Lift) + ", " + N(step.Shift) + ", " + BoolText(step.Blink) + ", " + N(step.Ticks) + "]";
            out += index + 1 < sprite.Steps.size() ? ",\n" : "\n";
        }
        out += "  ]\n";
        out += "}\n";
        return out;
    }

    Findings Lint(const Sprite& sprite)
    {
        Findings findings;
        std::vector<std::string>& errors = findings.Errors;
        std::vector<std::string>& warnings = findings.Warnings;
        const std::vector<Frame>& frames = sprite.Frames;
        const std::vector<Step>& steps = sprite.Steps;
        const int frameCount = static_cast<int>(frames.size());

        if (frames.empty())
            errors.push_back("no frames");
        if (frameCount > kMaxFrames)
            errors.push_back("more than 8 frames");
        const std::optional<Channels> body = ChannelsOf(sprite.Body);
        const std::optional<Channels> second = ChannelsOf(sprite.Second);
        const std::optional<Channels> third = ChannelsOf(sprite.Third);
        if (!body)
            errors.push_back("no body color");

        for (int index = 0; index < frameCount; ++index)
        {
            const Frame& frame = frames[static_cast<std::size_t>(index)];
            if (static_cast<int>(frame.size()) != kRows)
                errors.push_back("frame " + N(index) + " has " + N(static_cast<long long>(frame.size())) + " rows, not " + N(kRows));
            for (std::size_t row = 0; row < frame.size(); ++row)
            {
                const std::vector<std::uint32_t> line = CodePoints(frame[row]);
                const std::string where = "frame " + N(index) + " row " + N(static_cast<long long>(row));
                if (static_cast<int>(line.size()) != kColumns)
                    errors.push_back(where + " has " + N(static_cast<long long>(line.size())) + " cells, not " + N(kColumns) + ": " + Repr(line));
                std::set<std::uint32_t> unknown;
                for (const std::uint32_t code : line)
                {
                    if (code >= 0x80 || !IsCell(static_cast<char>(code)))
                        unknown.insert(code);
                }
                if (!unknown.empty())
                {
                    std::string list = "[";
                    for (const std::uint32_t code : unknown)
                    {
                        if (list.size() > 1)
                            list += ", ";
                        list += Repr(std::vector<std::uint32_t>{ code });
                    }
                    errors.push_back(where + " has unknown cells " + list + "]");
                }
                if (!second && HasAny(line, "adle"))
                    errors.push_back(where + " uses second-color cells but \"second\" is empty");
                if (!third && HasAny(line, "cC"))
                    errors.push_back(where + " uses third-color cells but \"third\" is empty");
            }
        }
        if (!errors.empty())
            return findings;

        if (steps.empty())
            errors.push_back("no steps");
        for (std::size_t index = 0; index < steps.size(); ++index)
        {
            const Step& step = steps[index];
            const std::string name = "step " + N(static_cast<long long>(index));
            if (step.Frame < 0 || step.Frame >= frameCount)
                errors.push_back(name + " names frame " + N(step.Frame));
            if (step.Lift < 0 || step.Lift > kMaxLift)
                errors.push_back(name + ": lift must be 0 to 4");
            if (step.Shift < -kMaxShift || step.Shift > kMaxShift)
                errors.push_back(name + ": shift must be -2 to 2");
            if (step.Ticks < 1)
                errors.push_back(name + ": ticks must be at least 1");
        }
        if (sprite.Sleep < 0 || sprite.Sleep >= frameCount)
            errors.push_back("sleep names frame " + N(sprite.Sleep));
        if (!errors.empty())
            return findings;

        std::set<int> used = { sprite.Sleep };
        for (const Step& step : steps)
            used.insert(step.Frame);
        for (int index = 0; index < frameCount; ++index)
        {
            if (!used.contains(index))
                warnings.push_back("frame " + N(index) + " is never used");
        }
        const double seconds = LoopSeconds(sprite);
        if (seconds < 2.0)
            warnings.push_back("the loop is only " + Fixed1(seconds) + " s long: it will look hectic");
        if (seconds > 14.0)
            warnings.push_back("the loop is " + Fixed1(seconds) + " s long: its action will rarely be seen");

        const bool hasEyes = UsesAny(frames, "*e");
        if (hasEyes && std::none_of(steps.begin(), steps.end(), [](const Step& step) { return step.Blink; }))
            warnings.push_back("it has eyes but never blinks");
        if (!hasEyes && !sprite.ObjectLike)
            warnings.push_back("no eye cells (* or e) in any frame: set \"objectLike\": true if that is on purpose");

        const auto cellAt = [](const Frame& frame, int row, int column) { return frame[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)]; };
        std::vector<int> grounds;
        for (int index = 0; index < frameCount; ++index)
        {
            const Frame& frame = frames[static_cast<std::size_t>(index)];
            int left = kColumns;
            int right = -1;
            int top = kRows;
            int bottom = -1;
            for (int row = 0; row < kRows; ++row)
            {
                for (int column = 0; column < kColumns; ++column)
                {
                    if (cellAt(frame, row, column) == '.')
                        continue;
                    left = std::min(left, column);
                    right = std::max(right, column);
                    top = std::min(top, row);
                    bottom = std::max(bottom, row);
                }
            }
            if (right < 0)
            {
                errors.push_back("frame " + N(index) + " is empty");
                continue;
            }
            grounds.push_back(bottom);
            const int cells = Filled(frame);
            if (static_cast<double>(cells) < kColumns * kRows * 0.2)
                warnings.push_back("frame " + N(index) + " has only " + N(cells) + " cells: it will look small next to the others");
            const int width = right - left + 1;
            const int height = bottom - top + 1;
            if (std::max(width, height) < std::min(kColumns, kRows) - 2)
                warnings.push_back("frame " + N(index) + " is only " + N(width) + "x" + N(height) + ": use more of the " + N(kColumns) + "x" + N(kRows) + " grid");
            const double offset = (left + right) / 2.0 - (kColumns - 1) / 2.0;
            if (std::fabs(offset) > 2.0)
                warnings.push_back("frame " + N(index) + " sits off centre by " + Fixed1(offset) + " cells");

            for (int row = 0; row < kRows; ++row)
            {
                for (int column = 0; column < kColumns; ++column)
                {
                    const char cell = cellAt(frame, row, column);
                    if (cell == '.')
                        continue;
                    int neighbours = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            const int y = row + dy;
                            const int x = column + dx;
                            if ((dx != 0 || dy != 0) && y >= 0 && y < kRows && x >= 0 && x < kColumns && cellAt(frame, y, x) != '.')
                                ++neighbours;
                        }
                    }
                    if (neighbours == 0 && cell != 'y' && cell != 'w' && cell != 'r')
                        warnings.push_back("frame " + N(index) + ": lone cell " + Repr(cell) + " at row " + N(row) + " column " + N(column));
                }
            }
            for (int row = 0; row < kRows; ++row)
            {
                for (int column = 0; column < kColumns; ++column)
                {
                    const char cell = cellAt(frame, row, column);
                    if (cell != '*' && cell != 'e')
                        continue;
                    int bodyish = 0;
                    int secondish = 0;
                    for (int y = row - 1; y <= row + 1; ++y)
                    {
                        for (int x = column - 1; x <= column + 1; ++x)
                        {
                            if (y < 0 || y >= kRows || x < 0 || x >= kColumns || (x == column && y == row))
                                continue;
                            const char beside = cellAt(frame, y, x);
                            bodyish += beside == '#' || beside == 'o' || beside == '+' ? 1 : 0;
                            secondish += beside == 'a' || beside == 'd' || beside == 'l' ? 1 : 0;
                        }
                    }
                    const std::string where = "frame " + N(index) + " row " + N(row) + " column " + N(column);
                    if (cell == '*' && secondish > bodyish)
                        warnings.push_back(where + ": eye '*' sits on the second color: use 'e' so it shuts in the right color");
                    if (cell == 'e' && bodyish > secondish)
                        warnings.push_back(where + ": eye 'e' sits on the body: use '*' so it shuts in the right color");
                }
            }
        }
        if (!grounds.empty() && !sprite.Floats)
        {
            const std::set<int> distinct(grounds.begin(), grounds.end());
            if (distinct.size() > 1)
            {
                std::string list = "[";
                for (std::size_t index = 0; index < grounds.size(); ++index)
                    list += (index > 0 ? ", " : "") + N(grounds[index]);
                warnings.push_back("the lowest cell moves between frames " + list + "]: feet should stay on the ground, or set \"floats\": true");
            }
            const int lowest = *std::max_element(grounds.begin(), grounds.end());
            if (lowest != kRows - 1)
                warnings.push_back("it stands on row " + N(lowest) + ", not on the ground row " + N(kRows - 1) + ": or set \"floats\": true");
        }

        std::map<std::string, int> seen;
        for (int index = 0; index < frameCount; ++index)
        {
            std::string fingerprint;
            for (const std::string& line : frames[static_cast<std::size_t>(index)])
                fingerprint += line + "\n";
            const auto found = seen.find(fingerprint);
            if (found != seen.end())
                warnings.push_back("frames " + N(found->second) + " and " + N(index) + " are identical");
            seen[fingerprint] = index;
        }

        for (std::size_t index = 0; index < steps.size(); ++index)
        {
            const std::size_t next = (index + 1) % steps.size();
            if (steps[index].Frame == steps[next].Frame)
                continue;
            const Frame& from = frames[static_cast<std::size_t>(steps[index].Frame)];
            const Frame& to = frames[static_cast<std::size_t>(steps[next].Frame)];
            int changed = 0;
            for (int row = 0; row < kRows; ++row)
            {
                for (int column = 0; column < kColumns; ++column)
                    changed += cellAt(from, row, column) != cellAt(to, row, column) ? 1 : 0;
            }
            const int drawn = std::max(1, Filled(from));
            if (static_cast<double>(changed) > std::max(16.0, drawn * 0.5))
                warnings.push_back("step " + N(static_cast<long long>(index)) + " to " + N(static_cast<long long>(next)) + " changes " + N(changed) + " cells of " + N(drawn) +
                    ": it may pop instead of move");
        }

        const struct
        {
            const char* Name;
            const std::optional<Channels>& Value;
            std::string_view Cells;
        } colors[] = { { "body", body, "#o+" }, { "second", second, "adl" }, { "third", third, "cC" } };
        for (const auto& color : colors)
        {
            if (!color.Value)
                continue;
            if (!UsesAny(frames, color.Cells))
            {
                if (std::string_view(color.Name) != "body")
                    warnings.push_back(std::string(color.Name) + " color is set but never used");
                continue;
            }
            const struct
            {
                const Channels& Surface;
                const char* Name;
            } surfaces[] = { { kCard, "the card" }, { kRunningRow, "a running row" } };
            for (const auto& surface : surfaces)
            {
                const double ratio = Contrast(*color.Value, surface.Surface);
                if (ratio < 2.2)
                    warnings.push_back(std::string(color.Name) + " color has low contrast on " + surface.Name + " (" + Fixed1(ratio) + ":1): it will vanish");
            }
        }
        return findings;
    }
}
