// XLL 入口：导出 Excel 要求的 xlAutoOpen / xlAutoClose / xlAutoFree12 / xlAddInManagerInfo12，
// 并注册 DeviceLogin() UDF（弹出扫码登录对话框，返回 "OK"/错误文本，并把 token 写到 %TEMP%）。
#include "xlcall.h"
#include "login_dialog.h"
#include <windows.h>
#include <string>
#include <cstdlib>
#include <cwchar>

// 构造一个 XLOPER12 字符串（堆分配，带 xlbitDLLFree 由 Excel 释放）
static XLOPER12 MakeStrOp(const wchar_t* s, bool dllFree) {
    int len = (int)wcslen(s);
    WCHAR* buf = (WCHAR*)malloc((len + 2) * sizeof(WCHAR));
    buf[0] = (WCHAR)len;
    wcscpy(&buf[1], s);
    XLOPER12 x;
    x.xltype = xltypeStr | (dllFree ? xlbitDLLFree : 0);
    x.val.str = buf;
    return x;
}

static XLOPER12 MakeEmptyStrOp() {
    WCHAR* buf = (WCHAR*)malloc(2 * sizeof(WCHAR));
    buf[0] = 0;
    XLOPER12 x;
    x.xltype = xltypeStr | xlbitDLLFree;
    x.val.str = buf;
    return x;
}

static XLOPER12 g_infoOp; // xlAddInManagerInfo12 返回，静态保活

extern "C" LPXLOPER12 WINAPI DeviceLogin(void) {
    std::wstring status;
    bool ok = ShowDeviceLoginDialog(status);
    std::wstring ret = ok ? L"OK" : (status.empty() ? L"FAILED" : status);

    static XLOPER12 x;
    int len = (int)ret.size();
    WCHAR* buf = (WCHAR*)malloc((len + 2) * sizeof(WCHAR));
    buf[0] = (WCHAR)len;
    wcscpy(&buf[1], ret.c_str());
    x.xltype = xltypeStr | xlbitDLLFree;
    x.val.str = buf;
    return &x;
}

extern "C" int WINAPI xlAutoOpen(void) {
    XLOPER12 xDll, xReg;
    Excel12(xlGetName, &xDll, 0);

    XLOPER12 op = MakeStrOp(L"DeviceLogin", true);   // 过程名（与导出名一致）
    XLOPER12 type = MakeStrOp(L"J", true);           // 返回字符串，无参数
    XLOPER12 name = MakeStrOp(L"DeviceLogin", true); // Excel 中显示名
    XLOPER12 args = MakeEmptyStrOp();                // 参数名（空）
    XLOPER12 cat = MakeStrOp(L"星主认证", true);   // 类别
    XLOPER12 help = MakeStrOp(L"弹出扫码登录对话框，返回 OK 或错误文本", true);

    int rc = Excel12(xlfRegister, &xReg, 7,
                     &xDll, &op, &type, &name, &args, &cat, &help);
    Excel12(xlFree, nullptr, 1, &xDll);
    return (rc == xlretSuccess) ? 1 : 0;
}

extern "C" int WINAPI xlAutoClose(void) {
    return 1;
}

extern "C" void WINAPI xlAutoFree12(LPXLOPER12 p) {
    if ((p->xltype & xlbitDLLFree) && (p->xltype & xltypeStr) && p->val.str) {
        free(p->val.str);
    }
}

extern "C" LPXLOPER12 WINAPI xlAddInManagerInfo12(LPXLOPER12 xAction) {
    // xAction->val.w == 1 → 返回 add-in 名称
    bool name = (xAction && (xAction->xltype & xltypeInt) && xAction->val.w == 1);
    std::wstring s = name ? L"DeviceLogin XLL（扫码登录）" : L"";
    XLOPER12 x = MakeStrOp(s.c_str(), true);
    g_infoOp = x; // 静态保活（实际由 Excel 经 xlAutoFree12 释放其 str）
    return &g_infoOp;
}

BOOL APIENTRY DllMain(HINSTANCE, DWORD reason, LPVOID) {
    return TRUE;
}
