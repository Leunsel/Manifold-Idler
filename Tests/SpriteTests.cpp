#include "TestKit.h"

#include "Core/Creatures.h"
#include "Core/SpriteEdit.h"
#include "Core/SpriteFile.h"
#include "UI/CreatureSprites.h"
#include "UI/SpriteView.h"

using namespace Idler;

namespace
{
    SpriteFile::Sprite SmallSprite()
    {
        SpriteFile::Sprite sprite;
        sprite.Key = "test";
        sprite.Id = "Test";
        sprite.Body = { SpriteFile::ColorKind::Rgb, 0x5CC756 };
        sprite.Second = { SpriteFile::ColorKind::Rgb, 0xD1E58F };
        const SpriteFile::Frame first = {
            "................",
            "................",
            "................",
            "................",
            "....########....",
            "...##########...",
            "..############..",
            "..###*####*###..",
            "..############..",
            "..############..",
            "..##aaaaaaaa##..",
            "..############..",
            "...##########...",
            "...##......##...",
        };
        SpriteFile::Frame second = first;
        second[6] = "..#####++#####..";
        sprite.Frames = { first, second };
        sprite.Steps = { { 0, 0, 0, false, 16 }, { 0, 0, 0, true, 1 }, { 1, 0, 0, false, 8 } };
        return sprite;
    }

    bool Has(const std::vector<std::string>& lines, std::string_view text)
    {
        return std::find(lines.begin(), lines.end(), text) != lines.end();
    }

    bool HasStart(const std::vector<std::string>& lines, std::string_view text)
    {
        return std::any_of(lines.begin(), lines.end(), [&](const std::string& line) { return line.starts_with(text); });
    }

    std::filesystem::path FindCreatureFolder()
    {
        const std::filesystem::path reference = FindErrorReference();
        if (reference.empty())
            return {};
        return reference.parent_path().parent_path() / "Tools" / "CreatureForge" / "creatures";
    }

    bool SameColor(ImVec4 one, ImVec4 two)
    {
        return one.x == two.x && one.y == two.y && one.z == two.z && one.w == two.w;
    }

    bool SameSprite(const UI::CreatureSprites::Sprite& one, const UI::CreatureSprites::Sprite& two)
    {
        if (one.Id != two.Id || one.FrameCount != two.FrameCount || one.StepCount != two.StepCount || one.SleepFrame != two.SleepFrame || one.Facing != two.Facing ||
            one.Floats != two.Floats || one.Swims != two.Swims || one.Stays != two.Stays || !SameColor(one.Body, two.Body) || !SameColor(one.Second, two.Second) ||
            !SameColor(one.Third, two.Third))
            return false;
        for (int frame = 0; frame < one.FrameCount; ++frame)
        {
            for (std::size_t row = 0; row < one.Frames[frame].size(); ++row)
            {
                if (std::strcmp(one.Frames[frame][row], two.Frames[frame][row]) != 0)
                    return false;
            }
        }
        for (int step = 0; step < one.StepCount; ++step)
        {
            const UI::CreatureSprites::Step& a = one.Steps[step];
            const UI::CreatureSprites::Step& b = two.Steps[step];
            if (a.FrameIndex != b.FrameIndex || a.Lift != b.Lift || a.Shift != b.Shift || a.Blink != b.Blink || a.Ticks != b.Ticks)
                return false;
        }
        return true;
    }
}

namespace Tests
{
    void TestSpriteFile()
    {
        using SpriteFile::ColorKind;
        const SpriteFile::Sprite small = SmallSprite();
        const SpriteFile::Findings clean = SpriteFile::Lint(small);
        CHECK(clean.Errors.empty() && clean.Warnings.empty());

        std::string problem;
        const auto again = SpriteFile::Read(SpriteFile::Write(small), problem);
        CHECK(again && *again == small && problem.empty());

        SpriteFile::Sprite tiny;
        tiny.Key = "k";
        tiny.Id = "K";
        tiny.Body = { ColorKind::Accent, 0 };
        tiny.Third = { ColorKind::Rgb, 0x0A0B0C };
        tiny.Floats = true;
        tiny.Facing = -1;
        tiny.Frames = { { "ab", "cd" }, { "ef" } };
        tiny.Steps = { { 0, 1, -1, true, 3 } };
        CHECK(SpriteFile::Write(tiny) ==
            "{\n  \"key\": \"k\",\n  \"id\": \"K\",\n  \"body\": \"accent\",\n  \"second\": null,\n  \"third\": \"#0A0B0C\",\n  \"floats\": true,\n"
            "  \"objectLike\": false,\n  \"sleep\": 0,\n  \"facing\": -1,\n  \"stays\": false,\n  \"frames\": [\n    [\n      \"ab\",\n      \"cd\"\n    ],\n"
            "    [\n      \"ef\"\n    ]\n  ],\n  \"steps\": [\n    [0, 1, -1, true, 3]\n  ]\n}\n");
        tiny.Swims = false;
        const std::string swimming = SpriteFile::Write(tiny);
        CHECK(swimming.find("  \"floats\": true,\n  \"swims\": false,\n  \"objectLike\"") != std::string::npos);
        const auto swims = SpriteFile::Read(swimming, problem);
        CHECK(swims && swims->Swims == false && *swims == tiny);

        CHECK(!SpriteFile::Read("[]", problem) && problem == "the file holds no object");
        CHECK(!SpriteFile::Read("{", problem) && problem.starts_with("line 1"));
        CHECK(!SpriteFile::Read("{\"key\": \"a\"}", problem) && problem == "the field \"id\" is missing");
        CHECK(!SpriteFile::Read("{\"key\": \"a\", \"id\": \"A\", \"colour\": 1}", problem) && problem == "the field \"colour\" is unknown");
        CHECK(!SpriteFile::Read("{\"key\": \"a\", \"id\": \"A\", \"body\": \"#12345\"}", problem) && problem.starts_with("\"body\" must be"));
        CHECK(!SpriteFile::Read("{\"key\": \"a\", \"id\": \"A\", \"steps\": [[0, 0, 0, false]]}", problem) && problem.starts_with("\"steps\" must be"));
        CHECK(!SpriteFile::Read("{\"key\": \"a\", \"id\": \"A\", \"floats\": 1}", problem) && problem == "\"floats\" must be true or false");
        const auto lower = SpriteFile::Read("{\"key\": \"a\", \"id\": \"A\", \"body\": \"#5cc756\", \"steps\": [[0, 0, 0, 1, 2]]}", problem);
        const SpriteFile::Color green{ ColorKind::Rgb, 0x5CC756 };
        CHECK(lower && lower->Body == green && lower->Steps.size() == 1 && lower->Steps[0].Blink);

        const auto lint = [](SpriteFile::Sprite sprite, auto change)
            {
                change(sprite);
                return SpriteFile::Lint(sprite);
            };
        CHECK(lint(small, [](SpriteFile::Sprite& s) { s.Frames.clear(); }).Errors == std::vector<std::string>{ "no frames" });
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[0][4] = "....#######...."; }).Errors, "frame 0 row 4 has 15 cells, not 16: '....#######....'"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[0][5][0] = 'x'; }).Errors, "frame 0 row 5 has unknown cells ['x']"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[0].pop_back(); }).Errors, "frame 0 has 13 rows, not 14"));
        const auto noSecond = lint(small, [](SpriteFile::Sprite& s) { s.Second = {}; });
        CHECK(Has(noSecond.Errors, "frame 0 row 10 uses second-color cells but \"second\" is empty") && Has(noSecond.Errors, "frame 1 row 10 uses second-color cells but \"second\" is empty"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Body = {}; }).Errors, "no body color"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps[0].Lift = 5; }).Errors, "step 0: lift must be 0 to 4"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps[2].Shift = -3; }).Errors, "step 2: shift must be -2 to 2"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps[1].Frame = 2; }).Errors, "step 1 names frame 2"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Sleep = 7; }).Errors, "sleep names frame 7"));

        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps = { { 0, 0, 0, true, 1 }, { 1, 0, 0, false, 1 } }; }).Warnings, "the loop is only 0.2 s long: it will look hectic"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps = { { 0, 0, 0, true, 3 }, { 1, 0, 0, false, 3 } }; }).Warnings, "the loop is only 0.8 s long: it will look hectic"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps = { { 0, 0, 0, true, 60 }, { 1, 0, 0, false, 60 } }; }).Warnings, "the loop is 15.0 s long: its action will rarely be seen"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps[1].Blink = false; }).Warnings, "it has eyes but never blinks"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Steps.pop_back(); }).Warnings, "frame 1 is never used"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[1] = s.Frames[0]; }).Warnings, "frames 0 and 1 are identical"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Third = { SpriteFile::ColorKind::Rgb, 0xFF0000 }; }).Warnings, "third color is set but never used"));
        CHECK(HasStart(lint(small, [](SpriteFile::Sprite& s) { s.Body = { SpriteFile::ColorKind::Rgb, 0x101010 }; }).Warnings, "body color has low contrast on the card ("));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[0][2][0] = '#'; }).Warnings, "frame 0: lone cell '#' at row 2 column 0"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[0][7][5] = 'e'; }).Warnings,
            "frame 0 row 7 column 5: eye 'e' sits on the body: use '*' so it shuts in the right color"));
        const auto lifted = [](SpriteFile::Sprite& s)
            {
                for (SpriteFile::Frame& frame : s.Frames)
                {
                    frame.erase(frame.begin());
                    frame.push_back("................");
                }
            };
        CHECK(Has(lint(small, lifted).Warnings, "it stands on row 12, not on the ground row 13: or set \"floats\": true"));
        CHECK(lint(small, [&](SpriteFile::Sprite& s) { lifted(s); s.Floats = true; }).Warnings.empty());
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.Frames[1][13] = "................"; s.Frames[1][12] = "...##......##..."; }).Warnings,
            "the lowest cell moves between frames [13, 12]: feet should stay on the ground, or set \"floats\": true"));
        CHECK(Has(lint(small, [](SpriteFile::Sprite& s) { s.ObjectLike = false; s.Frames[0][7] = s.Frames[0][6]; s.Frames[1][7] = s.Frames[1][6]; }).Warnings,
            "no eye cells (* or e) in any frame: set \"objectLike\": true if that is on purpose"));
    }

    void TestSpriteEdit()
    {
        using namespace SpriteEdit;
        SpriteFile::Frame frame = EmptyFrame();
        CHECK(frame.size() == 14 && frame[0] == std::string(16, '.'));
        Paint(frame, 2, 3, '#', true);
        CHECK(CellAt(frame, 2, 3) == '#' && CellAt(frame, 2, 12) == '#');
        CHECK(!SetCell(frame, 20, 3, '#') && CellAt(frame, -1, 0) == '\0' && CellAt(frame, 0, 16) == '\0');
        CHECK(!SetCell(frame, 2, 3, '#'));

        PaintLine(frame, 0, 0, 0, 15, 'o', false);
        CHECK(frame[0] == std::string(16, 'o'));
        PaintLine(frame, 5, 0, 9, 4, 'a', false);
        CHECK(CellAt(frame, 5, 0) == 'a' && CellAt(frame, 6, 1) == 'a' && CellAt(frame, 7, 2) == 'a' && CellAt(frame, 8, 3) == 'a' && CellAt(frame, 9, 4) == 'a');
        CHECK(CellAt(frame, 5, 1) == '.' && CellAt(frame, 9, 3) == '.');
        PaintLine(frame, 12, 15, 12, 13, 'k', true);
        CHECK(CellAt(frame, 12, 13) == 'k' && CellAt(frame, 12, 15) == 'k' && CellAt(frame, 12, 0) == 'k' && CellAt(frame, 12, 2) == 'k');

        frame = EmptyFrame();
        PaintRectangle(frame, 8, 9, 4, 4, '#', false, false);
        CHECK(CellAt(frame, 4, 4) == '#' && CellAt(frame, 8, 9) == '#' && CellAt(frame, 6, 4) == '#' && CellAt(frame, 4, 7) == '#');
        CHECK(CellAt(frame, 6, 6) == '.' && CellAt(frame, 3, 4) == '.' && CellAt(frame, 4, 10) == '.');
        Fill(frame, 6, 6, 'a', false);
        CHECK(CellAt(frame, 6, 6) == 'a' && CellAt(frame, 5, 8) == 'a' && CellAt(frame, 7, 5) == 'a');
        CHECK(CellAt(frame, 0, 0) == '.' && CellAt(frame, 4, 4) == '#');
        Fill(frame, 0, 0, 'k', false);
        CHECK(CellAt(frame, 0, 0) == 'k' && CellAt(frame, 13, 15) == 'k' && CellAt(frame, 6, 6) == 'a' && CellAt(frame, 4, 4) == '#');
        Fill(frame, 6, 6, 'a', false);
        CHECK(CellAt(frame, 6, 6) == 'a');

        SpriteFile::Frame walled = EmptyFrame();
        PaintLine(walled, 0, 7, 13, 7, '#', true);
        Fill(walled, 0, 0, 'c', true);
        CHECK(CellAt(walled, 5, 3) == 'c' && CellAt(walled, 5, 12) == 'c' && CellAt(walled, 5, 7) == '#' && CellAt(walled, 5, 8) == '#');

        SpriteFile::Frame dot = EmptyFrame();
        SetCell(dot, 0, 0, '#');
        const SpriteFile::Frame moved = Shifted(dot, 2, 3);
        CHECK(CellAt(moved, 2, 3) == '#' && CellAt(moved, 0, 0) == '.' && moved.size() == 14 && moved[13].size() == 16);
        const SpriteFile::Frame gone = Shifted(dot, -1, 0);
        CHECK(std::all_of(gone.begin(), gone.end(), [](const std::string& line) { return line == std::string(16, '.'); }));
        FlipHorizontal(dot);
        CHECK(CellAt(dot, 0, 15) == '#' && CellAt(dot, 0, 0) == '.');
        CHECK(MirroredColumn(0) == 15 && MirroredColumn(7) == 8);

        CHECK(CellName('o') == "Body shade" && CellName('.') == "Empty" && CellName('x') == "Unknown");
        CHECK(std::size(kCellKinds) == SpriteFile::kCells.size());
        for (const CellKind& kind : kCellKinds)
            CHECK(SpriteFile::IsCell(kind.Cell));
    }

    void TestSpriteTimeline()
    {
        using namespace SpriteEdit;
        const auto marked = [](int column)
            {
                SpriteFile::Frame frame = EmptyFrame();
                SetCell(frame, 0, column, '#');
                return frame;
            };
        const auto steps = [](const SpriteFile::Sprite& sprite)
            {
                std::vector<int> frames;
                for (const SpriteFile::Step& step : sprite.Steps)
                    frames.push_back(step.Frame);
                return frames;
            };

        SpriteFile::Sprite sprite;
        sprite.Frames = { marked(0), marked(1), marked(2) };
        sprite.Steps = { { 0, 0, 0, false, 4 }, { 1, 0, 0, false, 4 }, { 2, 0, 0, false, 4 }, { 1, 0, 0, true, 2 } };
        sprite.Sleep = 2;

        CHECK(TotalTicks(sprite) == 14);
        CHECK(StepAtTick(sprite, 0) == 0 && StepAtTick(sprite, 3) == 0 && StepAtTick(sprite, 4) == 1 && StepAtTick(sprite, 12) == 3 && StepAtTick(sprite, 14) == 0);
        CHECK(StepAtTick(sprite, -1) == 3);
        CHECK(TicksBefore(sprite, 0) == 0 && TicksBefore(sprite, 1) == 4 && TicksBefore(sprite, 3) == 12 && TicksBefore(sprite, 4) == 14);
        CHECK(TicksBefore(sprite, -1) == 0 && TicksBefore(sprite, 9) == 14);
        for (int step = 0; step < 4; ++step)
            CHECK(StepAtTick(sprite, TicksBefore(sprite, step)) == step);

        CHECK(InsertFrame(sprite, 1, marked(3)));
        CHECK(sprite.Frames.size() == 4 && sprite.Frames[1] == marked(3) && sprite.Frames[2] == marked(1));
        CHECK(steps(sprite) == std::vector<int>({ 0, 2, 3, 2 }) && sprite.Sleep == 3);

        CHECK(RemoveFrame(sprite, 2));
        CHECK(sprite.Frames.size() == 3 && sprite.Frames[2] == marked(2));
        CHECK(steps(sprite) == std::vector<int>({ 0, 2 }) && sprite.Sleep == 2);

        CHECK(MoveFrame(sprite, 2, 0));
        CHECK(sprite.Frames[0] == marked(2) && sprite.Frames[1] == marked(0) && sprite.Frames[2] == marked(3));
        CHECK(steps(sprite) == std::vector<int>({ 1, 0 }) && sprite.Sleep == 0);
        CHECK(!MoveFrame(sprite, 0, 3) && !MoveFrame(sprite, 1, 1));

        CHECK(RemoveFrame(sprite, 0));
        CHECK(steps(sprite) == std::vector<int>({ 0 }) && sprite.Sleep == 0);
        CHECK(RemoveFrame(sprite, 0));
        CHECK(sprite.Frames.size() == 1 && sprite.Frames[0] == marked(3));
        CHECK(sprite.Steps.size() == 1 && sprite.Steps[0].Frame == 0 && sprite.Steps[0].Ticks == kStartTicks);
        CHECK(!RemoveFrame(sprite, 0));

        while (sprite.Frames.size() < static_cast<std::size_t>(SpriteFile::kMaxFrames))
            CHECK(InsertFrame(sprite, static_cast<int>(sprite.Frames.size()), EmptyFrame()));
        CHECK(!InsertFrame(sprite, 0, EmptyFrame()));

        CHECK(InsertStep(sprite, 1, { 3, 1, -1, true, 5 }));
        CHECK(InsertStep(sprite, 99, { 4, 0, 0, false, 1 }));
        CHECK(steps(sprite) == std::vector<int>({ 0, 3, 4 }));
        CHECK(MoveStep(sprite, 2, 0) && steps(sprite) == std::vector<int>({ 4, 0, 3 }));
        CHECK(RemoveStep(sprite, 1) && steps(sprite) == std::vector<int>({ 4, 3 }));
        CHECK(RemoveStep(sprite, 0) && !RemoveStep(sprite, 0) && sprite.Steps.size() == 1);
        CHECK(sprite.Steps[0].Lift == 1 && sprite.Steps[0].Shift == -1 && sprite.Steps[0].Blink && sprite.Steps[0].Ticks == 5);
    }

    void TestCreatureFiles()
    {
        const std::filesystem::path folder = FindCreatureFolder();
        std::error_code ignored;
        const bool found = !folder.empty() && std::filesystem::is_directory(folder, ignored);
        CHECK(found);
        if (!found)
            return;

        std::size_t firsts = 0;
        std::size_t evolved = 0;
        for (const bool grown : { false, true })
        {
            for (const auto& entry : std::filesystem::directory_iterator(grown ? folder / "evolved" : folder, ignored))
            {
                if (!entry.is_regular_file(ignored) || entry.path().extension() != ".json")
                    continue;
                const std::string stem = entry.path().stem().string();
                const std::string key = (grown ? "evolved/" : "") + stem;
                std::ifstream in(entry.path(), std::ios::binary);
                std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());

                std::string problem;
                const auto sprite = SpriteFile::Read(text, problem);
                CHECK(sprite.has_value());
                if (!sprite)
                {
                    std::printf("  %s: %s\n", key.c_str(), problem.c_str());
                    continue;
                }
                const bool same = SpriteFile::Write(*sprite) == text;
                CHECK(same);
                if (!same)
                    std::printf("  %s is not written back as it was read\n", key.c_str());
                CHECK(sprite->Key == key);
                const SpriteFile::Findings findings = SpriteFile::Lint(*sprite);
                CHECK(findings.Errors.empty());
                for (const std::string& error : findings.Errors)
                    std::printf("  %s: %s\n", key.c_str(), error.c_str());

                const std::optional<CreatureId> id = CreatureWithKey(stem);
                CHECK(id.has_value());
                if (!id)
                    continue;
                const UI::SpriteView view(*sprite);
                const UI::CreatureSprites::Sprite* built = nullptr;
                if (!grown)
                {
                    built = &UI::CreatureSprites::kSprites[static_cast<std::size_t>(*id)];
                    ++firsts;
                }
                else
                {
                    for (const UI::CreatureSprites::Sprite& candidate : UI::CreatureSprites::kEvolvedSprites)
                    {
                        if (candidate.Id == *id)
                            built = &candidate;
                    }
                    ++evolved;
                }
                const bool matches = built && SameSprite(view.Get(), *built);
                CHECK(matches);
                if (!matches)
                    std::printf("  %s differs from Source/UI/CreatureSprites.h: write it again with forge.py header\n", key.c_str());
            }
        }
        CHECK(firsts == std::size(UI::CreatureSprites::kSprites));
        CHECK(evolved == std::size(UI::CreatureSprites::kEvolvedSprites));
    }
}
