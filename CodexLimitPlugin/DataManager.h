#pragma once

#include "framework.h"

#include <string>

struct CodexLimitState
{
    int percent{ -1 };
    std::wstring reset_text;
    bool has_reset{};
    SYSTEMTIME reset_local{};
    unsigned long long reset_file_time{};
    std::wstring value_text{ L"--%" };
    std::wstring reset_display{ L"not set" };
    std::wstring remaining_text{ L"not set" };
};

struct PendingCodexLimits
{
    bool available{};
    int five_hour_percent{ -1 };
    std::wstring five_hour_reset;
    int weekly_percent{ -1 };
    std::wstring weekly_reset;
    std::wstring error_text;
};

class CDataManager
{
private:
    CDataManager();
    ~CDataManager();

public:
    static CDataManager& Instance();

    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig() const;
    void Refresh();
    void SetValues(int five_hour_percent, const std::wstring& five_hour_reset, int weekly_percent, const std::wstring& weekly_reset);

public:
    CodexLimitState m_five_hour;
    CodexLimitState m_weekly;
    std::wstring m_tooltip_text{ L"ChatGPT Codex limit data is not configured." };
    unsigned long long m_last_query_tick{};
    COLORREF m_label_text_color{ RGB(0, 255, 0) };
    COLORREF m_value_text_color{ RGB(0, 255, 0) };

private:
    static CDataManager m_instance;
    std::wstring m_config_path;

    void EnsureConfigPath(const std::wstring& config_dir);
    void ReadConfigValues();
    void ReadStatusFileValues();
    void ApplyPendingCodexQueryResult();
    void StartCodexQueryIfNeeded(unsigned long long now);
    void UpdateDisplayText();
    static DWORD WINAPI CodexQueryThreadProc(LPVOID parameter);

private:
    std::wstring m_status_path;
    CRITICAL_SECTION m_query_lock;
    PendingCodexLimits m_pending_limits;
    HANDLE m_worker_handle{};
    bool m_query_running{};
    bool m_has_live_codex_data{};
    bool m_shutdown{};
    unsigned long long m_last_live_query_tick{};
};
