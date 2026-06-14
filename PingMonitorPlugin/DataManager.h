#pragma once

#include <deque>
#include <string>

struct PingSettings
{
    std::wstring target{ L"1.1.1.1" };
    int timeout_ms{ 1000 };
    int window_size{ 20 };
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
    void ClearSamples();
    void AddPingSample(bool success, unsigned long latency_ms, const std::wstring& status_text);
    double GetPingLossPercent() const;

public:
    PingSettings m_settings;
    std::wstring m_latency_value_text{ L"--ms" };
    std::wstring m_loss_value_text{ L"--%" };
    std::wstring m_tooltip_text{ L"No ping samples yet." };
    std::wstring m_status_text{ L"No data" };
    unsigned long m_latency_ms{};
    bool m_has_data{};
    bool m_last_success{};
    unsigned int m_total_sent{};
    unsigned int m_total_lost{};
    unsigned int m_window_lost{};
    unsigned long long m_last_query_tick{};

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::deque<bool> m_history;
};
