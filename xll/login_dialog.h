// 登录对话框：模态弹出，展示二维码并跑设备流，成功写出 token 文件。
#pragma once
#include <string>

// 弹出设备登录对话框（模态）。成功返回 true 并写出 TOKEN_FILE_NAME 到 %TEMP%；statusText 返回给调用方展示。
bool ShowDeviceLoginDialog(std::wstring& statusText);
