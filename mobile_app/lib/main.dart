import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart' show PlatformException;
import 'package:flutter_appauth/flutter_appauth.dart';
import 'keycloak_config.dart';

void main() => runApp(const MyApp());

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: '星主认证',
      theme: ThemeData(useMaterial3: true, colorSchemeSeed: Colors.blue),
      home: const LoginPage(),
    );
  }
}

class LoginPage extends StatefulWidget {
  const LoginPage({super.key});

  @override
  State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  String _status = '未登录';
  String _detail = '';
  bool _busy = false;

  Map<String, dynamic>? _decodeSub(String? idToken, String? accessToken) {
    final t = idToken ?? accessToken;
    if (t == null) return null;
    try {
      final p = t.split('.')[1];
      final b = base64Url.normalize(p);
      return jsonDecode(utf8.decode(base64Url.decode(b)));
    } catch (_) {
      return null;
    }
  }

  Future<void> _login() async {
    setState(() {
      _busy = true;
      _status = '登录中…（将唤起系统浏览器）';
      _detail = '';
    });
    try {
      // flutter_appauth 默认走系统浏览器组件（iOS ASWebAuthenticationSession /
      // Android Chrome Custom Tabs），登录后系统浏览器留存 Keycloak SSO 会话，
      // 日后扫码确认可免输密码。PKCE 默认开启。
      final appAuth = FlutterAppAuth();
      final result = await appAuth.authorizeAndExchangeCode(
        AuthorizationTokenRequest(
          kcClientId,
          kcRedirectUri,
          discoveryUrl: kcDiscoveryUrl,
          scopes: kcScopes,
        ),
      );
      final claims = _decodeSub(result.idToken, result.accessToken);
      setState(() {
        _status = '已登录 ✓';
        _detail =
            'subject: ${claims?['sub'] ?? '-'}\n'
            'username: ${claims?['preferred_username'] ?? '-'}\n'
            'expires_in: ${result.accessTokenExpirationDateTime ?? '-'}';
      });
    } on PlatformException catch (e) {
      setState(() {
        _status = '登录失败或被取消';
        _detail = e.toString();
      });
    } catch (e) {
      setState(() {
        _status = '错误';
        _detail = e.toString();
      });
    } finally {
      setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('星主认证 · 手机端')),
      body: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Text(_status, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            const SizedBox(height: 16),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.grey[200],
                borderRadius: BorderRadius.circular(8),
              ),
              child: SelectableText(_detail, style: const TextStyle(fontSize: 13)),
            ),
            const SizedBox(height: 24),
            ElevatedButton(
              onPressed: _busy ? null : _login,
              child: Text(_busy ? '请稍候…' : '使用 Keycloak 登录'),
            ),
            const SizedBox(height: 12),
            const Text(
              '登录后系统浏览器将留存 Keycloak 会话；\n日后在桌面 / XLL / 网页端扫码确认即可免输密码。',
              textAlign: TextAlign.center,
              style: TextStyle(fontSize: 12, color: Colors.grey),
            ),
          ],
        ),
      ),
    );
  }
}
