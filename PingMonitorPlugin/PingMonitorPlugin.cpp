#include "pch.h"
#include "PingMonitorPlugin.h"
#include "DataManager.h"

#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")

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

    bool EnsureWinsockInitialized()
    {
        static bool initialized = []() -> bool
        {
            WSADATA wsa_data{};
            return WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0;
        }();
        return initialized;
    }

    bool ResolveIPv4Address(const std::wstring& host, IPAddr& address)
    {
        if (!EnsureWinsockInitialized())
            return false;

        ADDRINFOW hints{};
        hints.ai_family = AF_INET;

        ADDRINFOW* result{};
        if (GetAddrInfoW(host.c_str(), nullptr, &hints, &result) != 0 || result == nullptr)
            return false;

        if (result->ai_addr == nullptr)
        {
            FreeAddrInfoW(result);
            return false;
        }

        const sockaddr_in* ipv4_addr{ reinterpret_cast<const sockaddr_in*>(result->ai_addr) };
        address = ipv4_addr->sin_addr.S_un.S_addr;
        FreeAddrInfoW(result);
        return true;
    }

    bool PingHost(const std::wstring& host, unsigned long timeout, unsigned long& latency_ms, std::wstring& status_text)
    {
        IPAddr address{};
        if (!ResolveIPv4Address(host, address))
        {
            status_text = L"Resolve failed";
            return false;
        }

        HANDLE icmp_file{ IcmpCreateFile() };
        if (icmp_file == INVALID_HANDLE_VALUE)
        {
            status_text = L"ICMP unavailable";
            return false;
        }

        char send_data[] = "TrafficMonitor ping";
        const DWORD reply_size{ sizeof(ICMP_ECHO_REPLY) + sizeof(send_data) + 8 };
        std::vector<BYTE> reply_buffer(reply_size);
        const DWORD reply_count{ IcmpSendEcho(icmp_file, address, send_data, sizeof(send_data), nullptr, reply_buffer.data(), reply_size, timeout) };
        IcmpCloseHandle(icmp_file);

        if (reply_count == 0)
        {
            status_text = L"Timeout";
            return false;
        }

        const ICMP_ECHO_REPLY* echo_reply{ reinterpret_cast<const ICMP_ECHO_REPLY*>(reply_buffer.data()) };
        if (echo_reply->Status != IP_SUCCESS)
        {
            status_text = L"Ping failed";
            return false;
        }

        latency_ms = echo_reply->RoundTripTime;
        wchar_t status[64]{};
        swprintf_s(status, L"%lu ms", latency_ms);
        status_text = status;
        return true;
    }

    INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG:
        {
            const auto& settings{ CDataManager::Instance().m_settings };
            SetDlgItemTextW(hDlg, IDC_TARGET_EDIT, settings.target.c_str());
            SetDlgItemInt(hDlg, IDC_TIMEOUT_EDIT, settings.timeout_ms, FALSE);
            SetDlgItemInt(hDlg, IDC_WINDOW_SIZE_EDIT, settings.window_size, FALSE);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDOK:
            {
                wchar_t target[256]{};
                GetDlgItemTextW(hDlg, IDC_TARGET_EDIT, target, ARRAYSIZE(target));
                if (target[0] == L'\0')
                {
                    MessageBoxW(hDlg, L"Please enter a server IP or host name.", L"Ping Monitor", MB_ICONWARNING);
                    return TRUE;
                }

                BOOL timeout_valid{};
                BOOL window_valid{};
                int timeout_ms{ static_cast<int>(GetDlgItemInt(hDlg, IDC_TIMEOUT_EDIT, &timeout_valid, FALSE)) };
                int window_size{ static_cast<int>(GetDlgItemInt(hDlg, IDC_WINDOW_SIZE_EDIT, &window_valid, FALSE)) };
                if (!timeout_valid || !window_valid)
                {
                    MessageBoxW(hDlg, L"Timeout and loss window must be numbers.", L"Ping Monitor", MB_ICONWARNING);
                    return TRUE;
                }

                auto& data{ CDataManager::Instance() };
                data.m_settings.target = target;
                data.m_settings.timeout_ms = ClampInt(timeout_ms, 200, 5000);
                data.m_settings.window_size = ClampInt(window_size, 5, 100);
                data.ClearSamples();
                data.SaveConfig();
                EndDialog(hDlg, IDOK);
                return TRUE;
            }
            case IDCANCEL:
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            default:
                break;
            }
            break;
        default:
            break;
        }
        return FALSE;
    }
}

CPingMonitorPlugin CPingMonitorPlugin::m_instance;

CPingMonitorPlugin::CPingMonitorPlugin()
{
}

CPingMonitorPlugin& CPingMonitorPlugin::Instance()
{
    return m_instance;
}

IPluginItem* CPingMonitorPlugin::GetItem(int index)
{
    switch (index)
    {
    case 0:
        return &m_latency_item;
    case 1:
        return &m_loss_item;
    default:
        break;
    }
    return nullptr;
}

void CPingMonitorPlugin::DataRequired()
{
    auto& data{ CDataManager::Instance() };
    const unsigned long long now{ GetTickCount64() };
    if (data.m_last_query_tick != 0 && now - data.m_last_query_tick < 1000)
        return;

    unsigned long latency_ms{};
    std::wstring status_text;
    const bool success{ PingHost(data.m_settings.target, static_cast<unsigned long>(data.m_settings.timeout_ms), latency_ms, status_text) };
    data.AddPingSample(success, latency_ms, status_text);
    data.m_last_query_tick = GetTickCount64();
}

ITMPlugin::OptionReturn CPingMonitorPlugin::ShowOptionsDialog(void* hParent)
{
    const auto hInstance{ reinterpret_cast<HINSTANCE>(&__ImageBase) };
    const INT_PTR result{ DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_OPTIONS_DIALOG), static_cast<HWND>(hParent), OptionsDlgProc, 0) };
    return result == IDOK ? OR_OPTION_CHANGED : OR_OPTION_UNCHANGED;
}

const wchar_t* CPingMonitorPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"Ping Monitor";
    case TMI_DESCRIPTION:
        return L"Displays ping latency and packet loss for a configured server.";
    case TMI_AUTHOR:
        return L"TrafficMonitor contributor";
    case TMI_COPYRIGHT:
        return L"Copyright (C) 2026";
    case TMI_VERSION:
        return L"1.0";
    case TMI_URL:
        return L"https://github.com/zhongyang219/TrafficMonitor";
    default:
        return L"";
    }
}

const wchar_t* CPingMonitorPlugin::GetTooltipInfo()
{
    return CDataManager::Instance().m_tooltip_text.c_str();
}

void CPingMonitorPlugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR)
        CDataManager::Instance().LoadConfig(std::wstring(data));
}

ITMPlugin* TMPluginGetInstance()
{
    return &CPingMonitorPlugin::Instance();
}
