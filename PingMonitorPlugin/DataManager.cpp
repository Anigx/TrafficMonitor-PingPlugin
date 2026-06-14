#include "pch.h"
#include "DataManager.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
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
}

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
}

CDataManager::~CDataManager()
{
    SaveConfig();
}

CDataManager& CDataManager::Instance()
{
    return m_instance;
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
    wchar_t module_path_buffer[MAX_PATH]{};
    GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), module_path_buffer, ARRAYSIZE(module_path_buffer));
    std::wstring module_path{ module_path_buffer };
    m_config_path = module_path;

    if (!config_dir.empty())
    {
        const size_t index{ module_path.find_last_of(L"\\/") };
        const std::wstring module_file_name{ module_path.substr(index + 1) };
        m_config_path = config_dir + module_file_name;
    }
    m_config_path += L".ini";

    wchar_t target[256]{};
    GetPrivateProfileStringW(L"config", L"ping_target", L"1.1.1.1", target, ARRAYSIZE(target), m_config_path.c_str());
    m_settings.target = target;
    if (m_settings.target.empty())
        m_settings.target = L"1.1.1.1";

    m_settings.timeout_ms = static_cast<int>(GetPrivateProfileIntW(L"config", L"ping_timeout", 1000, m_config_path.c_str()));
    m_settings.timeout_ms = ClampInt(m_settings.timeout_ms, 200, 5000);
    m_settings.window_size = static_cast<int>(GetPrivateProfileIntW(L"config", L"ping_window_size", 20, m_config_path.c_str()));
    m_settings.window_size = ClampInt(m_settings.window_size, 5, 100);
}

void CDataManager::SaveConfig() const
{
    if (m_config_path.empty())
        return;

    WritePrivateProfileStringW(L"config", L"ping_target", m_settings.target.c_str(), m_config_path.c_str());
    WritePrivateProfileInt(L"config", L"ping_timeout", m_settings.timeout_ms, m_config_path.c_str());
    WritePrivateProfileInt(L"config", L"ping_window_size", m_settings.window_size, m_config_path.c_str());
}

void CDataManager::ClearSamples()
{
    m_latency_value_text = L"--ms";
    m_loss_value_text = L"--%";
    m_tooltip_text = L"No ping samples yet.";
    m_status_text = L"No data";
    m_latency_ms = 0;
    m_has_data = false;
    m_last_success = false;
    m_total_sent = 0;
    m_total_lost = 0;
    m_window_lost = 0;
    m_last_query_tick = 0;
    m_history.clear();
}

void CDataManager::AddPingSample(bool success, unsigned long latency_ms, const std::wstring& status_text)
{
    m_has_data = true;
    m_last_success = success;
    m_latency_ms = latency_ms;
    m_status_text = status_text;
    ++m_total_sent;
    if (!success)
        ++m_total_lost;

    m_history.push_back(success);
    while (m_history.size() > static_cast<size_t>(m_settings.window_size))
        m_history.pop_front();

    m_window_lost = static_cast<unsigned int>(std::count(m_history.begin(), m_history.end(), false));
    const double loss_percent{ GetPingLossPercent() };

    wchar_t latency_value_text[32]{};
    if (success)
        swprintf_s(latency_value_text, L"%lums", latency_ms);
    else
        swprintf_s(latency_value_text, L"--ms");
    m_latency_value_text = latency_value_text;

    wchar_t loss_value_text[32]{};
    swprintf_s(loss_value_text, L"%.0f%%", loss_percent);
    m_loss_value_text = loss_value_text;

    wchar_t tooltip_text[512]{};
    swprintf_s(
        tooltip_text,
        L"Ping target: %s\nLast result: %s\nRecent loss: %.1f%% (%u/%zu)\nTotal sent: %u\nTotal lost: %u\nTimeout: %d ms",
        m_settings.target.c_str(),
        m_status_text.c_str(),
        loss_percent,
        m_window_lost,
        m_history.size(),
        m_total_sent,
        m_total_lost,
        m_settings.timeout_ms);
    m_tooltip_text = tooltip_text;
}

double CDataManager::GetPingLossPercent() const
{
    if (m_history.empty())
        return 0.0;
    return static_cast<double>(m_window_lost) * 100.0 / static_cast<double>(m_history.size());
}
