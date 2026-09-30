// 登录对话框实现：Win32 + GDI 画二维码 + 工作线程跑设备流。
#include "login_dialog.h"
#include "device_flow.h"
#include "config.h"
#include "qrcodegen.hpp"
#include "resource.h"
#include <windows.h>
#include <string>

using qrcodegen::QrCode;

static HWND g_hwndDlg = nullptr;
static HWND g_hQR = nullptr, g_hCode = nullptr, g_hStatus = nullptr;
static HBITMAP g_hBmp = nullptr;
static std::wstring g_resultText;

#define WM_DC_READY (WM_APP + 1)
#define WM_STATUS   (WM_APP + 2)
enum StatusKind { SK_PENDING = 0, SK_SLOWDOWN = 1, SK_EXPIRED = 2, SK_ERROR = 3, SK_SUCCESS = 4 };

static std::string WToUtf8(const std::wstring& s) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string o(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &o[0], n, nullptr, nullptr);
    return o;
}

// 生成二维码位图（32 位 DIB，顶向下）
static HBITMAP MakeQrBitmap(const std::wstring& text, int targetPx) {
    QrCode qr = QrCode::encodeText(WToUtf8(text).c_str(), QrCode::Ecc::MEDIUM);
    int n = qr.getSize();
    int border = 4;
    int total = n + border * 2;
    int scale = (targetPx / total) > 0 ? (targetPx / total) : 1;
    int dim = total * scale;

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(bi);
    bi.biWidth = dim;
    bi.biHeight = -dim; // 顶向下
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!hbmp) return nullptr;

    DWORD* px = (DWORD*)bits;
    for (int py = 0; py < dim; py++) {
        int my = py / scale - border;
        for (int pxx = 0; pxx < dim; pxx++) {
            int mx = pxx / scale - border;
            bool black = (mx >= 0 && my >= 0 && mx < n && my < n) && qr.getModule(mx, my);
            px[py * dim + pxx] = black ? 0x00000000 : 0x00FFFFFF;
        }
    }
    return hbmp;
}

static DWORD WINAPI WorkerThread(LPVOID) {
    auto onDC = [](const DeviceCode& dc) {
        DeviceCode* p = new DeviceCode(dc);
        PostMessage(g_hwndDlg, WM_DC_READY, 0, (LPARAM)p);
    };
    auto onStatus = [](const std::wstring& kind, const std::wstring& msg) {
        int k = SK_PENDING;
        if (kind == L"slow_down") k = SK_SLOWDOWN;
        else if (kind == L"expired") k = SK_EXPIRED;
        else if (kind == L"error") k = SK_ERROR;
        else if (kind == L"success") k = SK_SUCCESS;
        std::wstring* ps = new std::wstring(msg);
        PostMessage(g_hwndDlg, WM_STATUS, (WPARAM)k, (LPARAM)ps);
    };
    RunDeviceFlow(KC_BASE, KC_REALM, KC_CLIENT_ID, KC_SCOPE, onDC, onStatus);
    return 0;
}

static void WriteTokenFile(const std::wstring& json) {
    std::wstring path;
    wchar_t tmp[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tmp))
        path = std::wstring(tmp) + TOKEN_FILE_NAME;
    else
        path = std::wstring(L".\\") + TOKEN_FILE_NAME;
    std::string u = WToUtf8(json);
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD w = 0;
        WriteFile(h, u.data(), (DWORD)u.size(), &w, nullptr);
        CloseHandle(h);
    }
}

static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_INITDIALOG:
        g_hwndDlg = hwnd;
        g_hQR = GetDlgItem(hwnd, IDC_QR);
        g_hCode = GetDlgItem(hwnd, IDC_CODE);
        g_hStatus = GetDlgItem(hwnd, IDC_STATUS);
        SetDlgItemTextW(hwnd, IDC_STATUS, L"正在申请设备码…");
        { HANDLE h = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr); if (h) CloseHandle(h); }
        return TRUE;

    case WM_DC_READY: {
        DeviceCode* dc = (DeviceCode*)l;
        if (dc) {
            if (g_hBmp) DeleteObject(g_hBmp);
            g_hBmp = MakeQrBitmap(dc->verification_uri_complete, 240);
            if (g_hBmp) SendMessage(g_hQR, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)g_hBmp);
            SetDlgItemTextW(g_hCode, IDC_CODE, dc->user_code.c_str());
        }
        delete dc;
        return TRUE;
    }

    case WM_STATUS: {
        std::wstring* ps = (std::wstring*)l;
        int k = (int)w;
        std::wstring txt;
        switch (k) {
        case SK_PENDING:  txt = L"等待手机端授权…"; break;
        case SK_SLOWDOWN: txt = L"降低轮询频率，等待授权…"; break;
        case SK_EXPIRED:  txt = L"二维码已过期，请重试"; break;
        case SK_ERROR:    txt = L"错误：" + (ps ? *ps : std::wstring()); break;
        case SK_SUCCESS:
            txt = L"登录成功";
            if (ps) { WriteTokenFile(*ps); g_resultText = L"OK"; }
            break;
        }
        if (ps) delete ps;
        SetDlgItemTextW(g_hStatus, IDC_STATUS, txt.c_str());
        if (k == SK_SUCCESS) EndDialog(hwnd, IDOK);
        else if (k == SK_EXPIRED || k == SK_ERROR) EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }

    case WM_CLOSE:
        EndDialog(hwnd, IDCANCEL);
        return TRUE;

    case WM_DESTROY:
        if (g_hBmp) { DeleteObject(g_hBmp); g_hBmp = nullptr; }
        return FALSE;
    }
    return FALSE;
}

bool ShowDeviceLoginDialog(std::wstring& statusText) {
    g_resultText.clear();
    if (g_hBmp) { DeleteObject(g_hBmp); g_hBmp = nullptr; }
    INT_PTR r = DialogBoxParamW(GetModuleHandle(nullptr),
                               MAKEINTRESOURCEW(IDR_LOGIN_DIALOG), nullptr, DlgProc, 0);
    bool ok = (r == IDOK);
    statusText = g_resultText;
    return ok;
}
