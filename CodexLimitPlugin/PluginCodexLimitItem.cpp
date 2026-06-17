#include "pch.h"
#include "PluginCodexLimitItem.h"
#include "DataManager.h"

namespace
{
    constexpr int VALUE_COLUMN_PADDING{ 8 };

    float PercentToGraphValue(int percent)
    {
        if (percent < 0)
            return 0.0f;
        return static_cast<float>(std::min(std::max(percent, 0), 100)) / 100.0f;
    }

    int GetTextWidth(HDC hdc, const wchar_t* text)
    {
        SIZE size{};
        GetTextExtentPoint32W(hdc, text, static_cast<int>(wcslen(text)), &size);
        return size.cx;
    }

    int GetValueColumnX(HDC hdc)
    {
        return std::max(GetTextWidth(hdc, L"Codex 5h:"), GetTextWidth(hdc, L"Codex W:")) + VALUE_COLUMN_PADDING;
    }

    int CalculateCodexItemWidth(HDC hdc, const wchar_t* sample_text)
    {
        return GetValueColumnX(hdc) + GetTextWidth(hdc, sample_text) + 2;
    }

    void DrawCodexItem(HDC hdc, int x, int y, int w, int h, const wchar_t* label_text, const wchar_t* value_text)
    {
        auto& data{ CDataManager::Instance() };
        const int old_bk_mode{ SetBkMode(hdc, TRANSPARENT) };
        const COLORREF old_text_color{ GetTextColor(hdc) };

        RECT label_rect{ x, y, x + GetValueColumnX(hdc), y + h };
        RECT value_rect{ x + GetValueColumnX(hdc), y, x + w, y + h };

        SetTextColor(hdc, data.m_label_text_color);
        DrawTextW(hdc, label_text, -1, &label_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SetTextColor(hdc, data.m_value_text_color);
        DrawTextW(hdc, value_text, -1, &value_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SetTextColor(hdc, old_text_color);
        SetBkMode(hdc, old_bk_mode);
    }
}

const wchar_t* CPluginCodexFiveHourItem::GetItemName() const
{
    return L"Codex 5-hour limit";
}

const wchar_t* CPluginCodexFiveHourItem::GetItemId() const
{
    return L"CodexLimitFiveHour";
}

const wchar_t* CPluginCodexFiveHourItem::GetItemLableText() const
{
    return L"Codex 5h:";
}

const wchar_t* CPluginCodexFiveHourItem::GetItemValueText() const
{
    return CDataManager::Instance().m_five_hour.value_text.c_str();
}

const wchar_t* CPluginCodexFiveHourItem::GetItemValueSampleText() const
{
    return L"100% 5.0h";
}

bool CPluginCodexFiveHourItem::IsCustomDraw() const
{
    return true;
}

int CPluginCodexFiveHourItem::GetItemWidthEx(void* hDC) const
{
    return CalculateCodexItemWidth(static_cast<HDC>(hDC), GetItemValueSampleText());
}

void CPluginCodexFiveHourItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    DrawCodexItem(static_cast<HDC>(hDC), x, y, w, h, GetItemLableText(), GetItemValueText());
}

int CPluginCodexFiveHourItem::IsDrawResourceUsageGraph() const
{
    return 1;
}

float CPluginCodexFiveHourItem::GetResourceUsageGraphValue() const
{
    return PercentToGraphValue(CDataManager::Instance().m_five_hour.percent);
}

const wchar_t* CPluginCodexWeeklyItem::GetItemName() const
{
    return L"Codex weekly limit";
}

const wchar_t* CPluginCodexWeeklyItem::GetItemId() const
{
    return L"CodexLimitWeekly";
}

const wchar_t* CPluginCodexWeeklyItem::GetItemLableText() const
{
    return L"Codex W:";
}

const wchar_t* CPluginCodexWeeklyItem::GetItemValueText() const
{
    return CDataManager::Instance().m_weekly.value_text.c_str();
}

const wchar_t* CPluginCodexWeeklyItem::GetItemValueSampleText() const
{
    return L"100% 168.0h";
}

bool CPluginCodexWeeklyItem::IsCustomDraw() const
{
    return true;
}

int CPluginCodexWeeklyItem::GetItemWidthEx(void* hDC) const
{
    return CalculateCodexItemWidth(static_cast<HDC>(hDC), GetItemValueSampleText());
}

void CPluginCodexWeeklyItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    DrawCodexItem(static_cast<HDC>(hDC), x, y, w, h, GetItemLableText(), GetItemValueText());
}

int CPluginCodexWeeklyItem::IsDrawResourceUsageGraph() const
{
    return 1;
}

float CPluginCodexWeeklyItem::GetResourceUsageGraphValue() const
{
    return PercentToGraphValue(CDataManager::Instance().m_weekly.percent);
}
