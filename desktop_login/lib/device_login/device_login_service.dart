/// Device Grant（RFC 8628）设备端核心逻辑 —— 纯 Dart，无 UI 依赖。
library;

import 'dart:async';
import 'dart:convert';
import 'package:http/http.dart' as http;

class DeviceCodeResponse {
  final String deviceCode;
  final String userCode;
  final String verificationUri;
  final String verificationUriComplete;
  final int expiresIn;
  final int interval;
  const DeviceCodeResponse({
    required this.deviceCode,
    required this.userCode,
    required this.verificationUri,
    required this.verificationUriComplete,
    required this.expiresIn,
    required this.interval,
  });
  factory DeviceCodeResponse.fromJson(Map<String, dynamic> j) => DeviceCodeResponse(
        deviceCode: j['device_code'] as String,
        userCode: j['user_code'] as String,
        verificationUri: j['verification_uri'] as String,
        verificationUriComplete: j['verification_uri_complete'] as String,
        expiresIn: (j['expires_in'] as num).toInt(),
        interval: (j['interval'] as num? ?? 5).toInt(),
      );
}

class TokenResponse {
  final String accessToken;
  final String? refreshToken;
  final String? idToken;
  final int expiresIn;
  const TokenResponse({
    required this.accessToken,
    this.refreshToken,
    this.idToken,
    required this.expiresIn,
  });
  factory TokenResponse.fromJson(Map<String, dynamic> j) => TokenResponse(
        accessToken: j['access_token'] as String,
        refreshToken: j['refresh_token'] as String?,
        idToken: j['id_token'] as String?,
        expiresIn: (j['expires_in'] as num).toInt(),
      );
}

enum PollStatus { pending, slowDown, expired, success, error }

class PollResult {
  final PollStatus status;
  final TokenResponse? token;
  final String? message;
  const PollResult._(this.status, {this.token, this.message});
  factory PollResult.success(TokenResponse t) => PollResult._(PollStatus.success, token: t);
  factory PollResult.pending() => const PollResult._(PollStatus.pending);
  factory PollResult.slowDown() => const PollResult._(PollStatus.slowDown);
  factory PollResult.expired() => const PollResult._(PollStatus.expired);
  factory PollResult.error(String m) => PollResult._(PollStatus.error, message: m);
}

class DeviceLoginService {
  final String baseUrl;
  final String realm;
  final String clientId;
  final String scope;
  final http.Client _client;

  DeviceLoginService({
    this.baseUrl = 'https://keycloak.starmaster.us.ci',
    this.realm = 'master',
    this.clientId = 'device-login',
    this.scope = 'openid',
    http.Client? client,
  }) : _client = client ?? http.Client();

  String get _deviceEndpoint =>
      '$baseUrl/realms/$realm/protocol/openid-connect/auth/device';
  String get _tokenEndpoint =>
      '$baseUrl/realms/$realm/protocol/openid-connect/token';

  Future<DeviceCodeResponse> requestDeviceCode() async {
    final resp = await _client.post(
      Uri.parse(_deviceEndpoint),
      body: {'client_id': clientId, 'scope': scope},
    );
    if (resp.statusCode != 200) {
      throw DeviceLoginException('requestDeviceCode failed: ${resp.statusCode} ${resp.body}');
    }
    return DeviceCodeResponse.fromJson(json.decode(resp.body) as Map<String, dynamic>);
  }

  Future<PollResult> pollOnce(DeviceCodeResponse dc) async {
    final resp = await _client.post(
      Uri.parse(_tokenEndpoint),
      body: {
        'grant_type': 'urn:ietf:params:oauth:grant-type:device_code',
        'client_id': clientId,
        'device_code': dc.deviceCode,
      },
    );
    final body = json.decode(resp.body) as Map<String, dynamic>;
    if (resp.statusCode == 200) return PollResult.success(TokenResponse.fromJson(body));
    final error = body['error'] as String? ?? 'unknown_error';
    switch (error) {
      case 'authorization_pending':
        return PollResult.pending();
      case 'slow_down':
        return PollResult.slowDown();
      case 'expired_token':
        return PollResult.expired();
      default:
        return PollResult.error(body['error_description'] as String? ?? error);
    }
  }

  Stream<PollResult> pollLoop(DeviceCodeResponse dc) async* {
    var interval = dc.interval;
    final deadline = DateTime.now().add(Duration(seconds: dc.expiresIn));
    while (DateTime.now().isBefore(deadline)) {
      await Future.delayed(Duration(seconds: interval));
      final r = await pollOnce(dc);
      yield r;
      switch (r.status) {
        case PollStatus.slowDown:
          interval += 5;
        case PollStatus.success:
        case PollStatus.expired:
        case PollStatus.error:
          return;
        case PollStatus.pending:
          break;
      }
    }
    yield PollResult.expired();
  }

  void dispose() => _client.close();
}

class DeviceLoginException implements Exception {
  final String message;
  const DeviceLoginException(this.message);
  @override
  String toString() => 'DeviceLoginException: $message';
}
