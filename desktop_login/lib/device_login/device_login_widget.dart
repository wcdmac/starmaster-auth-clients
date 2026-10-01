/// 设备端扫码登录 UI 组件（Flutter 桌面）。将 DeviceLoginService 的轮询流接成界面状态。
library device_login_widget;

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:qr_flutter/qr_flutter.dart';
import 'device_login_service.dart';

class DeviceLoginWidget extends StatefulWidget {
  final String baseUrl;
  final String realm;
  final String clientId;
  final String scope;
  final void Function(TokenResponse token) onSuccess;
  final void Function(Object error)? onError;
  const DeviceLoginWidget({
    super.key,
    this.baseUrl = 'https://keycloak.starmaster.us.ci',
    this.realm = 'master',
    this.clientId = 'device-login',
    this.scope = 'openid',
    required this.onSuccess,
    this.onError,
  });
  @override
  State<DeviceLoginWidget> createState() => _DeviceLoginWidgetState();
}

class _DeviceLoginWidgetState extends State<DeviceLoginWidget> {
  late final DeviceLoginService _svc;
  DeviceCodeResponse? _dc;
  String _status = '正在申请设备码…';
  Color _statusColor = Colors.grey;
  int _remaining = 0;
  StreamSubscription<PollResult>? _sub;
  Timer? _countdown;

  @override
  void initState() {
    super.initState();
    _svc = DeviceLoginService(
      baseUrl: widget.baseUrl,
      realm: widget.realm,
      clientId: widget.clientId,
      scope: widget.scope,
    );
    _start();
  }

  Future<void> _start() async {
    setState(() {
      _status = '正在申请设备码…';
      _statusColor = Colors.grey;
      _dc = null;
    });
    try {
      final dc = await _svc.requestDeviceCode();
      if (!mounted) return;
      setState(() {
        _dc = dc;
        _remaining = dc.expiresIn;
      });
      _countdown = Timer.periodic(const Duration(seconds: 1), (_) {
        if (!mounted) return;
        setState(() => _remaining = _remaining > 0 ? _remaining - 1 : 0);
      });
      _subscribe(dc);
    } on DeviceLoginException catch (e) {
      _fail(e.message);
    } catch (e) {
      _fail(e.toString());
    }
  }

  void _subscribe(DeviceCodeResponse dc) {
    _sub = _svc.pollLoop(dc).listen((r) {
      if (!mounted) return;
      switch (r.status) {
        case PollStatus.pending:
          _setStatus('等待手机端授权…（剩余 $_remaining s）', Colors.grey);
        case PollStatus.slowDown:
          _setStatus('降低轮询频率，等待授权…', Colors.orange);
        case PollStatus.success:
          _countdown?.cancel();
          _setStatus('登录成功', Colors.green);
          widget.onSuccess(r.token!);
        case PollStatus.expired:
          _countdown?.cancel();
          _setStatus('二维码已过期，请重新生成', Colors.orange);
        case PollStatus.error:
          _countdown?.cancel();
          _fail(r.message ?? '未知错误');
      }
    });
  }

  void _setStatus(String s, Color c) => setState(() {
        _status = s;
        _statusColor = c;
      });
  void _fail(String msg) {
    if (!mounted) return;
    _setStatus('错误：$msg', Colors.red);
    widget.onError?.call(msg);
  }

  @override
  void dispose() {
    _sub?.cancel();
    _countdown?.cancel();
    _svc.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.all(16),
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Text('扫码登录', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            const SizedBox(height: 4),
            const Text('用已登录的手机扫描二维码完成授权',
                style: TextStyle(fontSize: 12, color: Colors.grey)),
            const SizedBox(height: 20),
            SizedBox(
              width: 220,
              height: 220,
              child: _dc == null
                  ? const CircularProgressIndicator()
                  : QrImageView(
                      data: _dc!.verificationUriComplete,
                      version: QrVersions.auto,
                      size: 220,
                    ),
            ),
            const SizedBox(height: 12),
            if (_dc != null)
              SelectableText(_dc!.userCode,
                  style: const TextStyle(
                      fontSize: 24, letterSpacing: 3, fontWeight: FontWeight.bold, color: Colors.blue)),
            const SizedBox(height: 12),
            Text(_status, style: TextStyle(color: _statusColor)),
            const SizedBox(height: 8),
            if (_dc != null)
              Text('二维码有效期剩余 $_remaining s',
                  style: const TextStyle(fontSize: 11, color: Colors.grey)),
            if (_status.contains('过期') || _status.contains('错误'))
              TextButton(onPressed: _start, child: const Text('重新生成二维码')),
          ],
        ),
      ),
    );
  }
}
