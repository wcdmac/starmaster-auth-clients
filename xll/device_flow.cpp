// Device Grant 设备流实现：WinHTTP 申请设备码 + 轮询取 token。
#include "device_flow.h"
#include "config.h"
#include <windows.h>
#include <winhttp.h>
#include <string>

#pragma comment(lib, "winhttp.lib")

namespace {

// 极简 JSON 取值：在 value 中找 "key"，随后取字符串（"..."）或整数。
std::wstring JsonGetString(const std::wstring& j, const std::wstring& key) {
    std::wstring pat = L"\"" + key + L"\"";
    size_t p = j.find(pat);
    if (p == std::wstring::npos) return L"";
    p = j.find(L':', p + pat.size());
    if (p == std::wstring::npos) return L"";
    p = j.find(L'"', p + 1);
    if (p == std::wstring::npos) return L"";
    size_t q = j.find(L'"', p + 1);
    if (q == std::wstring::npos) return L"";
    return j.substr(p + 1, q - p - 1);
}

int JsonGetInt(const std::wstring& j, const std::wstring& key, int def) {
    std::wstring pat = L"\"" + key + L"\"";
    size_t p = j.find(pat);
    if (p == std::wstring::npos) return def;
    p = j.find(L':', p + pat.size());
    if (p == std::wstring::npos) return def;
    p++;
    while (p < j.size() && (j[p] == L' ' || j[p] == L'\t')) p++;
    try { return std::stoi(j.substr(p, 12)); } catch (...) { return def; }
}

std::wstring JsonGetError(const std::wstring& j) {
    std::wstring e = JsonGetString(j, L"error");
    if (!e.empty()) return e;
    return L"unknown_error";
}

// 拆分 URL：https://host[:port]/path
bool CrackUrl(const std::wstring& url, std::wstring& host, std::wstring& path, INTERNET_PORT& port, bool& secure) {
    URL_COMPONENTS uc = { sizeof(uc) };
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = nullptr; uc.dwHostNameLength = 0;
    uc.lpszUrlPath = nullptr; uc.dwUrlPathLength = 0;
    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) return false;
    host = std::wstring(uc.lpszHostName, uc.dwHostNameLength);
    path = std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (path.empty()) path = L"/";
    port = uc.nPort;
    secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    return true;
}

// HTTPS POST，返回响应体（窄/宽统一用宽串承载）。
bool HttpPost(const std::wstring& url, const std::wstring& body, std::wstring& out) {
    std::wstring host, path;
    INTERNET_PORT port = 0;
    bool secure = false;
    if (!CrackUrl(url, host, path, port, secure)) return false;

    HINTERNET hSess = WinHttpOpen(L"DeviceLoginXLL/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return false;

    HINTERNET hConn = WinHttpConnect(hSess, host.c_str(), port, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return false; }

    DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return false; }

    // 超时，避免长时间挂起
    DWORD timeout = 15000;
    WinHttpSetOption(hReq, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));

    int n = (int)body.size();
    BOOL ok = WinHttpSendRequest(hReq, L"Content-Type: application/x-www-form-urlencoded\r\n",
                                 (DWORD)-1, (LPVOID)body.data(), (DWORD)(body.size() * sizeof(wchar_t)),
                                 (DWORD)(body.size() * sizeof(wchar_t)), 0);
    if (!ok) { WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return false; }

    if (!WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return false;
    }

    DWORD avail = 0, total = 0;
    std::wstring buf;
    // 响应体为 UTF-8 JSON，先按字节读再转宽
    std::string raw;
    do {
        if (!WinHttpQueryDataAvailable(hReq, &avail)) break;
        if (avail == 0) break;
        std::string chunk(avail, 0);
        DWORD read = 0;
        if (!WinHttpReadData(hReq, chunk.data(), avail, &read)) break;
        raw.append(chunk, 0, read);
        total += read;
    } while (avail > 0);

    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);

    // UTF-8 → UTF-16
    int wlen = MultiByteToWideChar(CP_UTF8, 0, raw.data(), (int)raw.size(), nullptr, 0);
    if (wlen <= 0) return false;
    buf.resize(wlen);
    MultiByteToWideChar(CP_UTF8, 0, raw.data(), (int)raw.size(), buf.data(), wlen);
    out = buf;
    return total > 0 || true; // 即便空也视为成功（由调用方判内容）
}

// 组装 form body（键值均无特殊字符，直接拼）
std::wstring FormBody(const std::initializer_list<std::pair<std::wstring, std::wstring>>& kv) {
    std::wstring s;
    bool first = true;
    for (auto& p : kv) {
        if (!first) s += L"&";
        s += p.first + L"=" + p.second;
        first = false;
    }
    return s;
}

} // namespace

Tokens RunDeviceFlow(
    const std::wstring& base, const std::wstring& realm,
    const std::wstring& clientId, const std::wstring& scope,
    const std::function<void(const DeviceCode&)>& onDeviceCode,
    const std::function<void(const std::wstring& kind, const std::wstring& msg)>& onStatus) {

    Tokens result;
    std::wstring deviceUrl = base + L"/realms/" + realm + L"/protocol/openid-connect/auth/device";
    std::wstring tokenUrl = base + L"/realms/" + realm + L"/protocol/openid-connect/token";

    // 1. 申请设备码
    std::wstring resp;
    if (!HttpPost(deviceUrl, FormBody({ { L"client_id", clientId }, { L"scope", scope } }), resp)) {
        onStatus(L"error", L"申请设备码网络失败");
        return result;
    }
    DeviceCode dc;
    dc.device_code = JsonGetString(resp, L"device_code");
    dc.user_code = JsonGetString(resp, L"user_code");
    dc.verification_uri = JsonGetString(resp, L"verification_uri");
    dc.verification_uri_complete = JsonGetString(resp, L"verification_uri_complete");
    dc.expires_in = JsonGetInt(resp, L"expires_in", DC_EXPIRES_IN);
    dc.interval = JsonGetInt(resp, L"interval", DC_INTERVAL);
    if (dc.device_code.empty()) {
        onStatus(L"error", L"设备码返回为空");
        return result;
    }
    onDeviceCode(dc);

    // 2. 轮询
    int interval = dc.interval;
    int elapsed = 0;
    while (elapsed < dc.expires_in) {
        Sleep(interval * 1000);
        elapsed += interval;

        std::wstring body = FormBody({
            { L"grant_type", L"urn:ietf:params:oauth:grant-type:device_code" },
            { L"client_id", clientId },
            { L"device_code", dc.device_code },
        });
        std::wstring tresp;
        if (!HttpPost(tokenUrl, body, tresp)) {
            onStatus(L"error", L"轮询网络失败");
            return result;
        }
        if (!JsonGetString(tresp, L"access_token").empty()) {
            result.access_token = JsonGetString(tresp, L"access_token");
            result.refresh_token = JsonGetString(tresp, L"refresh_token");
            result.id_token = JsonGetString(tresp, L"id_token");
            result.expires_in = JsonGetInt(tresp, L"expires_in", 0);
            std::wstring json = L"{\"access_token\":\"" + result.access_token +
                L"\",\"refresh_token\":\"" + result.refresh_token +
                L"\",\"id_token\":\"" + result.id_token +
                L"\",\"expires_in\":" + std::to_wstring(result.expires_in) + L"}";
            onStatus(L"success", json);
            return result;
        }
        std::wstring err = JsonGetError(tresp);
        if (err == L"authorization_pending") {
            onStatus(L"pending", L"");
        } else if (err == L"slow_down") {
            interval += 5;
            onStatus(L"slow_down", L"");
        } else if (err == L"expired_token") {
            onStatus(L"expired", L"");
            return result;
        } else {
            onStatus(L"error", err);
            return result;
        }
    }
    onStatus(L"expired", L"");
    return result;
}
