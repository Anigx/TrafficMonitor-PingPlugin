#include "pch.h"
#include "PluginPingItem.h"
#include "DataManager.h"

const wchar_t* CPluginPingLatencyItem::GetItemName() const
{
    return L"Ping latency";
}

const wchar_t* CPluginPingLatencyItem::GetItemId() const
{
    return L"PingMonitorLatency";
}

const wchar_t* CPluginPingLatencyItem::GetItemLableText() const
{
    return L"PING:";
}

const wchar_t* CPluginPingLatencyItem::GetItemValueText() const
{
    return CDataManager::Instance().m_latency_value_text.c_str();
}

const wchar_t* CPluginPingLatencyItem::GetItemValueSampleText() const
{
    return L"999ms";
}

int CPluginPingLatencyItem::IsDrawResourceUsageGraph() const
{
    return 1;
}

float CPluginPingLatencyItem::GetResourceUsageGraphValue() const
{
    const auto& data{ CDataManager::Instance() };
    if (!data.m_has_data)
        return 0.0f;
    if (!data.m_last_success)
        return 1.0f;
    return std::min(data.m_latency_ms / 300.0f, 1.0f);
}

const wchar_t* CPluginPacketLossItem::GetItemName() const
{
    return L"Packet loss";
}

const wchar_t* CPluginPacketLossItem::GetItemId() const
{
    return L"PingMonitorPacketLoss";
}

const wchar_t* CPluginPacketLossItem::GetItemLableText() const
{
    return L"LOSS:";
}

const wchar_t* CPluginPacketLossItem::GetItemValueText() const
{
    return CDataManager::Instance().m_loss_value_text.c_str();
}

const wchar_t* CPluginPacketLossItem::GetItemValueSampleText() const
{
    return L"100%";
}

int CPluginPacketLossItem::IsDrawResourceUsageGraph() const
{
    return 1;
}

float CPluginPacketLossItem::GetResourceUsageGraphValue() const
{
    const auto& data{ CDataManager::Instance() };
    if (!data.m_has_data)
        return 0.0f;
    return static_cast<float>(data.GetPingLossPercent() / 100.0);
}
