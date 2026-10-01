#!/usr/bin/env python3
"""CI 辅助：在 `flutter create` 生成平台目录后，为 mobile_app 注入 myapp://callback 重定向 scheme。
Android：向 MainActivity 注入 VIEW/BROWSABLE intent-filter。
iOS：向 Info.plist 注入 CFBundleURLTypes。
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOBILE = os.path.join(ROOT, "mobile_app")


def patch_android():
    path = os.path.join(MOBILE, "android", "app", "src", "main", "AndroidManifest.xml")
    if not os.path.exists(path):
        print(f"[skip] AndroidManifest not found: {path}")
        return
    with open(path, encoding="utf-8") as f:
        s = f.read()
    marker = 'android:scheme="myapp"'
    if marker in s:
        print("[ok] Android redirect already present")
        return
    inject = """
        <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="myapp" android:host="callback" />
        </intent-filter>
"""
    idx = s.rfind("</activity>")
    if idx == -1:
        print("[skip] </activity> not found")
        return
    s = s[:idx] + inject + s[idx:]
    with open(path, "w", encoding="utf-8") as f:
        f.write(s)
    print("[ok] Android redirect injected")


def patch_android_gradle():
    """给 flutter create 生成的 build.gradle.kts 打两处补丁：
    1) appAuthRedirectScheme 占位符（flutter_appauth 的 manifest 合并要求）；
    2) compileSdk 强制设为 35（flutter_appauth 8.x 的 AAR 要求 compileSdk >= 34，
       否则 checkReleaseAarMetadata 失败）。
    """
    import re
    path = os.path.join(MOBILE, "android", "app", "build.gradle.kts")
    if not os.path.exists(path):
        print(f"[skip] build.gradle.kts not found: {path}")
        return
    with open(path, encoding="utf-8") as f:
        s = f.read()
    changed = False

    # 1) appAuthRedirectScheme 占位符
    if "appAuthRedirectScheme" not in s:
        marker = "defaultConfig {"
        idx = s.find(marker)
        if idx == -1:
            print("[skip] defaultConfig not found in build.gradle.kts")
        else:
            insert_at = idx + len(marker)
            nl = s.find("\n", insert_at)
            if nl == -1:
                nl = len(s)
            s = s[:nl] + '\n        manifestPlaceholders["appAuthRedirectScheme"] = "myapp"' + s[nl:]
            changed = True
            print("[ok] appAuthRedirectScheme injected into build.gradle.kts")
    else:
        print("[ok] appAuthRedirectScheme already set")

    # 2) compileSdk >= 34
    new_s, n = re.subn(r'compileSdk\s*=\s*[^;\n]+', 'compileSdk = 35', s)
    if n == 0:
        m = s.find("android {")
        if m != -1:
            ins = m + len("android {")
            nl = s.find("\n", ins)
            if nl == -1:
                nl = len(s)
            new_s = s[:nl] + "\n    compileSdk = 35" + s[nl:]
            changed = True
    if new_s != s:
        s = new_s
        changed = True
        print("[ok] compileSdk set to 35")
    else:
        print("[ok] compileSdk already >= 34")

    if changed:
        with open(path, "w", encoding="utf-8") as f:
            f.write(s)


def patch_android_localprops():
    """flutter_appauth 等插件模块的 compileSdk 来自 flutter.compileSdkVersion
    （由 android/local.properties 提供）。仅改 app 模块的 build.gradle.kts 不够，
    必须在这里把 flutter.compileSdkVersion 提到 >=34，插件模块才会继承，
    否则 :flutter_appauth 仍按 android-31 编译，AAR 元数据校验失败。
    """
    path = os.path.join(MOBILE, "android", "local.properties")
    if not os.path.exists(path):
        print(f"[skip] local.properties not found: {path}")
        return
    with open(path, encoding="utf-8") as f:
        lines = f.readlines()
    out = []
    found = False
    for line in lines:
        if line.startswith("flutter.compileSdkVersion"):
            out.append("flutter.compileSdkVersion=35\n")
            found = True
        else:
            out.append(line)
    if not found:
        out.append("flutter.compileSdkVersion=35\n")
    with open(path, "w", encoding="utf-8") as f:
        f.writelines(out)
    print("[ok] local.properties flutter.compileSdkVersion set to 35")


def patch_plugin_compilesdk():
    """直接改 pub 缓存中 flutter_appauth 插件模块的 compileSdk。
    local.properties / app 模块的 compileSdk 都不会传导到插件模块自身，
    所以 AAR 元数据校验仍以 android-31 失败；只能就地改插件 android/build.gradle。
    """
    import re
    import glob
    roots = [os.path.join(MOBILE, ".pub-cache"),
             os.path.expanduser("~/.pub-cache")]
    candidates = []
    for r in roots:
        candidates += glob.glob(os.path.join(r, "**", "flutter_appauth*", "android", "build.gradle*"), recursive=True)
    if not candidates:
        print("[skip] flutter_appauth plugin build.gradle not found in pub cache")
        return
    patched_any = False
    for p in candidates:
        with open(p, encoding="utf-8") as f:
            s = f.read()
        new_s = s
        # compileSdkVersion 31 / flutter.compileSdkVersion -> 35
        new_s = re.sub(r'compileSdkVersion\s+flutter\.compileSdkVersion', 'compileSdkVersion 35', new_s)
        new_s = re.sub(r'compileSdkVersion\s+\d+', 'compileSdkVersion 35', new_s)
        # Kotlin DSL: compileSdk = 35 / flutter.compileSdkVersion -> 35
        new_s = re.sub(r'compileSdk\s*=\s*flutter\.compileSdkVersion', 'compileSdk = 35', new_s)
        new_s = re.sub(r'compileSdk\s*=\s*\d+', 'compileSdk = 35', new_s)
        if new_s != s:
            with open(p, "w", encoding="utf-8") as f:
                f.write(new_s)
            patched_any = True
            print(f"[ok] plugin compileSdk forced 35: {p}")
    if not patched_any:
        print("[skip] no compileSdk line patchable in plugin (already >=34?)")


def patch_ios():
    path = os.path.join(MOBILE, "ios", "Runner", "Info.plist")
    if not os.path.exists(path):
        print(f"[skip] Info.plist not found: {path}")
        return
    with open(path, encoding="utf-8") as f:
        s = f.read()
    if "myapp" in s:
        print("[ok] iOS redirect already present")
        return
    block = """
	<key>CFBundleURLTypes</key>
	<array>
		<dict>
			<key>CFBundleURLSchemes</key>
			<array>
				<string>myapp</string>
			</array>
		</dict>
	</array>
"""
    idx = s.rfind("</dict>")
    if idx == -1:
        print("[skip] </dict> not found")
        return
    s = s[:idx] + block + s[idx:]
    with open(path, "w", encoding="utf-8") as f:
        f.write(s)
    print("[ok] iOS redirect injected")


if __name__ == "__main__":
    patch_android()
    patch_android_gradle()
    patch_android_localprops()
    patch_plugin_compilesdk()
    patch_ios()
    print("done")
    sys.exit(0)
