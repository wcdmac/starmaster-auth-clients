// Device Grant 设备流核心（WinHTTP，与 UI 无关）。线程安全：在主流程线程调用回调，调用方负责向 UI 线程投递。
#pragma once
#include <string>
#include <functional>

struct DeviceCode {
    std::wstring device_code;
    std::wstring user_code;
    std::wstring verification_uri;
    std::wstring verification_uri_complete;
    int expires_in = 0;
    int interval = 5;
};

struct Tokens {
    std::wstring access_token;
    std::wstring refresh_token;
    std::wstring id_token;
    int expires_in = 0;
};

// 运行完整设备流（阻塞，应在工作线程中调用）：
//   1. 申请设备码，通过 onDeviceCode 把二维码信息交出去；
//   2. 每 interval 秒轮询 token 端点；
//   3. 通过 onStatus 回报 pending / slow_down / expired / error / success。
// 成功返回 Tokens；过期/出错返回空 Tokens（access_token 为空）。
Tokens RunDeviceFlow(
    const std::wstring& base, const std::wstring& realm,
    const std::wstring& clientId, const std::wstring& scope,
    const std::function<void(const DeviceCode&)>& onDeviceCode,
    const std::function<void(const std::wstring& kind, const std::wstring& msg)>& onStatus);
