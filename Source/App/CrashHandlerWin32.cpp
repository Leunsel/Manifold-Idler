#include "App/CrashHandler.h"

#include "Core/CrashReport.h"
#include "Core/Text.h"
#include "Core/Version.h"

#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <typeinfo>
#include <vector>

namespace Idler::CrashHandler
{
    namespace
    {
        constexpr DWORD kReportMilliseconds = 30000;
        constexpr DWORD kAbortCode = 0xE0000001u;
        constexpr DWORD kTerminateCode = 0xE0000002u;
        constexpr DWORD kPureCallCode = 0xE0000003u;
        constexpr DWORD kInvalidParameterCode = 0xE0000004u;
        constexpr DWORD kOnPurposeCode = 0xE0000005u;
        constexpr ULONG kStackGuarantee = 64 * 1024;
        constexpr std::size_t kSymbolNameLength = 1024;

        using WriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION,
            PMINIDUMP_CALLBACK_INFORMATION);
        using SetOptionsFn = DWORD(WINAPI*)(DWORD);
        using InitializeFn = BOOL(WINAPI*)(HANDLE, PCWSTR, BOOL);
        using WalkFn = BOOL(WINAPI*)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64,
            PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
        using FromAddressFn = BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
        using LineFromAddressFn = BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);

        struct Tools
        {
            WriteDumpFn WriteDump = nullptr;
            SetOptionsFn SetOptions = nullptr;
            InitializeFn Initialize = nullptr;
            WalkFn Walk = nullptr;
            PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTable = nullptr;
            PGET_MODULE_BASE_ROUTINE64 ModuleBase = nullptr;
            FromAddressFn FromAddress = nullptr;
            LineFromAddressFn LineFromAddress = nullptr;
        };

        std::filesystem::path g_settings;
        std::filesystem::path g_log;
        Tools g_tools;
        HANDLE g_request = nullptr;
        HANDLE g_done = nullptr;
        std::atomic<bool> g_crashing{ false };
        EXCEPTION_POINTERS* g_pointers = nullptr;
        DWORD g_thread = 0;

        template <typename T>
        T Find(HMODULE module, const char* name)
        {
            return reinterpret_cast<T>(reinterpret_cast<void*>(::GetProcAddress(module, name)));
        }

        bool WriteText(const std::filesystem::path& path, std::string_view text, bool append)
        {
            const HANDLE file = ::CreateFileW(path.c_str(), append ? FILE_APPEND_DATA : GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                append ? OPEN_EXISTING : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                return false;
            DWORD written = 0;
            const BOOL ok = ::WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
            ::CloseHandle(file);
            return ok && written == text.size();
        }

        std::wstring ModulePath(HMODULE module)
        {
            std::wstring path(MAX_PATH, L'\0');
            for (;;)
            {
                const DWORD length = ::GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
                if (length == 0)
                    return {};
                if (length < path.size())
                {
                    path.resize(length);
                    return path;
                }
                path.resize(path.size() * 2);
            }
        }

        std::string LastPart(std::string_view path)
        {
            const std::size_t slash = path.find_last_of("\\/");
            return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
        }

        std::string ModuleOf(DWORD64 address, DWORD64& base)
        {
            HMODULE module = nullptr;
            base = 0;
            if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(address), &module) ||
                !module)
                return {};
            base = reinterpret_cast<DWORD64>(module);
            return LastPart(WideToUtf8(ModulePath(module)));
        }

        std::string LinkedAt()
        {
            const auto base = reinterpret_cast<const unsigned char*>(::GetModuleHandleW(nullptr));
            if (!base)
                return {};
            const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            const std::time_t linked = static_cast<std::time_t>(nt->FileHeader.TimeDateStamp);
            std::tm local{};
            if (!LocalTime(linked, local))
                return {};
            char text[32];
            const std::size_t length = std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M", &local);
            return std::string(text, length);
        }

        struct ThrowInfo
        {
            unsigned Attributes;
            int Unwind;
            int ForwardCompat;
            int CatchableTypes;
        };

        struct CatchableTypeArray
        {
            int Count;
            int Types[1];
        };

        struct CatchableType
        {
            unsigned Properties;
            int Type;
            int ThisDisplacement;
            int BaseDisplacement;
            int VirtualDisplacement;
            int Size;
            int Copy;
        };

        struct TypeDescriptor
        {
            const void* Table;
            void* Spare;
            char Name[1];
        };

        struct CppException
        {
            char Type[256];
            char What[512];
        };

        bool ReadCppException(const EXCEPTION_RECORD& record, CppException& thrown)
        {
            thrown.Type[0] = 0;
            thrown.What[0] = 0;
            if (record.NumberParameters < 4 || (record.ExceptionInformation[0] != 0x19930520u && record.ExceptionInformation[0] != 0x19930521u &&
                                                   record.ExceptionInformation[0] != 0x19930522u))
                return false;
            const auto object = reinterpret_cast<const char*>(record.ExceptionInformation[1]);
            const auto info = reinterpret_cast<const ThrowInfo*>(record.ExceptionInformation[2]);
            const auto base = reinterpret_cast<const char*>(record.ExceptionInformation[3]);
            if (!object || !info || !base)
                return false;
            __try
            {
                const auto types = reinterpret_cast<const CatchableTypeArray*>(base + info->CatchableTypes);
                for (int index = 0; index < types->Count && index < 64; ++index)
                {
                    const auto type = reinterpret_cast<const CatchableType*>(base + types->Types[index]);
                    const auto descriptor = reinterpret_cast<const TypeDescriptor*>(base + type->Type);
                    if (index == 0)
                        ::strncpy_s(thrown.Type, descriptor->Name, _TRUNCATE);
                    if (std::strcmp(descriptor->Name, ".?AVexception@std@@") == 0 && type->BaseDisplacement == -1)
                    {
                        const auto error = reinterpret_cast<const std::exception*>(object + type->ThisDisplacement);
                        ::strncpy_s(thrown.What, error->what(), _TRUNCATE);
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return thrown.Type[0] != 0;
            }
            return thrown.Type[0] != 0;
        }

        std::string TypeName(std::string_view decorated)
        {
            if (decorated.starts_with(".?AV") || decorated.starts_with(".?AU"))
                decorated.remove_prefix(4);
            if (const std::size_t end = decorated.find("@@"); end != std::string_view::npos)
                decorated = decorated.substr(0, end);
            std::string name;
            while (!decorated.empty())
            {
                const std::size_t at = decorated.rfind('@');
                const std::string_view part = at == std::string_view::npos ? decorated : decorated.substr(at + 1);
                name += (name.empty() ? "" : "::") + std::string(part);
                decorated = at == std::string_view::npos ? std::string_view() : decorated.substr(0, at);
            }
            return name;
        }

        std::string WhyTerminated()
        {
            std::string why = CrashReport::ExceptionText(kTerminateCode);
            if (const std::exception_ptr current = std::current_exception())
            {
                why = CrashReport::ExceptionText(0xE06D7363u);
                try
                {
                    std::rethrow_exception(current);
                }
                catch (const std::exception& error)
                {
                    std::string type = typeid(error).name();
                    for (const std::string_view word : { "class ", "struct " })
                    {
                        if (type.starts_with(word))
                            type.erase(0, word.size());
                    }
                    why += ": " + type + ", \"" + error.what() + "\"";
                }
                catch (...)
                {
                }
            }
            return why;
        }

        std::string g_why;

        std::string ProblemOf(const EXCEPTION_RECORD& record)
        {
            std::string text = CrashReport::ExceptionText(record.ExceptionCode);
            if ((record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION || record.ExceptionCode == EXCEPTION_IN_PAGE_ERROR) && record.NumberParameters >= 2)
                text += ", " + CrashReport::AccessText(record.ExceptionInformation[0], record.ExceptionInformation[1]);
            CppException thrown;
            if (record.ExceptionCode == 0xE06D7363u && ReadCppException(record, thrown))
            {
                text += ": " + TypeName(thrown.Type);
                if (thrown.What[0] != 0)
                    text += ", \"" + std::string(thrown.What) + "\"";
            }
            return text;
        }

        CrashReport::Frame FrameAt(HANDLE process, DWORD64 address, bool first)
        {
            CrashReport::Frame frame;
            DWORD64 base = 0;
            frame.Module = ModuleOf(address, base);
            frame.Offset = address - base;
            const DWORD64 lookup = first ? address : address - 1;

            alignas(SYMBOL_INFO) char space[sizeof(SYMBOL_INFO) + kSymbolNameLength];
            std::memset(space, 0, sizeof(space));
            SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(space);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = static_cast<ULONG>(kSymbolNameLength);
            DWORD64 displacement = 0;
            if (g_tools.FromAddress && g_tools.FromAddress(process, lookup, &displacement, symbol))
            {
                frame.Function.assign(symbol->Name, std::min<std::size_t>(symbol->NameLen, kSymbolNameLength - 1));
                frame.Displacement = address - symbol->Address;
            }
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisplacement = 0;
            if (g_tools.LineFromAddress && g_tools.LineFromAddress(process, lookup, &lineDisplacement, &line) && line.FileName)
            {
                frame.File = LastPart(line.FileName);
                frame.Line = line.LineNumber;
            }
            return frame;
        }

        std::vector<CrashReport::Frame> WalkStack(HANDLE process, HANDLE thread, CONTEXT context)
        {
            std::vector<CrashReport::Frame> frames;
            if (g_tools.Walk)
            {
                STACKFRAME64 frame{};
                frame.AddrPC.Offset = context.Rip;
                frame.AddrPC.Mode = AddrModeFlat;
                frame.AddrFrame.Offset = context.Rbp;
                frame.AddrFrame.Mode = AddrModeFlat;
                frame.AddrStack.Offset = context.Rsp;
                frame.AddrStack.Mode = AddrModeFlat;
                while (frames.size() < CrashReport::kMostFrames &&
                    g_tools.Walk(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr, g_tools.FunctionTable, g_tools.ModuleBase, nullptr))
                {
                    if (frame.AddrPC.Offset == 0)
                        break;
                    frames.push_back(FrameAt(process, frame.AddrPC.Offset, frames.empty()));
                }
            }
            if (frames.empty() && context.Rip != 0)
                frames.push_back(FrameAt(process, context.Rip, true));
            return frames;
        }

        void Report()
        {
            const HANDLE process = ::GetCurrentProcess();
            CrashReport::Crash crash;
            crash.Time = std::time(nullptr);
            crash.Thread = g_thread;
            crash.Version = kVersion;
            crash.Program = WideToUtf8(ModulePath(nullptr));
            crash.Built = LinkedAt();
            const EXCEPTION_RECORD* record = g_pointers ? g_pointers->ExceptionRecord : nullptr;
            crash.Problem = !g_why.empty() ? g_why : record ? ProblemOf(*record) : "unknown";

            const std::string name = CrashReport::ReportName(crash.Time);
            const std::filesystem::path folder = CrashReport::FolderOf(g_settings);
            std::error_code error;
            std::filesystem::create_directories(folder, error);

            if (g_tools.WriteDump)
            {
                const std::filesystem::path dump = folder / (name + ".dmp");
                const HANDLE file = ::CreateFileW(dump.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file != INVALID_HANDLE_VALUE)
                {
                    MINIDUMP_EXCEPTION_INFORMATION information{};
                    information.ThreadId = g_thread;
                    information.ExceptionPointers = g_pointers;
                    information.ClientPointers = FALSE;
                    const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
                    const bool written = g_tools.WriteDump(process, ::GetCurrentProcessId(), file, type, g_pointers ? &information : nullptr, nullptr, nullptr) != FALSE;
                    ::CloseHandle(file);
                    if (written)
                        crash.Dump = name + ".dmp";
                    else
                        ::DeleteFileW(dump.c_str());
                }
            }

            if (g_tools.SetOptions && g_tools.Initialize)
            {
                g_tools.SetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
                const std::wstring program = ModulePath(nullptr);
                const std::wstring searchPath = program.substr(0, program.find_last_of(L"\\/"));
                g_tools.Initialize(process, searchPath.c_str(), TRUE);
            }
            if (g_pointers && g_pointers->ContextRecord)
            {
                const HANDLE thread = ::OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, g_thread);
                crash.Stack = WalkStack(process, thread ? thread : ::GetCurrentThread(), *g_pointers->ContextRecord);
                if (thread)
                    ::CloseHandle(thread);
                constexpr std::string_view kOwn = "Idler::CrashHandler::`anonymous namespace'::";
                std::size_t own = 0;
                while (own + 1 < crash.Stack.size() && crash.Stack[own].Function.starts_with(kOwn))
                    ++own;
                crash.Stack.erase(crash.Stack.begin(), crash.Stack.begin() + static_cast<std::ptrdiff_t>(own));
            }

            const std::filesystem::path report = folder / (name + ".txt");
            const bool written = WriteText(report, CrashReport::ReportText(crash), false);
            if (written)
                WriteText(CrashReport::MarkerOf(g_settings), name + "\n", false);
            if (!g_log.empty())
                WriteText(g_log, CrashReport::LogLines(crash, written ? report : std::filesystem::path()), true);
        }

        DWORD WINAPI ReportThread(void*)
        {
            ::WaitForSingleObject(g_request, INFINITE);
            Report();
            ::SetEvent(g_done);
            return 0;
        }

        LONG WINAPI OnUnhandled(EXCEPTION_POINTERS* pointers)
        {
            if (g_crashing.exchange(true))
            {
                ::Sleep(INFINITE);
                return EXCEPTION_CONTINUE_SEARCH;
            }
            g_pointers = pointers;
            g_thread = ::GetCurrentThreadId();
            if (g_request && g_done)
            {
                ::SetEvent(g_request);
                ::WaitForSingleObject(g_done, kReportMilliseconds);
            }
            const UINT code = pointers && pointers->ExceptionRecord ? pointers->ExceptionRecord->ExceptionCode : 1u;
            ::TerminateProcess(::GetCurrentProcess(), code);
            return EXCEPTION_EXECUTE_HANDLER;
        }

        [[noreturn]] void ReportHere(DWORD code)
        {
            CONTEXT context{};
            ::RtlCaptureContext(&context);
            EXCEPTION_RECORD record{};
            record.ExceptionCode = code;
            record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
            record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
            EXCEPTION_POINTERS pointers{};
            pointers.ExceptionRecord = &record;
            pointers.ContextRecord = &context;
            OnUnhandled(&pointers);
            std::_Exit(3);
        }

        void __cdecl OnAbortSignal(int)
        {
            ReportHere(kAbortCode);
        }

        [[noreturn]] void OnTerminate()
        {
            if (!g_crashing.load())
                g_why = WhyTerminated();
            ReportHere(kTerminateCode);
        }

        void __cdecl OnPureCall()
        {
            ReportHere(kPureCallCode);
        }

        void __cdecl OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
        {
            ReportHere(kInvalidParameterCode);
        }

        void Throws()
        {
            throw std::logic_error("thrown through a noexcept function, as a check");
        }

        void Refuses() noexcept
        {
            Throws();
        }

        int Deeper(int depth)
        {
            volatile char pad[4096];
            pad[0] = static_cast<char>(depth);
            if (depth >= 0)
                return Deeper(depth + 1) + pad[0];
            return pad[0];
        }
    }

    void Install(const std::filesystem::path& settings, const std::filesystem::path& log)
    {
        g_settings = settings;
        g_log = log;
        if (const HMODULE help = ::LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
        {
            g_tools.WriteDump = Find<WriteDumpFn>(help, "MiniDumpWriteDump");
            g_tools.SetOptions = Find<SetOptionsFn>(help, "SymSetOptions");
            g_tools.Initialize = Find<InitializeFn>(help, "SymInitializeW");
            g_tools.Walk = Find<WalkFn>(help, "StackWalk64");
            g_tools.FunctionTable = Find<PFUNCTION_TABLE_ACCESS_ROUTINE64>(help, "SymFunctionTableAccess64");
            g_tools.ModuleBase = Find<PGET_MODULE_BASE_ROUTINE64>(help, "SymGetModuleBase64");
            g_tools.FromAddress = Find<FromAddressFn>(help, "SymFromAddr");
            g_tools.LineFromAddress = Find<LineFromAddressFn>(help, "SymGetLineFromAddr64");
        }

        g_request = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        g_done = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (const HANDLE thread = ::CreateThread(nullptr, 0, ReportThread, nullptr, 0, nullptr))
            ::CloseHandle(thread);

        ULONG guarantee = kStackGuarantee;
        ::SetThreadStackGuarantee(&guarantee);
        ::SetUnhandledExceptionFilter(OnUnhandled);
        std::signal(SIGABRT, OnAbortSignal);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        std::set_terminate(OnTerminate);
        _set_purecall_handler(OnPureCall);
        _set_invalid_parameter_handler(OnInvalidParameter);

        CrashReport::Prune(settings);
    }

    void CrashOnPurpose(std::wstring_view how)
    {
        if (how == L"abort")
            std::abort();
        if (how == L"terminate")
            std::terminate();
        if (how == L"throw")
            throw std::runtime_error("a crash on purpose, as a check");
        if (how == L"noexcept")
            Refuses();
        if (how == L"stack")
        {
            Deeper(0);
            return;
        }
        if (how == L"thread")
        {
            std::thread([]()
                {
                    volatile int* nowhere = nullptr;
                    *nowhere = 1;
                }).join();
            return;
        }
        if (how == L"raise")
            ::RaiseException(kOnPurposeCode, EXCEPTION_NONCONTINUABLE, 0, nullptr);
        volatile int* nowhere = nullptr;
        *nowhere = 1;
    }
}
