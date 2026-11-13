#pragma once

#include <cstdint>
#include <string_view>

namespace Idler
{
    class Shell;
}

namespace Idler::UI
{
    void DrawMain(Shell& app);

    void SetStartPage(std::wstring_view page, std::uint32_t selectedApp);
    void SetStartSelection(std::wstring_view apps);
    void SetWebPreview();
    void SetCrashTest(std::wstring_view how);
    void SetAchievementsPreview();
    void SetGameWindowPreview(std::wstring_view tab);
    void SetStartCreature(std::wstring_view key);
    void SetMiniGamePreview(std::wstring_view game);
}
