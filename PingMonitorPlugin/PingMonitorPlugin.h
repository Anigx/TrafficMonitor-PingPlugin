#pragma once

#include "..\include\PluginInterface.h"
#include "PluginPingItem.h"

class CPingMonitorPlugin : public ITMPlugin
{
private:
    CPingMonitorPlugin();

public:
    static CPingMonitorPlugin& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;

private:
    CPluginPingLatencyItem m_latency_item;
    CPluginPacketLossItem m_loss_item;
    static CPingMonitorPlugin m_instance;
};

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
