#include "stdafx.h"
#include "PdhQuery.h"

CPdhQuery::CPdhQuery(LPCTSTR _fullCounterPath)
    : fullCounterPath(_fullCounterPath)
{
    Initialize();
}

CPdhQuery::~CPdhQuery()
{
    //关闭查询
    PdhCloseQuery(query);
}

bool CPdhQuery::Initialize()
{
    if (isInitialized)
        return true;

    PDH_STATUS status;
    //打开查询
    status = PdhOpenQuery(NULL, NULL, &query);
    if (status != ERROR_SUCCESS)
        return false;

    //添加计数器
    status = PdhAddCounter(query, fullCounterPath.GetString(), NULL, &counter);
    //先调用PdhAddCounter，如果失败使用PdhAddEnglishCounter再试一次
    if (status != ERROR_SUCCESS)
    {
        status = PdhAddEnglishCounter(query, fullCounterPath.GetString(), NULL, &counter);
        if (status != ERROR_SUCCESS)
        {
            PdhCloseQuery(query);
            query = nullptr;
            return false;
        }
    }

    //初始化计数器
    PdhCollectQueryData(query);
    isInitialized = true;
    return true;
}

bool CPdhQuery::QueryValue(double& value)
{
    if (!isInitialized)
        return false;

    //更新数据
    PdhCollectQueryData(query);
    PDH_FMT_COUNTERVALUE pdhValue;
    DWORD dwValue;
    PDH_STATUS status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, &dwValue, &pdhValue);
    if (status != ERROR_SUCCESS)
    {
        return false;
    }
    value = pdhValue.doubleValue;
    return true;
}

bool CPdhQuery::QueryValues(std::vector<CounterValueItem>& values)
{
    values.clear();
    if (!isInitialized || PdhCollectQueryData(query) != ERROR_SUCCESS)
        return false;
    DWORD bytes = 0, count = 0;
    auto status = PdhGetFormattedCounterArray(counter, PDH_FMT_DOUBLE, &bytes, &count, nullptr);
    if (status != PDH_MORE_DATA)
        return false;
    m_array_buffer.resize(bytes);
    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(m_array_buffer.data());
    status = PdhGetFormattedCounterArray(counter, PDH_FMT_DOUBLE, &bytes, &count, items);
    if (status != ERROR_SUCCESS)
        return false;
    values.reserve(count);
    for (DWORD i = 0; i < count; ++i)
    {
        if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
            items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA)
            continue;
        CounterValueItem item;
        item.name = items[i].szName;
        item.value = items[i].FmtValue.doubleValue;
        values.push_back(std::move(item));
    }
    return true;
}
