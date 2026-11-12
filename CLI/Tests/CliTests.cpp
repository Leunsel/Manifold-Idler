#include "Core/Errors.h"
#include "Http.h"
#include "Paths.h"
#include "Request.h"
#include "Service.h"
#include "Themes.h"
#include "Web.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    int g_checks = 0;
    int g_failed = 0;

    void Check(bool ok, const char* what, int line)
    {
        ++g_checks;
        if (!ok)
        {
            ++g_failed;
            std::printf("FAILED line %d: %s\n", line, what);
        }
    }
}

#define CHECK(condition) Check((condition), #condition, __LINE__)

using namespace Idler;
using namespace Idler::Cli;

namespace
{
    HttpRequest Parsed(std::string_view text)
    {
        HttpRequest request;
        ParseRequest(text, request);
        return request;
    }

    HttpParse ParseOf(std::string_view text)
    {
        HttpRequest request;
        return ParseRequest(text, request);
    }

    HttpRequest Asked(std::string method, std::string path, std::vector<std::pair<std::string, std::string>> headers, std::string body = {})
    {
        HttpRequest request;
        request.Method = std::move(method);
        const std::size_t question = path.find('?');
        request.Path = path.substr(0, question);
        if (question != std::string::npos)
            request.Query = path.substr(question + 1);
        request.Headers = std::move(headers);
        request.Body = std::move(body);
        return request;
    }

    std::string NonceIn(const std::string& policy)
    {
        const std::string start = "script-src 'nonce-";
        const std::size_t at = policy.find(start);
        if (at == std::string::npos)
            return {};
        const std::size_t from = at + start.size();
        return policy.substr(from, policy.find('\'', from) - from);
    }

    std::string ReadSource(const std::filesystem::path& file)
    {
        std::ifstream in(file, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
        return text;
    }

    std::string ReadBytes(const std::filesystem::path& file)
    {
        std::ifstream in(file, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    const EmbeddedFile* Embedded(std::string_view path)
    {
        for (const EmbeddedFile& file : BuiltInThemeFiles())
        {
            if (file.Path == path)
                return &file;
        }
        return nullptr;
    }

    void PageSourceChecks()
    {
        std::error_code ignored;
        std::filesystem::path root;
        for (std::filesystem::path folder = std::filesystem::current_path(ignored); !folder.empty(); folder = folder.parent_path())
        {
            if (std::filesystem::exists(folder / "CLI" / "Web" / "page.js", ignored))
            {
                root = folder;
                break;
            }
            if (folder == folder.parent_path())
                break;
        }
        const std::filesystem::path web = root / "CLI" / "Web";
        bool current = !root.empty() && ReadSource(web / "page.html") == WebPageHtml() && ReadSource(web / "base.css") == WebPageStyle() &&
            ReadSource(web / "page.js") == WebPageScript();
        std::size_t sourceFiles = 0;
        for (std::filesystem::recursive_directory_iterator entry(web / "Themes", ignored), end; current && !ignored && entry != end; entry.increment(ignored))
        {
            if (!entry->is_regular_file(ignored))
                continue;
            ++sourceFiles;
            std::string relative = PathToUtf8(entry->path().lexically_relative(web / "Themes"));
            std::replace(relative.begin(), relative.end(), '\\', '/');
            const EmbeddedFile* embedded = Embedded(relative);
            current = embedded && embedded->Bytes == ReadSource(entry->path());
        }
        for (const char* font : { "PressStart2P.woff2", "VT323.woff2" })
        {
            const EmbeddedFile* embedded = Embedded(std::string("arcade/") + font);
            current = current && embedded && embedded->Bytes == ReadBytes(root / "Dependencies" / "Fonts" / font);
        }
        current = current && sourceFiles > 0 && BuiltInThemeFiles().size() == sourceFiles + 2;
        if (!current)
            std::printf("  CLI/Source/WebAssets.cpp is not the one of CLI/Web: run CLI/Tools/web-assets.py\n");
        CHECK(current);
        CHECK(WebLogo().rfind("data:image/png;base64,iVBORw0KGgo", 0) == 0);
        const EmbeddedFile* arcade = Embedded("arcade/theme.css");
        CHECK(arcade && arcade->Bytes.find("font-family:\"Idler Pixel\"") != std::string::npos && arcade->Bytes.find("url(PressStart2P.woff2)") != std::string::npos);
    }

    void WriteFile(const std::filesystem::path& file, std::string_view text)
    {
        std::error_code ignored;
        std::filesystem::create_directories(file.parent_path(), ignored);
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << text;
    }

    void ThemeChecks()
    {
        CHECK(IsThemeId("arcade") && IsThemeId("my-theme-2") && IsThemeId("9"));
        CHECK(!IsThemeId("") && !IsThemeId("-x") && !IsThemeId("My") && !IsThemeId("a b") && !IsThemeId("a_b") && !IsThemeId(std::string(33, 'a')));
        CHECK(IsThemeFilePath("theme.css") && IsThemeFilePath("fonts/Pixel-1.woff2") && IsThemeFilePath("a/b/c/d.png"));
        CHECK(!IsThemeFilePath("") && !IsThemeFilePath("../x.css") && !IsThemeFilePath("a/../x.css") && !IsThemeFilePath(".hidden.css") && !IsThemeFilePath("a//b.css"));
        CHECK(!IsThemeFilePath("a\\b.css") && !IsThemeFilePath("a/b/c/d/e.css") && !IsThemeFilePath("x%2e.css") && !IsThemeFilePath("/x.css") && !IsThemeFilePath("x.css/"));
        CHECK(ThemeFileType("theme.css") == "text/css; charset=utf-8" && ThemeFileType("a/B.WOFF2") == "font/woff2" && ThemeFileType("x.svg") == "image/svg+xml");
        CHECK(ThemeFileType("theme.ini").empty() && ThemeFileType("x.js").empty() && ThemeFileType("x.html").empty() && ThemeFileType("x").empty() && ThemeFileType("a.b/c").empty());

        Theme theme;
        theme.Id = "mine";
        std::vector<std::string> problems;
        ReadThemeInfo("\xEF\xBB\xBF; a comment\r\nname = Mine\r\nnote=Calm.\r\n\r\n[variant night]\r\nname=Night\r\nswatch=#112233, #ABC #11223344\r\n"
            "[variant day]\r\n[option glow]\r\nname=Glow\r\nnote=It glows.\r\ndefault=on\r\n[option quiet]\r\n", theme, problems);
        CHECK(problems.empty());
        CHECK(theme.Name == "Mine" && theme.Note == "Calm.");
        CHECK(theme.Variants.size() == 2 && theme.Variants[0].Id == "night" && theme.Variants[0].Name == "Night" && theme.Variants[1].Id == "day");
        CHECK(!theme.Variants.empty() && (theme.Variants[0].Swatch == std::vector<std::string>{ "#112233", "#abc", "#11223344" }));
        CHECK(theme.Options.size() == 2 && theme.Options[0].Id == "glow" && theme.Options[0].On && theme.Options[0].Note == "It glows." && !theme.Options[1].On);

        Theme bad;
        bad.Id = "bad";
        problems.clear();
        ReadThemeInfo("name=Bad\nswatch=#12345 red\n[variant Night]\nname=x\n[variant ok]\nnote=no notes here\n[option ok]\ndefault=maybe\n[look]\nsize=3\nwhat\n[variant ok]\n[option ok]\n", bad, problems);
        CHECK(problems.size() == 8);
        CHECK(!problems.empty() && problems[0] == "Theme problem: bad, theme.ini line 2: \"#12345\" is not a colour such as #22e6ff (MI-7020)");
        CHECK(bad.Name == "Bad" && bad.Swatch.empty() && bad.Variants.size() == 1 && bad.Options.size() == 1);

        const ThemeSet builtIn = LoadThemes({});
        CHECK(builtIn.Problems.empty());
        CHECK(builtIn.Themes.size() == 2 && builtIn.Themes[0].Id == "arcade" && builtIn.Themes[1].Id == "manifold");
        if (builtIn.Themes.size() == 2)
        {
            const Theme& arcade = builtIn.Themes[0];
            CHECK(arcade.Name == "Arcade" && arcade.BuiltIn && !arcade.FromFolder && arcade.Variants.size() == 4 && arcade.Variants[0].Id == "synthwave");
            CHECK(arcade.Options.empty() && builtIn.Themes[1].Options.empty());
            CHECK(arcade.Swatch.size() == 4 && !arcade.Variants.empty() && arcade.Swatch == arcade.Variants[0].Swatch);
            CHECK(builtIn.Themes[1].Name == "Manifold" && builtIn.Themes[1].Variants.empty() && builtIn.Themes[1].Swatch.size() == 4);
        }

        std::error_code ignored;
        const std::filesystem::path folder = std::filesystem::temp_directory_path(ignored) / "manifold-idler-theme-test";
        std::filesystem::remove_all(folder, ignored);
        WriteFile(folder / "mine" / "theme.css", ":root{--accent:#f0f}");
        WriteFile(folder / "mine" / "theme.ini", "name=</script><b>&\n[variant one]\nswatch=#fff\n");
        WriteFile(folder / "mine" / "art" / "dot.png", "\x89PNG dot");
        WriteFile(folder / "mine" / "notes.txt", "not served");
        WriteFile(folder / "arcade" / "theme.css", "/* mine */");
        WriteFile(folder / "Bad Name" / "theme.css", "x");
        WriteFile(folder / "empty" / "readme.png", "x");
        WriteFile(folder / ".git" / "theme.css", "x");
        WriteFile(folder / "outside.css", "secret");

        const ThemeSet own = LoadThemes(folder);
        CHECK(own.Themes.size() == 3 && own.Themes[0].Id == "arcade" && own.Themes[0].FromFolder && own.Themes[0].BuiltIn && own.Themes[0].Name == "arcade" && own.Themes[0].Variants.empty());
        CHECK(own.Themes.size() == 3 && own.Themes[2].Id == "mine" && own.Themes[2].FromFolder && !own.Themes[2].BuiltIn && (own.Themes[2].Swatch == std::vector<std::string>{ "#fff" }));
        CHECK(own.Problems.size() == 2);
        const std::string json = ThemesJson(own);
        CHECK(json.find('<') == std::string::npos && json.find('&') == std::string::npos && json.find("\\u003c/script\\u003e\\u003cb\\u003e\\u0026") != std::string::npos);
        CHECK(json.find("\"from\":\"replaced\"") != std::string::npos && json.find("\"from\":\"built-in\"") != std::string::npos && json.find("\"from\":\"folder\"") != std::string::npos);
        CHECK(json.find("{\"default\":\"arcade\"") == 0 && json.find("\"docs\":\"" + std::string(kThemesReference) + "\"") != std::string::npos);
        CHECK(ThemesText(own).find("arcade") != std::string::npos && ThemesText(own).find("MI-7020") != std::string::npos);

        std::string bytes;
        std::string type;
        CHECK(FindThemeFile(folder, "arcade", "theme.css", bytes, type) && bytes == "/* mine */" && type.rfind("text/css", 0) == 0);
        CHECK(FindThemeFile(folder, "arcade", "VT323.woff2", bytes, type) && type == "font/woff2" && bytes.compare(0, 4, "wOF2") == 0);
        CHECK(FindThemeFile({}, "arcade", "theme.css", bytes, type) && bytes.find("Idler Pixel") != std::string::npos);
        CHECK(FindThemeFile(folder, "mine", "art/dot.png", bytes, type) && type == "image/png" && bytes == "\x89PNG dot");
        CHECK(!FindThemeFile(folder, "mine", "notes.txt", bytes, type));
        CHECK(!FindThemeFile(folder, "mine", "theme.ini", bytes, type));
        CHECK(!FindThemeFile(folder, "mine", "../outside.css", bytes, type));
        CHECK(!FindThemeFile(folder, "empty", "readme.png", bytes, type));
        CHECK(!FindThemeFile(folder, "nothing", "theme.css", bytes, type));
        CHECK(!FindThemeFile(folder, "..", "outside.css", bytes, type));

        WebConfig config;
        config.Token = "0123456789abcdef0123456789abcdef";
        config.Themes = folder;
        const CommandRunner run = [](const std::vector<std::string>&) { return CommandResult{}; };
        const std::pair<std::string, std::string> host{ "host", "127.0.0.1:8090" };
        const auto answer = [&](const HttpRequest& request) { return AnswerWeb(request, config, run); };
        const std::string themes = "/themes/" + config.Token + "/";
        const HttpResponse sheet = answer(Asked("GET", themes + "arcade/theme.css", { host, { "sec-fetch-site", "same-origin" } }));
        CHECK(sheet.Status == 200 && sheet.Body == "/* mine */" && sheet.Type.rfind("text/css", 0) == 0);
        CHECK(sheet.Header("Cache-Control") && *sheet.Header("Cache-Control") == "no-cache" && sheet.Header("X-Content-Type-Options"));
        const HttpResponse font = answer(Asked("GET", themes + "arcade/PressStart2P.woff2", { host }));
        CHECK(font.Status == 200 && font.Type == "font/woff2" && font.Header("Cross-Origin-Resource-Policy"));
        CHECK(answer(Asked("GET", themes + "manifold/theme.css", { host })).Status == 200);
        CHECK(answer(Asked("GET", "/themes/0123456789abcdef0123456789abcdee/arcade/theme.css", { host })).Status == 401);
        CHECK(answer(Asked("GET", "/themes/arcade/theme.css", { host })).Status == 401);
        CHECK(answer(Asked("GET", "/themes/arcade/theme.css?token=" + config.Token, { host })).Status == 401);
        CHECK(answer(Asked("GET", themes + "mine/../outside.css", { host })).Status == 404);
        CHECK(answer(Asked("GET", themes + "mine/%2e%2e/outside.css", { host })).Status == 404);
        CHECK(answer(Asked("GET", themes + "mine/theme.ini", { host })).Status == 404);
        CHECK(answer(Asked("GET", themes + "arcade", { host })).Status == 404);
        CHECK(answer(Asked("GET", "/themes/" + config.Token, { host })).Status == 404);
        CHECK(answer(Asked("POST", themes + "arcade/theme.css", { host })).Status == 405);
        CHECK(answer(Asked("GET", themes + "arcade/theme.css", { host, { "sec-fetch-site", "cross-site" } })).Status == 403);
        const HttpResponse page = answer(Asked("GET", "/?token=" + config.Token, { host }));
        CHECK(page.Status == 200 && page.Body.find("<script type=\"application/json\" id=\"themes\">{\"default\":\"arcade\"") != std::string::npos);
        CHECK(page.Body.find("</script><b>") == std::string::npos && page.Body.find("\"id\":\"mine\"") != std::string::npos);

        std::string text;
        CHECK(CopyTheme(folder, "arcade", "my-arcade", text) && text.find("my-arcade") != std::string::npos);
        CHECK(ReadBytes(folder / "my-arcade" / "theme.css") == "/* mine */" && ReadBytes(folder / "my-arcade" / "VT323.woff2").compare(0, 4, "wOF2") == 0);
        CHECK(ReadSource(folder / "my-arcade" / "theme.ini").find("[theme]\nname=my-arcade\nnote=") == 0);
        CHECK(CopyTheme(folder, "manifold", "calm", text));
        const EmbeddedFile* manifold = Embedded("manifold/theme.css");
        CHECK(manifold && ReadBytes(folder / "calm" / "theme.css") == manifold->Bytes);
        CHECK(CopyTheme(folder, "mine", "mine-2", text) && std::filesystem::exists(folder / "mine-2" / "art" / "dot.png", ignored) && !std::filesystem::exists(folder / "mine-2" / "notes.txt", ignored));
        CHECK(ReadSource(folder / "mine-2" / "theme.ini") == "name=mine-2\n[variant one]\nswatch=#fff\n");
        const ThemeSet copied = LoadThemes(folder);
        CHECK(copied.Themes.size() == 6);
        const auto named = std::find_if(copied.Themes.begin(), copied.Themes.end(), [](const Theme& one) { return one.Id == "my-arcade"; });
        CHECK(named != copied.Themes.end() && named->Name == "my-arcade" && named->Variants.size() == 4 && !named->BuiltIn);
        CHECK(!CopyTheme(folder, "manifold", "calm", text) && text.find("(MI-7018)") != std::string::npos);
        CHECK(!CopyTheme(folder, "nothing", "x", text) && text.find("(MI-7017)") != std::string::npos);
        CHECK(!CopyTheme(folder, "empty", "y", text) && text.find("(MI-7017)") != std::string::npos);
        CHECK(!CopyTheme(folder, "manifold", "Not Fine", text) && text.find("(MI-7021)") != std::string::npos);
        CHECK(!std::filesystem::exists(folder / "x", ignored) && !std::filesystem::exists(folder / "y", ignored));
        std::filesystem::remove_all(folder, ignored);
    }

    void HttpChecks()
    {
        const HttpRequest get = Parsed("GET /?token=abc HTTP/1.1\r\nHost:   127.0.0.1:8090  \r\nUser-Agent: test\r\n\r\n");
        CHECK(get.Method == "GET" && get.Path == "/" && get.Query == "token=abc");
        CHECK(get.Header("host") && *get.Header("host") == "127.0.0.1:8090");
        CHECK(get.Header("user-agent") && *get.Header("user-agent") == "test");
        CHECK(!get.Header("User-Agent"));
        CHECK(ParseOf("GET / HTTP/1.1\r\nHost: a\r\n\r\n") == HttpParse::Complete);
        CHECK(ParseOf("GET / HTTP/1.0\r\n\r\n") == HttpParse::Complete);
        CHECK(ParseOf("GET / HTTP/1.1\r\nHost: a\r\n") == HttpParse::Incomplete);
        CHECK(ParseOf("") == HttpParse::Incomplete);

        const std::string post = "POST /api HTTP/1.1\r\nHost: a\r\nContent-Length: 10\r\n\r\n";
        CHECK(ParseOf(post + "[\"stat") == HttpParse::Incomplete);
        const HttpRequest full = Parsed(post + "[\"status\"]extra");
        CHECK(full.Method == "POST" && full.Path == "/api" && full.Body == "[\"status\"]");

        CHECK(ParseOf(std::string(kLongestHead + 1, 'a')) == HttpParse::TooLarge);
        CHECK(ParseOf("POST /api HTTP/1.1\r\nContent-Length: 70000\r\n\r\n") == HttpParse::TooLarge);
        CHECK(ParseOf("GET / HTTP/1.1\r\nX-Long: " + std::string(kLongestHead, 'a') + "\r\n\r\n") == HttpParse::TooLarge);

        CHECK(ParseOf("GET / HTTP/2\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("get / HTTP/1.1\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET  / HTTP/1.1\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET / HTTP/1.1 more\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET http://evil.example/ HTTP/1.1\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET /\x01 HTTP/1.1\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET / HTTP/1.1\r\nHost a\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET / HTTP/1.1\r\n: a\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET / HTTP/1.1\r\nBad Name: a\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET / HTTP/1.1\r\nHost: a\r\n folded\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET / HTTP/1.1\r\nOrigin: a\r\norigin: b\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET / HTTP/1.1\r\nX-Idler-Token: a\r\nX-Idler-Token: b\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("GET / HTTP/1.1\r\nHost: a\x01\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("POST /api HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("POST /api HTTP/1.1\r\nContent-Length: 1x\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("POST /api HTTP/1.1\r\nContent-Length: -1\r\n\r\n") == HttpParse::Bad);
        CHECK(ParseOf("POST /api HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx") == HttpParse::Bad);

        CHECK(QueryValue("token=abc&x=1", "token") == "abc");
        CHECK(QueryValue("x=1&token=a%2Bb%zz", "token") == "a+b%zz");
        CHECK(QueryValue("a=b+c", "a") == "b c");
        CHECK(QueryValue("tokenx=1&xtoken=2", "token").empty());
        CHECK(QueryValue("", "token").empty());
        CHECK(QueryValue("token", "token").empty());

        HttpResponse response;
        response.Type = "text/plain";
        response.Body = "hello";
        response.Headers.emplace_back("X-Test", "1");
        const std::string text = ResponseText(response);
        CHECK(text.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
        CHECK(text.find("Content-Type: text/plain\r\n") != std::string::npos);
        CHECK(text.find("Content-Length: 5\r\n") != std::string::npos);
        CHECK(text.find("X-Test: 1\r\n") != std::string::npos);
        CHECK(text.size() > 9 && text.substr(text.size() - 9) == "\r\n\r\nhello");
    }

    void JsonChecks()
    {
        CHECK(JsonString("a\"b\\c\n\t\x01") == "\"a\\\"b\\\\c\\n\\t\\u0001\"");
        CHECK(JsonString("\xC3\xBC") == "\"\xC3\xBC\"");

        std::vector<std::string> words;
        CHECK((ParseJsonWords("[\"add\",\"--\",\"620\"]", words) && words == std::vector<std::string>{ "add", "--", "620" }));
        CHECK((ParseJsonWords(" [ \"a\" ,\r\n \"\" ] ", words) && words == std::vector<std::string>{ "a", "" }));
        CHECK(ParseJsonWords("[]", words) && words.empty());
        CHECK(ParseJsonWords("[\"\\u00fc\\ud83d\\ude00\\n\\\"\\\\\\/\"]", words) && words.size() == 1 && words[0] == "\xC3\xBC\xF0\x9F\x98\x80\n\"\\/");
        CHECK(!ParseJsonWords("", words));
        CHECK(!ParseJsonWords("[\"a\",]", words));
        CHECK(!ParseJsonWords("[\"a\"", words));
        CHECK(!ParseJsonWords("\"a\"", words));
        CHECK(!ParseJsonWords("[1]", words));
        CHECK(!ParseJsonWords("[\"a\"] x", words));
        CHECK(!ParseJsonWords("[\"a\" \"b\"]", words));
        CHECK(!ParseJsonWords("[\"\\ud800\"]", words));
        CHECK(!ParseJsonWords("[\"\\udc00\"]", words));
        CHECK(!ParseJsonWords("[\"\\u12\"]", words));
        CHECK(!ParseJsonWords("[\"a\x01\"]", words));
        CHECK(!ParseJsonWords("[\"\\q\"]", words));
        CHECK(!ParseJsonWords("{\"words\":[\"a\"]}", words));
        std::string many = "[";
        for (std::size_t index = 0; index <= kMostWords; ++index)
            many += index > 0 ? ",\"a\"" : "\"a\"";
        CHECK(!ParseJsonWords(many + "]", words));

        CommandResult result;
        result.Text = "fine\n";
        CHECK(ResultJson(result) == "{\"ok\":true,\"text\":\"fine\\n\"}");
        result.Ok = false;
        result.Columns = { "app", "name" };
        result.Rows = { { "620", "Portal 2" }, { "480", "Spacewar" } };
        CHECK(ResultJson(result) == "{\"ok\":false,\"text\":\"fine\\n\",\"columns\":[\"app\",\"name\"],\"rows\":[[\"620\",\"Portal 2\"],[\"480\",\"Spacewar\"]]}");
        CommandResult failed;
        failed.Ok = false;
        failed.Text = ErrorMessage(ErrorCode::SteamNotRunning) + "\n";
        CHECK(ResultJson(failed) == "{\"ok\":false,\"text\":\"Steam is not running. (MI-1001)\\n\",\"code\":\"MI-1001\"}");
    }

    void AddressChecks()
    {
        WebAddress address;
        std::string problem;
        CHECK(ParseWebAddress("", address, problem) && address.Host == "127.0.0.1" && address.Port == 8090 && !address.Six && IsLoopback(address));
        CHECK(ParseWebAddress("8091", address, problem) && address.Host == "127.0.0.1" && address.Port == 8091);
        CHECK(ParseWebAddress("localhost:9000", address, problem) && address.Host == "127.0.0.1" && address.Port == 9000);
        CHECK(ParseWebAddress("127.5.6.7:80", address, problem) && IsLoopback(address) && !IsWildcard(address));
        CHECK(ParseWebAddress("0.0.0.0:8090", address, problem) && !IsLoopback(address) && IsWildcard(address));
        CHECK(ParseWebAddress("192.168.1.5", address, problem) && address.Port == 8090 && !IsLoopback(address) && !IsWildcard(address));
        CHECK(ParseWebAddress("[::1]:8090", address, problem) && address.Six && IsLoopback(address) && AddressText(address) == "[::1]:8090");
        CHECK(ParseWebAddress("[::]", address, problem) && address.Six && address.Port == 8090 && IsWildcard(address) && !IsLoopback(address));
        CHECK((AddressText(WebAddress{ "10.0.0.2", 8080, false }) == "10.0.0.2:8080"));
        CHECK(!ParseWebAddress("::1", address, problem) && !problem.empty());
        CHECK(!ParseWebAddress("host.example:8090", address, problem));
        CHECK(!ParseWebAddress("127.0.0.1:0", address, problem));
        CHECK(!ParseWebAddress("127.0.0.1:65536", address, problem));
        CHECK(!ParseWebAddress("127.0.0.1:", address, problem));
        CHECK(!ParseWebAddress("127.0.0.1:80x", address, problem));
        CHECK(!ParseWebAddress("999.1.1.1:80", address, problem));
        CHECK(!ParseWebAddress("[::1", address, problem));
        CHECK(!ParseWebAddress("[::1]x", address, problem));
        CHECK(!ParseWebAddress("[127.0.0.1]:80", address, problem));
        CHECK(PageAddress("127.0.0.1:8090", "abc") == "http://127.0.0.1:8090/?token=abc");
    }

    void GuardChecks()
    {
        CHECK(HostName("LocalHost:8090") == "localhost");
        CHECK(HostName("[::1]:8090") == "[::1]");
        CHECK(HostName("example") == "example");
        CHECK(HostName("a:b").empty());
        CHECK(HostName("a:1:2").empty());
        CHECK(HostName("[::1]x").empty());
        CHECK(IsIpLiteral("127.0.0.1") && IsIpLiteral("[::1]") && IsIpLiteral("[fe80::1]"));
        CHECK(!IsIpLiteral("::1") && !IsIpLiteral("127.1") && !IsIpLiteral("localhost") && !IsIpLiteral("127.0.0.1.nip.io"));

        const std::vector<std::string> none;
        CHECK(HostAllowed("127.0.0.1:8090", none));
        CHECK(HostAllowed("localhost:8090", none));
        CHECK(HostAllowed("LOCALHOST", none));
        CHECK(HostAllowed("localhost:9000", none));
        CHECK(HostAllowed("[::1]:8090", none));
        CHECK(HostAllowed("192.168.1.5:8090", none));
        CHECK(!HostAllowed("", none));
        CHECK(!HostAllowed("evil.example:8090", none));
        CHECK(!HostAllowed("127.0.0.1.nip.io:8090", none));
        CHECK(!HostAllowed("localhost.evil.example", none));
        CHECK(!HostAllowed("foo.localhost:8090", none));
        CHECK(!HostAllowed("[::1", none));
        CHECK(!HostAllowed("myserver:8090", none));
        const std::vector<std::string> names{ "myserver", "myserver.local" };
        CHECK(HostAllowed("MyServer:8090", names));
        CHECK(HostAllowed("myserver.local:8090", names));
        CHECK(!HostAllowed("myserver.evil.example:8090", names));

        const auto origin = [](std::vector<std::pair<std::string, std::string>> headers) { return OriginAllowed(Asked("POST", "/api", std::move(headers))); };
        CHECK(origin({ { "host", "127.0.0.1:8090" } }));
        CHECK(origin({ { "host", "127.0.0.1:8090" }, { "origin", "http://127.0.0.1:8090" } }));
        CHECK(origin({ { "host", "LocalHost:8090" }, { "origin", "http://localhost:8090" } }));
        CHECK(!origin({ { "host", "127.0.0.1:8090" }, { "origin", "http://localhost:8090" } }));
        CHECK(!origin({ { "host", "127.0.0.1:8090" }, { "origin", "http://127.0.0.1:3000" } }));
        CHECK(!origin({ { "host", "127.0.0.1:8090" }, { "origin", "https://127.0.0.1:8090" } }));
        CHECK(!origin({ { "host", "127.0.0.1:8090" }, { "origin", "null" } }));
        CHECK(!origin({ { "origin", "http://127.0.0.1:8090" } }));
        CHECK(origin({ { "host", "127.0.0.1:8090" }, { "sec-fetch-site", "same-origin" } }));
        CHECK(origin({ { "host", "127.0.0.1:8090" }, { "sec-fetch-site", "none" } }));
        CHECK(!origin({ { "host", "127.0.0.1:8090" }, { "sec-fetch-site", "same-site" } }));
        CHECK(!origin({ { "host", "127.0.0.1:8090" }, { "sec-fetch-site", "cross-site" } }));

        CHECK(TokenMatches("0123456789abcdef", "0123456789abcdef"));
        CHECK(!TokenMatches("0123456789abcdef", "0123456789abcdee"));
        CHECK(!TokenMatches("0123456789abcdef", "0123456789abcde"));
        CHECK(!TokenMatches("0123456789abcdef", ""));
        CHECK(!TokenMatches("", ""));

        CHECK(WebAllows("status") && WebAllows("add") && WebAllows("slots") && WebAllows("log"));
        CHECK(WebAllows("library") && WebAllows("game") && WebAllows("stats") && WebAllows("history") && WebAllows("settings") && WebAllows("set"));
        CHECK(WebAllows("move") && WebAllows("sort") && WebAllows("retry") && WebAllows("now") && WebAllows("refund") && WebAllows("clear") && WebAllows("undo") && WebAllows("list"));
        CHECK(!WebAllows("quit") && !WebAllows("web") && !WebAllows("") && !WebAllows("Status"));
    }

    void TokenChecks()
    {
        std::string first;
        std::string second;
        CHECK(RandomHex(16, first) && RandomHex(16, second) && first.size() == 32 && first != second);
        CHECK(first.find_first_not_of("0123456789abcdef") == std::string::npos);
        CHECK(IsUsableToken(first));
        CHECK(IsUsableToken("Abc-def_0123456789"));
        CHECK(!IsUsableToken("short"));
        CHECK(!IsUsableToken("0123456789abcdef 0123"));
        CHECK(!IsUsableToken("0123456789abcdef%41"));
        CHECK(!IsUsableToken(std::string(257, 'a')));
        CHECK(WebTokenFile("a/idler.ini") == std::filesystem::path("a/idler.web-token"));

        std::error_code error;
        const std::filesystem::path folder = std::filesystem::temp_directory_path(error) / ("idler-tests-" + first);
        std::filesystem::create_directories(folder, error);
        const std::filesystem::path file = folder / "idler.web-token";
        std::string token;
        std::string problem;
        CHECK(LoadWebToken(file, token, problem) && IsUsableToken(token) && std::filesystem::exists(file));
        std::string again;
        CHECK(LoadWebToken(file, again, problem) && again == token);
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << "  my-own-token-0123456789\r\n";
        }
        CHECK(LoadWebToken(file, again, problem) && again == "my-own-token-0123456789");
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << "bad token!";
        }
        problem.clear();
        CHECK(!LoadWebToken(file, again, problem) && !problem.empty());
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
        }
        CHECK(LoadWebToken(file, again, problem) && IsUsableToken(again) && again != token);
        std::filesystem::remove(file, error);
        std::filesystem::remove(folder, error);
    }

    void AnswerChecks()
    {
        WebConfig config;
        config.Token = "0123456789abcdef0123456789abcdef";
        config.Names = { "myserver" };
        std::vector<std::vector<std::string>> asked;
        const CommandRunner run = [&](const std::vector<std::string>& words)
            {
                asked.push_back(words);
                CommandResult result;
                result.Text = "fine";
                result.Columns = { "a" };
                result.Rows = { { "1" } };
                return result;
            };
        const std::pair<std::string, std::string> host{ "host", "127.0.0.1:8090" };
        const std::pair<std::string, std::string> key{ "x-idler-token", config.Token };
        const std::pair<std::string, std::string> json{ "content-type", "application/json" };
        const auto answer = [&](const HttpRequest& request) { return AnswerWeb(request, config, run); };

        const HttpResponse locked = answer(Asked("GET", "/", { host }));
        CHECK(locked.Status == 401 && locked.Body == ErrorMessage(ErrorCode::WrongToken) + "\n");
        CHECK(answer(Asked("GET", "/?token=0123456789abcdef0123456789abcdee", { host })).Status == 401);

        const HttpResponse page = answer(Asked("GET", "/?token=" + config.Token, { host, { "sec-fetch-site", "none" } }));
        CHECK(page.Status == 200 && page.Type.rfind("text/html", 0) == 0);
        const std::string* policy = page.Header("Content-Security-Policy");
        CHECK(policy != nullptr);
        const std::string nonce = policy ? NonceIn(*policy) : std::string();
        CHECK(nonce.size() == 32);
        CHECK(policy && policy->find("style-src 'nonce-" + nonce + "'") != std::string::npos);
        CHECK(policy && policy->find("connect-src 'self'") != std::string::npos && policy->find("frame-ancestors 'none'") != std::string::npos);
        CHECK(policy && policy->find("style-src 'nonce-" + nonce + "' 'self';") != std::string::npos);
        CHECK(policy && policy->find("font-src data: 'self';") != std::string::npos);
        CHECK(page.Body.find("<script nonce=\"" + nonce + "\">") != std::string::npos);
        CHECK(page.Body.find("<style nonce=\"" + nonce + "\">") != std::string::npos);
        CHECK(page.Body.find("{{nonce}}") == std::string::npos);
        CHECK(page.Header("X-Frame-Options") && *page.Header("X-Frame-Options") == "DENY");
        CHECK(page.Header("Cache-Control") && page.Header("Referrer-Policy") && page.Header("X-Content-Type-Options"));
        CHECK(!page.Header("Access-Control-Allow-Origin"));
        const HttpResponse other = answer(Asked("GET", "/", { host, key }));
        CHECK(other.Status == 200 && other.Header("Content-Security-Policy") && NonceIn(*other.Header("Content-Security-Policy")) != nonce);

        CHECK(answer(Asked("GET", "/?token=" + config.Token, { { "host", "evil.example:8090" } })).Status == 403);
        CHECK(answer(Asked("GET", "/?token=" + config.Token, { { "host", "myserver:8090" } })).Status == 200);
        CHECK(answer(Asked("GET", "/?token=" + config.Token, {})).Status == 403);
        CHECK(answer(Asked("GET", "/?token=" + config.Token, { host, { "sec-fetch-site", "cross-site" } })).Status == 403);
        const HttpResponse preflight = answer(Asked("OPTIONS", "/api", { host, { "origin", "http://evil.example" } }));
        CHECK(preflight.Status == 403 && !preflight.Header("Access-Control-Allow-Origin"));
        const HttpResponse options = answer(Asked("OPTIONS", "/api", { host, key }));
        CHECK(options.Status == 405 && options.Header("Allow") && !options.Header("Access-Control-Allow-Origin"));
        CHECK(answer(Asked("HEAD", "/", { host, key })).Status == 405);

        CHECK(answer(Asked("POST", "/api", { host, json }, "[\"status\"]")).Status == 401);
        CHECK(asked.empty());
        const HttpResponse status = answer(Asked("POST", "/api", { host, key, json, { "origin", "http://127.0.0.1:8090" } }, "[\"status\"]"));
        CHECK(status.Status == 200 && status.Body == "{\"ok\":true,\"text\":\"fine\",\"columns\":[\"a\"],\"rows\":[[\"1\"]]}");
        CHECK(status.Type.rfind("application/json", 0) == 0);
        CHECK(asked.size() == 1 && asked[0] == std::vector<std::string>{ "status" });
        CHECK(answer(Asked("POST", "/api?token=" + config.Token, { host, { "content-type", "application/json; charset=utf-8" } }, "[\"queue\"]")).Status == 200);
        CHECK(asked.size() == 2 && asked[1] == std::vector<std::string>{ "queue" });

        asked.clear();
        CHECK(answer(Asked("POST", "/api", { host, key, json, { "origin", "http://evil.example" } }, "[\"stop\"]")).Status == 403);
        CHECK(answer(Asked("POST", "/api", { { "host", "evil.example:8090" }, key, json }, "[\"stop\"]")).Status == 403);
        CHECK(answer(Asked("POST", "/api", { host, key, { "content-type", "text/plain" } }, "[\"stop\"]")).Status == 415);
        CHECK(answer(Asked("POST", "/api", { host, key }, "[\"stop\"]")).Status == 415);
        const HttpResponse refused = answer(Asked("POST", "/api", { host, key, json }, "[\"quit\"]"));
        CHECK(refused.Status == 403 && refused.Body == "{\"ok\":false,\"text\":\"Not from the web page: quit (MI-7014)\",\"code\":\"MI-7014\"}");
        CHECK(answer(Asked("POST", "/api", { host, key, json }, "stop")).Status == 400);
        CHECK(answer(Asked("POST", "/api", { host, key, json }, "[]")).Status == 400);
        CHECK(answer(Asked("GET", "/api", { host, key })).Status == 405);
        CHECK(answer(Asked("POST", "/", { host, key, json }, "[\"stop\"]")).Status == 405);
        CHECK(asked.empty());

        CHECK(answer(Asked("GET", "/nothing", { host, key })).Status == 404);
        CHECK(answer(Asked("GET", "/nothing", { host })).Status == 401);
        CHECK(PlainResponse(400, "x").Header("X-Content-Type-Options") != nullptr);

        const std::string html = WebPage("abc", LoadThemes({}));
        CHECK(html.find("<script nonce=\"abc\">") != std::string::npos && html.find("<style nonce=\"abc\">") != std::string::npos);
        CHECK(html.find("src=\"http") == std::string::npos && html.find("href=\"http") == std::string::npos);
        CHECK(html.find("@import") == std::string::npos);
        CHECK(html.find("url(") == std::string::npos && html.find("font-family:\"Idler") == std::string::npos);
        CHECK(html.find("document.write('<link id=\"theme-sheet\" rel=\"stylesheet\" href=\"/themes/' + token + \"/\" + theme.id + '/theme.css\">')") != std::string::npos);
        CHECK(html.find("<script type=\"application/json\" id=\"themes\">{\"default\":\"arcade\",") != std::string::npos);
        CHECK(html.find("<button id=\"look\"") != std::string::npos);
        CHECK(html.find("<img class=\"logo\" src=\"data:image/png;base64,") != std::string::npos && html.find("<link rel=\"icon\" href=\"data:image/png;base64,") != std::string::npos);
        for (const char* view : { "idling", "library", "statistics", "log", "settings" })
            CHECK(html.find(std::string("<a href=\"#") + view + "\" data-page=\"" + view + "\"") != std::string::npos);

        const std::filesystem::path picture = std::filesystem::temp_directory_path() / "manifold-idler-cover-test.jpg";
        {
            std::ofstream out(picture, std::ios::binary | std::ios::trunc);
            out << "\xFF\xD8\xFF picture";
        }
        config.Cover = [&](std::uint32_t appId) { return appId == 620 ? picture : std::filesystem::path(); };
        const HttpResponse coverResponse = answer(Asked("GET", "/cover/620?token=" + config.Token, { host, { "sec-fetch-site", "same-origin" } }));
        CHECK(coverResponse.Status == 200 && coverResponse.Type == "image/jpeg" && coverResponse.Body == "\xFF\xD8\xFF picture");
        CHECK(coverResponse.Header("Cache-Control") && coverResponse.Header("Cache-Control")->find("max-age") != std::string::npos);
        CHECK(answer(Asked("GET", "/cover/621?token=" + config.Token, { host })).Status == 404);
        CHECK(answer(Asked("GET", "/cover/abc?token=" + config.Token, { host })).Status == 404);
        CHECK(answer(Asked("GET", "/cover/620", { host })).Status == 401);
        CHECK(answer(Asked("POST", "/cover/620?token=" + config.Token, { host })).Status == 405);
        CHECK(answer(Asked("GET", "/cover/620?token=" + config.Token, { host, { "sec-fetch-site", "cross-site" } })).Status == 403);
        config.Cover = [&](std::uint32_t) { return picture.parent_path() / "manifold-idler-cover-test.txt"; };
        CHECK(answer(Asked("GET", "/cover/620?token=" + config.Token, { host })).Status == 404);
        std::error_code ignored;
        std::filesystem::remove(picture, ignored);
        const HttpResponse again = answer(Asked("GET", "/?token=" + config.Token, { host }));
        const std::string* pagePolicy = again.Header("Content-Security-Policy");
        CHECK(pagePolicy && pagePolicy->find("img-src data: 'self';") != std::string::npos);
        CHECK(html.find(" style=\"") == std::string::npos && html.find("innerHTML") == std::string::npos);
        CHECK(html.find("{{") == std::string::npos);
        CHECK(html.find("new Error(\"The idler does not answer. It may have ended; this page keeps trying. (MI-7015)\")") != std::string::npos);
        CHECK(html.find("new Error(\"The idler gave an answer the page cannot read (HTTP {}). (MI-7016)\".replace(\"{}\", String(response.status)))") != std::string::npos);
        CHECK(html.find("showBanner(\"" + ErrorMessage(ErrorCode::WrongToken) + "\")") != std::string::npos);
    }
}

int main()
{
    const std::vector<std::string> words{ "games", "50", "half life" };
    CHECK(SplitRequest(JoinRequest(words)) == words);
    const std::vector<std::string> withEmpty{ "games", "50", "" };
    CHECK(SplitRequest(JoinRequest(withEmpty)) == withEmpty);
    CHECK(SplitRequest("").empty());
    CHECK(SplitRequest("status") == std::vector<std::string>{ "status" });
    const std::vector<std::string> sneaky{ "use", "a\tb\nc" };
    const std::vector<std::string> cleaned{ "use", "a b c" };
    CHECK(SplitRequest(JoinRequest(sneaky)) == cleaned);

    bool ok = false;
    std::string text;
    CHECK(ReadReply(MakeReply(true, "fine\nsecond"), ok, text) && ok && text == "fine\nsecond");
    CHECK(ReadReply(MakeReply(false, "no"), ok, text) && !ok && text == "no");
    CHECK(ReadReply(MakeReply(true, ""), ok, text) && ok && text.empty());
    CHECK(!ReadReply("", ok, text));
    CHECK(!ReadReply("ok", ok, text));
    CHECK(!ReadReply("maybe\nfine", ok, text));

    float hours = 0.0f;
    CHECK(ParseHours("3", hours) && hours == 3.0f);
    CHECK(ParseHours("1.5", hours) && hours == 1.5f);
    CHECK(ParseHours("1,5", hours) && hours == 1.5f);
    CHECK(!ParseHours("", hours));
    CHECK(!ParseHours("0", hours));
    CHECK(!ParseHours("-1", hours));
    CHECK(!ParseHours("3h", hours));
    CHECK(!ParseHours("nan", hours));
    CHECK(!ParseHours("inf", hours));
    CHECK(!ParseHours("9999999", hours));

    Goal goal;
    std::string problem;
    CHECK(ParseGoal({ "goal", "620", "none" }, 2, goal, problem) && goal.Kind == GoalKind::None);
    CHECK(ParseGoal({ "goal", "620", "for", "3" }, 2, goal, problem) && goal.Kind == GoalKind::ForHours && goal.Hours == 3.0f);
    CHECK(ParseGoal({ "until", "100,5" }, 0, goal, problem) && goal.Kind == GoalKind::UntilTotal && goal.Hours == 100.5f);
    CHECK(!ParseGoal({ "goal", "620" }, 2, goal, problem));
    CHECK(!ParseGoal({ "goal", "620", "for" }, 2, goal, problem));
    CHECK(!ParseGoal({ "goal", "620", "for", "soon" }, 2, goal, problem));
    CHECK(!ParseGoal({ "goal", "620", "none", "3" }, 2, goal, problem));
    CHECK(!ParseGoal({ "goal", "620", "cards" }, 2, goal, problem));

    CHECK(SocketName("a/idler.ini") == SocketName("a/idler.ini"));
    CHECK(SocketName("a/idler.ini") != SocketName("b/idler.ini"));
    CHECK(SocketName("a/idler.ini").size() < 50);
    CHECK(SocketFile("a/idler.ini").filename().string().find(SocketName("a/idler.ini")) != std::string::npos);

    CHECK(QuoteForUnit("/home/x/idler") == "\"/home/x/idler\"");
    CHECK(QuoteForUnit("/home/a b/100%/$HOME/\"q\"\\") == "\"/home/a b/100%%/$$HOME/\\\"q\\\"\\\\\"");
    CHECK(ServiceName("/home/x/.config/manifold-idler/idler.ini", "/home/x/.config/manifold-idler/idler.ini") == "manifold-idler");
    CHECK(ServiceName("/home/x/.config/manifold-idler/./idler.ini", "/home/x/.config/manifold-idler/idler.ini") == "manifold-idler");
    CHECK(ServiceName("/srv/night shift.ini", "/home/x/.config/manifold-idler/idler.ini") == "manifold-idler-night-shift");
    const std::string unit = ServiceUnit("/home/x/bin/idler", "/home/x/.config/manifold-idler/idler.ini", false);
    CHECK(unit.find("ExecStart=\"/home/x/bin/idler\" --settings \"/home/x/.config/manifold-idler/idler.ini\" serve\n") != std::string::npos);
    CHECK(unit.find("Restart=on-failure\n") != std::string::npos);
    CHECK(unit.find("WantedBy=default.target\n") != std::string::npos);
    CHECK(ServiceUnit("/home/x/bin/idler", "/home/x/idler.ini", true).find(" serve --start\n") != std::string::npos);
    CHECK(ServiceUnit("/home/x/bin/idler", "/home/x/idler.ini", true, "127.0.0.1:8090").find(" serve --start --web 127.0.0.1:8090\n") != std::string::npos);
    CHECK(ServiceUnit("/home/x/bin/idler", "/home/x/idler.ini", false, "[::1]:8091").find(" serve --web [::1]:8091\n") != std::string::npos);

    PageSourceChecks();
    HttpChecks();
    JsonChecks();
    AddressChecks();
    GuardChecks();
    TokenChecks();
    AnswerChecks();
    ThemeChecks();

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
