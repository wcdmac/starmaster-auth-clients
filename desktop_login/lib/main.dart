import 'package:flutter/material.dart';
import 'device_login/device_login_service.dart';
import 'device_login/device_login_widget.dart';

void main() => runApp(const MyApp());

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: '星主认证 · 桌面端',
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
  String _detail = '未登录';

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('星主认证 · 桌面端扫码登录')),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            DeviceLoginWidget(
              onSuccess: (TokenResponse t) {
                setState(() => _detail = '已登录 ✓\naccess_token 前20位：\n${t.accessToken.substring(0, 20)}…');
              },
              onError: (e) => setState(() => _detail = '错误：$e'),
            ),
            const SizedBox(height: 16),
            SelectableText(_detail, textAlign: TextAlign.center),
          ],
        ),
      ),
    );
  }
}
