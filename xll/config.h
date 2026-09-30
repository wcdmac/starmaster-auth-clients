// 设备端配置（按部署环境修改，应与 web/flutter 三端一致）
#pragma once

#ifndef DEVICE_LOGIN_CONFIG_H
#define DEVICE_LOGIN_CONFIG_H

// Keycloak 基地址（外部域名经 Nginx + CloudFlare Tunnel）
#define KC_BASE      L"https://keycloak.starmaster.us.ci"
#define KC_REALM     L"master"
#define KC_CLIENT_ID L"device-login"
#define KC_SCOPE     L"openid"

// device_code 超时与轮询间隔（应与 Keycloak client 配置一致）
#define DC_EXPIRES_IN 120
#define DC_INTERVAL   5

// token 落地文件（XLL 单文件方案：XLL 自己写，无需外部 helper）
#define TOKEN_FILE_NAME L"device_login_token.json"

#endif // DEVICE_LOGIN_CONFIG_H
