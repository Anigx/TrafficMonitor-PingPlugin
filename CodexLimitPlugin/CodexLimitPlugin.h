#pragma once

#include "..\include\PluginInterface.h"
#include "PluginCodexLimitItem.h"

class CCodexLimitPlugin : public ITMPlugin
{
private:
    CCodexLimitPlugin();

public:
    static CCodexLimitPlugin& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;

private:
    CPluginCodexFiveHourItem m_five_hour_item;
    CPluginCodexWeeklyItem m_weekly_item;
    static CCodexLimitPlugin m_instance;
};

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
