# starmaster-auth-clients

星主认证客户端集合（Keycloak Device Grant 扫码登录）。对应自建 Keycloak（`keycloak.starmaster.us.ci`，realm `master`）的扫码登录体系。

## 架构总览
手机端先用 `mobile-app` 登录并保持 Keycloak SSO 会话 → 桌面 / XLL / 网页端用 `device-login`（Device Grant）申请设备码、展示二维码 → 手机扫码在确认页批准（已登录则免输密码）→ 设备端轮询拿到 token → 后端 JWKS 验签放行（模型 A：登录即得功能权限）。

服务端已落地：关闭自助注册（仅内部用户）、`device-login` 公共 client（开启 Device Grant，`device_code` 有效期 120s）、`webOrigins` 已配跨域。详见《Keycloak部署与接入文档.md》（`D:/cd/ssh cli/Keycloak部署与接入文档.md` §3.5）。

## 目录
```
mobile_app/        手机端（Flutter）：mobile-app 客户端，PKCE 登录（flutter_appauth，系统浏览器）
desktop_login/     桌面端（Flutter）：device-login 设备端，申请码 + 二维码 + 轮询
xll/               单文件 XLL（C++）：内嵌二维码生成 + WinHTTP 轮询 + Win32 对话框，一个 .xll 即全部
ci/                CI 辅助脚本（redirect scheme 注入）
.github/workflows/ build-flutter.yml / build-xll.yml
```

## 配置点（三端一致）
- Keycloak：`KEYCLOAK_BASE=https://keycloak.starmaster.us.ci`、`REALM=master`。
- 手机端 `mobile_app/lib/keycloak_config.dart`：`kcClientId=mobile-app`、`kcRedirectUri=myapp://callback`。
- 桌面端 / XLL：`device-login` 的 `base/realm/clientId/scope`（XLL 在 `xll/config.h`；桌面端在 `device_login_service.dart` 默认值）。

## 构建
### 本地
- Flutter 工程：首次在 `mobile_app/`、`desktop_login/` 运行 `flutter create --platforms=android,ios .`（桌面端用 `windows,macos,linux`）生成平台目录，再 `flutter pub get` 与 `flutter run` / `flutter build`。
- XLL：需用 Visual Studio / MSVC（或 CMake + MSVC）。`cmake -S xll -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release` → 产物 `build/Release/DeviceLogin.xll`。

### CI（GitHub Actions）
- `build-flutter.yml`：分析 + 构建 mobile_app（Android APK / iOS unsigned app）+ desktop_login（Windows / macOS / Linux），产物上传为 artifacts。
- `build-xll.yml`：Windows runner 用 MSVC 编译 `DeviceLogin.xll`，产物上传为 artifact。

## 单文件 XLL 说明
`xll/` 直接产出**一个 `DeviceLogin.xll`**：内嵌 Nayuki QR 生成器（vendor `qrcodegen.hpp/.cpp`）、Excel XLL SDK 的 `xlcall.h/.cpp`、`WinHTTP` 轮询、`Win32` 对话框。Excel 中 `=DeviceLogin()` 弹出二维码窗口，授权成功后把 token 写到 `%TEMP%/device_login_token.json` 并返回 `OK`。无需外部辅助 exe / .NET / WebView2（仅依赖普遍存在的 VC++ 运行库）。

> 早期方案（独立 `DeviceLoginHelper.exe` + 薄 XLL 壳）已废弃，由本单文件 XLL 取代。

## 后端衔接（模型 A）
三端拿到的 `access_token` 只代表"已通过 Keycloak 认证"。业务后端必须：① 用 Keycloak JWKS（`/realms/master/protocol/openid-connect/certs`）验签；② 验签通过即放行（模型 A 不判角色/到期）。客户端只做转发。

## 安全
- `device_code` 有效期 120s（降低钓鱼面）；仅 `device-login` 一个 client 开 Device Grant。
- 高敏操作可叠加按需 MFA。
- `%TEMP%/device_login_token.json` 含凭证，读取后应及时清理。
