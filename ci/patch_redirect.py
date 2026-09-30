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
    patch_ios()
    print("done")
    sys.exit(0)
