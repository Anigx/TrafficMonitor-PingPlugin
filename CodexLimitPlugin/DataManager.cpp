#include "pch.h"
#include "DataManager.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    constexpr unsigned long long CODEX_QUERY_INTERVAL_MS{ 5ULL * 60ULL * 1000ULL };

    struct RateLimitWindowData
    {
        int used_percent{ -1 };
        long long resets_at{};
    };

    struct RateLimitData
    {
        RateLimitWindowData primary;
        RateLimitWindowData secondary;
    };

    int ClampInt(int value, int min_value, int max_value)
    {
        if (value < min_value)
            return min_value;
        if (value > max_value)
            return max_value;
        return value;
    }

    void WritePrivateProfileInt(const wchar_t* app_name, const wchar_t* key_name, int value, const wchar_t* file_path)
    {
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"%d", value);
        WritePrivateProfileStringW(app_name, key_name, buffer, file_path);
    }

    std::wstring Trim(const std::wstring& value)
    {
        const size_t first{ value.find_first_not_of(L" \t\r\n") };
        if (first == std::wstring::npos)
            return L"";
        const size_t last{ value.find_last_not_of(L" \t\r\n") };
        return value.substr(first, last - first + 1);
    }

    int ReadPercent(const wchar_t* key_name, int current_value, const std::wstring& file_path)
    {
        wchar_t buffer[32]{};
        GetPrivateProfileStringW(L"limits", key_name, L"", buffer, ARRAYSIZE(buffer), file_path.c_str());

        const std::wstring text{ Trim(buffer) };
        if (text.empty())
            return current_value;

        wchar_t* end{};
        const long value{ wcstol(text.c_str(), &end, 10) };
        if (end == text.c_str())
            return current_value;

        return ClampInt(static_cast<int>(value), 0, 100);
    }

    std::wstring ReadString(const wchar_t* key_name, const std::wstring& current_value, const std::wstring& file_path)
    {
        wchar_t buffer[128]{};
        GetPrivateProfileStringW(L"limits", key_name, current_value.c_str(), buffer, ARRAYSIZE(buffer), file_path.c_str());
        return Trim(buffer);
    }

    bool ParseLocalTime(const std::wstring& value, SYSTEMTIME& local_time, unsigned long long& file_time_value)
    {
        std::wstring normalized{ Trim(value) };
        if (normalized.empty())
            return false;

        const bool german_date{ normalized.find(L'.') != std::wstring::npos };
        std::replace(normalized.begin(), normalized.end(), L'T', L' ');
        std::replace(normalized.begin(), normalized.end(), L'/', L'-');
        std::replace(normalized.begin(), normalized.end(), L'.', L'-');

        int first{};
        int second{};
        int third{};
        int hour{};
        int minute{};
        int second_value{};
        const int parsed{ swscanf_s(normalized.c_str(), L"%d-%d-%d %d:%d:%d", &first, &second, &third, &hour, &minute, &second_value) };
        if (parsed < 5)
            return false;

        SYSTEMTIME parsed_local{};
        if (german_date)
        {
            parsed_local.wDay = static_cast<WORD>(first);
            parsed_local.wMonth = static_cast<WORD>(second);
            parsed_local.wYear = static_cast<WORD>(third);
        }
        else
        {
            parsed_local.wYear = static_cast<WORD>(first);
            parsed_local.wMonth = static_cast<WORD>(second);
            parsed_local.wDay = static_cast<WORD>(third);
        }
        parsed_local.wHour = static_cast<WORD>(hour);
        parsed_local.wMinute = static_cast<WORD>(minute);
        parsed_local.wSecond = static_cast<WORD>(parsed >= 6 ? second_value : 0);

        SYSTEMTIME utc_time{};
        FILETIME file_time{};
        if (!TzSpecificLocalTimeToSystemTime(nullptr, &parsed_local, &utc_time))
            return false;
        if (!SystemTimeToFileTime(&utc_time, &file_time))
            return false;

        ULARGE_INTEGER file_time_integer{};
        file_time_integer.LowPart = file_time.dwLowDateTime;
        file_time_integer.HighPart = file_time.dwHighDateTime;

        local_time = parsed_local;
        file_time_value = file_time_integer.QuadPart;
        return true;
    }

    unsigned long long GetCurrentFileTimeValue()
    {
        FILETIME file_time{};
        GetSystemTimeAsFileTime(&file_time);

        ULARGE_INTEGER file_time_integer{};
        file_time_integer.LowPart = file_time.dwLowDateTime;
        file_time_integer.HighPart = file_time.dwHighDateTime;
        return file_time_integer.QuadPart;
    }

    std::wstring FormatLocalTime(const SYSTEMTIME& time)
    {
        wchar_t buffer[32]{};
        swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
        return buffer;
    }

    std::wstring FormatRemaining(unsigned long long reset_file_time)
    {
        const unsigned long long now{ GetCurrentFileTimeValue() };
        if (reset_file_time <= now)
            return L"reset due";

        const unsigned long long total_seconds{ (reset_file_time - now) / 10000000ULL };
        const unsigned long long hours{ total_seconds / 3600ULL };
        const unsigned long long minutes{ (total_seconds % 3600ULL) / 60ULL };
        const unsigned long long seconds{ total_seconds % 60ULL };

        wchar_t buffer[32]{};
        swprintf_s(buffer, L"%llu:%02llu:%02llu", hours, minutes, seconds);
        return buffer;
    }

    std::wstring FormatRemainingHours(unsigned long long reset_file_time)
    {
        const unsigned long long now{ GetCurrentFileTimeValue() };
        if (reset_file_time <= now)
            return L"0.0h";

        const double hours{ static_cast<double>(reset_file_time - now) / 10000000.0 / 3600.0 };
        wchar_t buffer[32]{};
        swprintf_s(buffer, L"%.1fh", hours);
        return buffer;
    }

    std::wstring FormatResetTextFromLocalTime(SYSTEMTIME local_time)
    {
        SYSTEMTIME utc_time{};
        FILETIME file_time{};
        if (!TzSpecificLocalTimeToSystemTime(nullptr, &local_time, &utc_time) || !SystemTimeToFileTime(&utc_time, &file_time))
            return L"";

        ULARGE_INTEGER file_time_integer{};
        file_time_integer.LowPart = file_time.dwLowDateTime;
        file_time_integer.HighPart = file_time.dwHighDateTime;

        if (file_time_integer.QuadPart <= GetCurrentFileTimeValue())
        {
            file_time_integer.QuadPart += 24ULL * 60ULL * 60ULL * 10000000ULL;
            file_time.dwLowDateTime = file_time_integer.LowPart;
            file_time.dwHighDateTime = file_time_integer.HighPart;
            FileTimeToSystemTime(&file_time, &utc_time);
            SystemTimeToTzSpecificLocalTime(nullptr, &utc_time, &local_time);
        }

        return FormatLocalTime(local_time);
    }

    bool EpochSecondsToLocalTime(long long epoch_seconds, SYSTEMTIME& local_time)
    {
        constexpr unsigned long long epoch_offset_seconds{ 11644473600ULL };
        if (epoch_seconds <= 0)
            return false;

        ULARGE_INTEGER file_time_value{};
        file_time_value.QuadPart = (static_cast<unsigned long long>(epoch_seconds) + epoch_offset_seconds) * 10000000ULL;

        FILETIME utc_file_time{};
        utc_file_time.dwLowDateTime = file_time_value.LowPart;
        utc_file_time.dwHighDateTime = file_time_value.HighPart;

        SYSTEMTIME utc_time{};
        if (!FileTimeToSystemTime(&utc_file_time, &utc_time))
            return false;
        return SystemTimeToTzSpecificLocalTime(nullptr, &utc_time, &local_time) != 0;
    }

    std::wstring FormatResetTextFromEpoch(long long epoch_seconds)
    {
        SYSTEMTIME local_time{};
        if (!EpochSecondsToLocalTime(epoch_seconds, local_time))
            return L"";
        return FormatLocalTime(local_time);
    }

    int MonthFromName(std::wstring month)
    {
        for (wchar_t& ch : month)
            ch = static_cast<wchar_t>(towlower(ch));
        if (month.size() > 3)
            month = month.substr(0, 3);

        static const wchar_t* months[]{ L"jan", L"feb", L"mar", L"apr", L"may", L"jun", L"jul", L"aug", L"sep", L"oct", L"nov", L"dec" };
        for (int index = 0; index < ARRAYSIZE(months); ++index)
        {
            if (month == months[index])
                return index + 1;
        }
        return 0;
    }

    bool ParseStatusResetText(const std::wstring& reset_text, std::wstring& normalized_reset)
    {
        SYSTEMTIME now{};
        GetLocalTime(&now);

        int hour{};
        int minute{};
        int day{};
        int year{};
        wchar_t month_text[32]{};

        if (swscanf_s(reset_text.c_str(), L"%d:%d on %d %31ls %d", &hour, &minute, &day, month_text, static_cast<unsigned int>(ARRAYSIZE(month_text)), &year) >= 4)
        {
            const int month{ MonthFromName(month_text) };
            if (month <= 0)
                return false;

            SYSTEMTIME reset_time{};
            reset_time.wYear = static_cast<WORD>(year > 0 ? year : now.wYear);
            reset_time.wMonth = static_cast<WORD>(month);
            reset_time.wDay = static_cast<WORD>(day);
            reset_time.wHour = static_cast<WORD>(hour);
            reset_time.wMinute = static_cast<WORD>(minute);
            reset_time.wSecond = 0;
            normalized_reset = FormatResetTextFromLocalTime(reset_time);
            return !normalized_reset.empty();
        }

        if (swscanf_s(reset_text.c_str(), L"%d:%d", &hour, &minute) >= 2)
        {
            SYSTEMTIME reset_time{ now };
            reset_time.wHour = static_cast<WORD>(hour);
            reset_time.wMinute = static_cast<WORD>(minute);
            reset_time.wSecond = 0;
            reset_time.wMilliseconds = 0;
            normalized_reset = FormatResetTextFromLocalTime(reset_time);
            return !normalized_reset.empty();
        }

        return false;
    }

    bool ReadTextFile(const std::wstring& file_path, std::wstring& content)
    {
        HANDLE file_handle{ CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };
        if (file_handle == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER file_size{};
        if (!GetFileSizeEx(file_handle, &file_size) || file_size.QuadPart <= 0 || file_size.QuadPart > 1024 * 1024)
        {
            CloseHandle(file_handle);
            return false;
        }

        std::string bytes(static_cast<size_t>(file_size.QuadPart), '\0');
        DWORD bytes_read{};
        const BOOL read_success{ ReadFile(file_handle, &bytes[0], static_cast<DWORD>(bytes.size()), &bytes_read, nullptr) };
        CloseHandle(file_handle);
        if (!read_success || bytes_read == 0)
            return false;
        bytes.resize(bytes_read);

        int required_chars{ MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0) };
        UINT code_page{ CP_UTF8 };
        if (required_chars <= 0)
        {
            code_page = CP_ACP;
            required_chars = MultiByteToWideChar(code_page, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        }
        if (required_chars <= 0)
            return false;

        content.assign(required_chars, L'\0');
        MultiByteToWideChar(code_page, 0, bytes.data(), static_cast<int>(bytes.size()), &content[0], required_chars);
        return true;
    }

    bool ReadStatusTextFile(const std::wstring& primary_path, std::wstring& content)
    {
        if (ReadTextFile(primary_path, content))
            return true;

        wchar_t user_profile[MAX_PATH]{};
        const DWORD length{ GetEnvironmentVariableW(L"USERPROFILE", user_profile, ARRAYSIZE(user_profile)) };
        if (length == 0 || length >= ARRAYSIZE(user_profile))
            return false;

        const std::wstring user_status_path{ std::wstring(user_profile) + L"\\.codex\\codex_status.txt" };
        return ReadTextFile(user_status_path, content);
    }

    bool ParseStatusLimitLine(const std::wstring& line, int& percent, std::wstring& reset_text)
    {
        const size_t percent_pos{ line.find(L'%') };
        if (percent_pos == std::wstring::npos)
            return false;

        size_t digit_start{ percent_pos };
        while (digit_start > 0 && iswdigit(line[digit_start - 1]))
            --digit_start;
        if (digit_start == percent_pos)
            return false;

        percent = ClampInt(_wtoi(line.substr(digit_start, percent_pos - digit_start).c_str()), 0, 100);

        const size_t resets_pos{ line.find(L"resets", percent_pos) };
        if (resets_pos == std::wstring::npos)
            return true;

        size_t reset_start{ resets_pos + 6 };
        while (reset_start < line.size() && iswspace(line[reset_start]))
            ++reset_start;

        size_t reset_end{ line.find(L')', reset_start) };
        if (reset_end == std::wstring::npos)
            reset_end = line.size();

        std::wstring normalized_reset;
        if (ParseStatusResetText(Trim(line.substr(reset_start, reset_end - reset_start)), normalized_reset))
            reset_text = normalized_reset;

        return true;
    }

    bool WriteAll(HANDLE handle, const std::string& text)
    {
        DWORD total_written{};
        while (total_written < text.size())
        {
            DWORD written{};
            if (!WriteFile(handle, text.data() + total_written, static_cast<DWORD>(text.size() - total_written), &written, nullptr))
                return false;
            total_written += written;
        }
        return true;
    }

    bool ReadLine(HANDLE handle, std::string& pending, std::string& line, unsigned long long deadline)
    {
        while (GetTickCount64() < deadline)
        {
            const size_t newline_pos{ pending.find('\n') };
            if (newline_pos != std::string::npos)
            {
                line = pending.substr(0, newline_pos);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                pending.erase(0, newline_pos + 1);
                return true;
            }

            DWORD available{};
            if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr))
                return false;

            if (available == 0)
            {
                Sleep(50);
                continue;
            }

            char buffer[4096]{};
            DWORD read{};
            if (!ReadFile(handle, buffer, std::min<DWORD>(available, sizeof(buffer)), &read, nullptr) || read == 0)
                return false;
            pending.append(buffer, read);
        }
        return false;
    }

    bool StartCodexAppServer(PROCESS_INFORMATION& process_info, HANDLE& child_stdin, HANDLE& child_stdout)
    {
        SECURITY_ATTRIBUTES security_attributes{};
        security_attributes.nLength = sizeof(security_attributes);
        security_attributes.bInheritHandle = TRUE;

        HANDLE stdout_read{};
        HANDLE stdout_write{};
        if (!CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0))
            return false;
        SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

        HANDLE stdin_read{};
        HANDLE stdin_write{};
        if (!CreatePipe(&stdin_read, &stdin_write, &security_attributes, 0))
        {
            CloseHandle(stdout_read);
            CloseHandle(stdout_write);
            return false;
        }
        SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW startup_info{};
        startup_info.cb = sizeof(startup_info);
        startup_info.dwFlags = STARTF_USESTDHANDLES;
        startup_info.hStdInput = stdin_read;
        startup_info.hStdOutput = stdout_write;
        startup_info.hStdError = stdout_write;

        wchar_t command_line[] = L"cmd.exe /d /c codex app-server --stdio";
        const BOOL started{ CreateProcessW(nullptr, command_line, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup_info, &process_info) };

        CloseHandle(stdin_read);
        CloseHandle(stdout_write);

        if (!started)
        {
            CloseHandle(stdin_write);
            CloseHandle(stdout_read);
            return false;
        }

        child_stdin = stdin_write;
        child_stdout = stdout_read;
        return true;
    }

    int ExtractIntAfter(const std::string& text, const std::string& key, size_t start)
    {
        size_t pos{ text.find(key, start) };
        if (pos == std::string::npos)
            return -1;
        pos += key.size();

        while (pos < text.size() && (text[pos] == ' ' || text[pos] == ':'))
            ++pos;
        if (pos >= text.size() || !isdigit(static_cast<unsigned char>(text[pos])))
            return -1;

        return std::atoi(text.c_str() + pos);
    }

    long long ExtractInt64After(const std::string& text, const std::string& key, size_t start)
    {
        size_t pos{ text.find(key, start) };
        if (pos == std::string::npos)
            return 0;
        pos += key.size();

        while (pos < text.size() && (text[pos] == ' ' || text[pos] == ':'))
            ++pos;
        if (pos >= text.size() || !isdigit(static_cast<unsigned char>(text[pos])))
            return 0;

        return _strtoi64(text.c_str() + pos, nullptr, 10);
    }

    bool ParseRateLimitsResponse(const std::string& json, RateLimitData& limits)
    {
        const size_t primary_pos{ json.find("\"primary\"") };
        const size_t secondary_pos{ json.find("\"secondary\"", primary_pos == std::string::npos ? 0 : primary_pos) };
        if (primary_pos == std::string::npos || secondary_pos == std::string::npos)
            return false;

        limits.primary.used_percent = ExtractIntAfter(json, "\"usedPercent\"", primary_pos);
        limits.primary.resets_at = ExtractInt64After(json, "\"resetsAt\"", primary_pos);
        limits.secondary.used_percent = ExtractIntAfter(json, "\"usedPercent\"", secondary_pos);
        limits.secondary.resets_at = ExtractInt64After(json, "\"resetsAt\"", secondary_pos);

        return limits.primary.used_percent >= 0 && limits.primary.resets_at > 0 && limits.secondary.used_percent >= 0 && limits.secondary.resets_at > 0;
    }

    bool QueryCodexRateLimits(PendingCodexLimits& result)
    {
        PROCESS_INFORMATION process_info{};
        HANDLE child_stdin{};
        HANDLE child_stdout{};
        if (!StartCodexAppServer(process_info, child_stdin, child_stdout))
        {
            result.error_text = L"Could not start codex app-server.";
            return false;
        }

        const unsigned long long deadline{ GetTickCount64() + 30000ULL };
        std::string pending;
        std::string line;
        bool initialized{};
        bool success{};

        WriteAll(child_stdin, "{\"id\":1,\"method\":\"initialize\",\"params\":{\"clientInfo\":{\"name\":\"codex-limit-plugin\",\"version\":\"1.0\"},\"capabilities\":{\"experimentalApi\":true}}}\n");

        while (ReadLine(child_stdout, pending, line, deadline))
        {
            if (!initialized && line.find("\"id\":1") != std::string::npos)
            {
                WriteAll(child_stdin, "{\"method\":\"initialized\"}\n");
                WriteAll(child_stdin, "{\"id\":2,\"method\":\"account/rateLimits/read\"}\n");
                FlushFileBuffers(child_stdin);
                initialized = true;
                continue;
            }

            if (line.find("\"id\":2") != std::string::npos)
            {
                RateLimitData limits;
                if (ParseRateLimitsResponse(line, limits))
                {
                    result.available = true;
                    result.five_hour_percent = ClampInt(100 - limits.primary.used_percent, 0, 100);
                    result.five_hour_reset = FormatResetTextFromEpoch(limits.primary.resets_at);
                    result.weekly_percent = ClampInt(100 - limits.secondary.used_percent, 0, 100);
                    result.weekly_reset = FormatResetTextFromEpoch(limits.secondary.resets_at);
                    success = true;
                }
                else
                    result.error_text = L"Could not parse codex rate-limit response.";
                break;
            }
        }

        CloseHandle(child_stdin);
        CloseHandle(child_stdout);
        if (WaitForSingleObject(process_info.hProcess, 1000) == WAIT_TIMEOUT)
            TerminateProcess(process_info.hProcess, 0);
        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);

        if (!success && result.error_text.empty())
            result.error_text = L"Codex rate-limit query timed out.";
        return success;
    }

    void UpdateLimitState(CodexLimitState& state, bool include_remaining_hours)
    {
        state.has_reset = ParseLocalTime(state.reset_text, state.reset_local, state.reset_file_time);
        if (state.has_reset)
        {
            state.reset_display = FormatLocalTime(state.reset_local);
            state.remaining_text = FormatRemaining(state.reset_file_time);
        }
        else
        {
            state.reset_display = state.reset_text.empty() ? L"not set" : L"invalid time";
            state.remaining_text = L"not set";
        }

        wchar_t value_text[16]{};
        if (state.percent < 0)
        {
            if (include_remaining_hours)
                swprintf_s(value_text, L"--%% --h");
            else
                swprintf_s(value_text, L"--%%");
        }
        else if (include_remaining_hours)
        {
            swprintf_s(value_text, L"%d%% %s", ClampInt(state.percent, 0, 100), state.has_reset ? FormatRemainingHours(state.reset_file_time).c_str() : L"--h");
        }
        else
            swprintf_s(value_text, L"%d%%", ClampInt(state.percent, 0, 100));
        state.value_text = value_text;
    }
}

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
    InitializeCriticalSection(&m_query_lock);
}

CDataManager::~CDataManager()
{
    EnterCriticalSection(&m_query_lock);
    m_shutdown = true;
    const HANDLE worker_handle{ m_worker_handle };
    LeaveCriticalSection(&m_query_lock);

    if (worker_handle != nullptr)
    {
        WaitForSingleObject(worker_handle, 1000);
        CloseHandle(worker_handle);
    }
    DeleteCriticalSection(&m_query_lock);
}

CDataManager& CDataManager::Instance()
{
    return m_instance;
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
    EnsureConfigPath(config_dir);
    ReadConfigValues();
    ReadStatusFileValues();
    UpdateDisplayText();
}

void CDataManager::SaveConfig() const
{
    if (m_config_path.empty())
        return;

    if (m_five_hour.percent >= 0)
        WritePrivateProfileInt(L"limits", L"five_hour_percent", ClampInt(m_five_hour.percent, 0, 100), m_config_path.c_str());
    else
        WritePrivateProfileStringW(L"limits", L"five_hour_percent", L"", m_config_path.c_str());

    if (m_weekly.percent >= 0)
        WritePrivateProfileInt(L"limits", L"weekly_percent", ClampInt(m_weekly.percent, 0, 100), m_config_path.c_str());
    else
        WritePrivateProfileStringW(L"limits", L"weekly_percent", L"", m_config_path.c_str());

    WritePrivateProfileStringW(L"limits", L"five_hour_reset", m_five_hour.reset_text.c_str(), m_config_path.c_str());
    WritePrivateProfileStringW(L"limits", L"weekly_reset", m_weekly.reset_text.c_str(), m_config_path.c_str());
}

void CDataManager::Refresh()
{
    if (m_config_path.empty())
        EnsureConfigPath(L"");

    const unsigned long long now{ GetTickCount64() };
    if (m_last_query_tick != 0 && now - m_last_query_tick < 1000)
        return;

    if (!m_has_live_codex_data)
    {
        ReadConfigValues();
        ReadStatusFileValues();
    }
    ApplyPendingCodexQueryResult();
    UpdateDisplayText();
    StartCodexQueryIfNeeded(now);
    m_last_query_tick = GetTickCount64();
}

void CDataManager::SetValues(int five_hour_percent, const std::wstring& five_hour_reset, int weekly_percent, const std::wstring& weekly_reset)
{
    m_has_live_codex_data = false;
    m_five_hour.percent = five_hour_percent < 0 ? -1 : ClampInt(five_hour_percent, 0, 100);
    m_five_hour.reset_text = Trim(five_hour_reset);
    m_weekly.percent = weekly_percent < 0 ? -1 : ClampInt(weekly_percent, 0, 100);
    m_weekly.reset_text = Trim(weekly_reset);
    UpdateDisplayText();
    SaveConfig();
}

void CDataManager::ApplyPendingCodexQueryResult()
{
    PendingCodexLimits pending_limits;
    bool has_result{};

    EnterCriticalSection(&m_query_lock);
    if (m_pending_limits.available || !m_pending_limits.error_text.empty())
    {
        pending_limits = m_pending_limits;
        m_pending_limits = PendingCodexLimits{};
        has_result = true;
    }

    if (m_worker_handle != nullptr && WaitForSingleObject(m_worker_handle, 0) == WAIT_OBJECT_0)
    {
        CloseHandle(m_worker_handle);
        m_worker_handle = nullptr;
    }
    LeaveCriticalSection(&m_query_lock);

    if (!has_result)
        return;

    if (pending_limits.available)
    {
        m_five_hour.percent = pending_limits.five_hour_percent;
        m_five_hour.reset_text = pending_limits.five_hour_reset;
        m_weekly.percent = pending_limits.weekly_percent;
        m_weekly.reset_text = pending_limits.weekly_reset;
        m_has_live_codex_data = true;
    }
    else if (!pending_limits.error_text.empty() && !m_has_live_codex_data)
    {
        m_tooltip_text = pending_limits.error_text;
    }
}

void CDataManager::StartCodexQueryIfNeeded(unsigned long long now)
{
    EnterCriticalSection(&m_query_lock);
    const bool should_query{ !m_shutdown && !m_query_running && (m_last_live_query_tick == 0 || now - m_last_live_query_tick >= CODEX_QUERY_INTERVAL_MS) };
    if (!should_query)
    {
        LeaveCriticalSection(&m_query_lock);
        return;
    }

    m_query_running = true;
    m_last_live_query_tick = now;
    HANDLE thread_handle{ CreateThread(nullptr, 0, CodexQueryThreadProc, this, 0, nullptr) };
    if (thread_handle == nullptr)
    {
        m_query_running = false;
        m_pending_limits.error_text = L"Could not start codex query worker.";
        LeaveCriticalSection(&m_query_lock);
        return;
    }
    m_worker_handle = thread_handle;
    LeaveCriticalSection(&m_query_lock);
}

DWORD WINAPI CDataManager::CodexQueryThreadProc(LPVOID parameter)
{
    auto* data_manager{ static_cast<CDataManager*>(parameter) };
    PendingCodexLimits query_result;
    QueryCodexRateLimits(query_result);

    EnterCriticalSection(&data_manager->m_query_lock);
    if (!data_manager->m_shutdown)
        data_manager->m_pending_limits = query_result;
    data_manager->m_query_running = false;
    LeaveCriticalSection(&data_manager->m_query_lock);
    return 0;
}

void CDataManager::EnsureConfigPath(const std::wstring& config_dir)
{
    wchar_t module_path_buffer[MAX_PATH]{};
    GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), module_path_buffer, ARRAYSIZE(module_path_buffer));
    std::wstring module_path{ module_path_buffer };
    m_config_path = module_path;
    const size_t module_dir_index{ module_path.find_last_of(L"\\/") };
    const std::wstring module_dir{ module_dir_index == std::wstring::npos ? L"" : module_path.substr(0, module_dir_index + 1) };
    m_status_path = module_dir + L"codex_status.txt";

    if (!config_dir.empty())
    {
        const size_t index{ module_path.find_last_of(L"\\/") };
        const std::wstring module_file_name{ module_path.substr(index + 1) };
        m_config_path = config_dir + module_file_name;
        m_status_path = config_dir + L"codex_status.txt";
    }
    m_config_path += L".ini";
}

void CDataManager::ReadConfigValues()
{
    if (m_config_path.empty())
        return;

    m_five_hour.percent = ReadPercent(L"five_hour_percent", m_five_hour.percent, m_config_path);
    m_weekly.percent = ReadPercent(L"weekly_percent", m_weekly.percent, m_config_path);
    m_five_hour.reset_text = ReadString(L"five_hour_reset", m_five_hour.reset_text, m_config_path);
    m_weekly.reset_text = ReadString(L"weekly_reset", m_weekly.reset_text, m_config_path);
}

void CDataManager::ReadStatusFileValues()
{
    if (m_status_path.empty())
        return;

    std::wstring content;
    if (!ReadStatusTextFile(m_status_path, content))
        return;

    size_t line_start{};
    while (line_start < content.size())
    {
        size_t line_end{ content.find_first_of(L"\r\n", line_start) };
        if (line_end == std::wstring::npos)
            line_end = content.size();

        const std::wstring line{ content.substr(line_start, line_end - line_start) };
        if (line.find(L"5h limit:") != std::wstring::npos)
            ParseStatusLimitLine(line, m_five_hour.percent, m_five_hour.reset_text);
        else if (line.find(L"Weekly limit:") != std::wstring::npos)
            ParseStatusLimitLine(line, m_weekly.percent, m_weekly.reset_text);

        line_start = line_end + 1;
        while (line_start < content.size() && (content[line_start] == L'\r' || content[line_start] == L'\n'))
            ++line_start;
    }
}

void CDataManager::UpdateDisplayText()
{
    UpdateLimitState(m_five_hour, true);
    UpdateLimitState(m_weekly, true);

    wchar_t tooltip_text[512]{};
    swprintf_s(
        tooltip_text,
        L"ChatGPT Codex limits\n5-hour limit: %s\n5-hour reset: %s\n5-hour remaining: %s\nWeekly limit: %s\nWeekly reset: %s\nWeekly remaining: %s",
        m_five_hour.value_text.c_str(),
        m_five_hour.reset_display.c_str(),
        m_five_hour.remaining_text.c_str(),
        m_weekly.value_text.c_str(),
        m_weekly.reset_display.c_str(),
        m_weekly.remaining_text.c_str());
    m_tooltip_text = tooltip_text;
}
