#include "pch.h"
#include "CodexLimitPlugin.h"
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

    int GetPercentFromEdit(HWND hDlg, int control_id, int current_value)
    {
        wchar_t text[32]{};
        GetDlgItemTextW(hDlg, control_id, text, ARRAYSIZE(text));
        if (text[0] == L'\0')
            return -1;

        wchar_t* end{};
        const long value{ wcstol(text, &end, 10) };
        if (end == text)
            return current_value;

        return ClampInt(static_cast<int>(value), 0, 100);
    }

    void SetPercentEdit(HWND hDlg, int control_id, int value)
    {
        if (value < 0)
        {
            SetDlgItemTextW(hDlg, control_id, L"");
            return;
        }
        SetDlgItemInt(hDlg, control_id, static_cast<UINT>(ClampInt(value, 0, 100)), FALSE);
    }

    INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG:
        {
            const auto& data{ CDataManager::Instance() };
            SetPercentEdit(hDlg, IDC_FIVE_HOUR_PERCENT_EDIT, data.m_five_hour.percent);
            SetDlgItemTextW(hDlg, IDC_FIVE_HOUR_RESET_EDIT, data.m_five_hour.reset_text.c_str());
            SetPercentEdit(hDlg, IDC_WEEKLY_PERCENT_EDIT, data.m_weekly.percent);
            SetDlgItemTextW(hDlg, IDC_WEEKLY_RESET_EDIT, data.m_weekly.reset_text.c_str());
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDOK:
            {
                auto& data{ CDataManager::Instance() };
                wchar_t five_hour_reset[128]{};
                wchar_t weekly_reset[128]{};
                GetDlgItemTextW(hDlg, IDC_FIVE_HOUR_RESET_EDIT, five_hour_reset, ARRAYSIZE(five_hour_reset));
                GetDlgItemTextW(hDlg, IDC_WEEKLY_RESET_EDIT, weekly_reset, ARRAYSIZE(weekly_reset));

                data.SetValues(
                    GetPercentFromEdit(hDlg, IDC_FIVE_HOUR_PERCENT_EDIT, data.m_five_hour.percent),
                    five_hour_reset,
                    GetPercentFromEdit(hDlg, IDC_WEEKLY_PERCENT_EDIT, data.m_weekly.percent),
                    weekly_reset);
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

CCodexLimitPlugin CCodexLimitPlugin::m_instance;

CCodexLimitPlugin::CCodexLimitPlugin()
{
}

CCodexLimitPlugin& CCodexLimitPlugin::Instance()
{
    return m_instance;
}

IPluginItem* CCodexLimitPlugin::GetItem(int index)
{
    switch (index)
    {
    case 0:
        return &m_five_hour_item;
    case 1:
        return &m_weekly_item;
    default:
        break;
    }
    return nullptr;
}

void CCodexLimitPlugin::DataRequired()
{
    CDataManager::Instance().Refresh();
}

ITMPlugin::OptionReturn CCodexLimitPlugin::ShowOptionsDialog(void* hParent)
{
    const auto hInstance{ reinterpret_cast<HINSTANCE>(&__ImageBase) };
    const INT_PTR result{ DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_OPTIONS_DIALOG), static_cast<HWND>(hParent), OptionsDlgProc, 0) };
    return result == IDOK ? OR_OPTION_CHANGED : OR_OPTION_UNCHANGED;
}

const wchar_t* CCodexLimitPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"Codex Limit Monitor";
    case TMI_DESCRIPTION:
        return L"Displays ChatGPT Codex 5-hour and weekly usage limits with reset countdowns.";
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

const wchar_t* CCodexLimitPlugin::GetTooltipInfo()
{
    return CDataManager::Instance().m_tooltip_text.c_str();
}

void CCodexLimitPlugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR)
        CDataManager::Instance().LoadConfig(std::wstring(data));
    else if (index == EI_LABEL_TEXT_COLOR)
        CDataManager::Instance().m_label_text_color = static_cast<COLORREF>(_wtoi(data));
    else if (index == EI_VALUE_TEXT_COLOR)
        CDataManager::Instance().m_value_text_color = static_cast<COLORREF>(_wtoi(data));
}

ITMPlugin* TMPluginGetInstance()
{
    return &CCodexLimitPlugin::Instance();
}
