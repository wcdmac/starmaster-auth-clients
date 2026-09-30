// Keycloak 配置（按部署环境修改）
const String kcBase = 'https://keycloak.starmaster.us.ci';
const String kcRealm = 'master';
const String kcClientId = 'mobile-app';
const String kcRedirectUri = 'myapp://callback';
const String kcDiscoveryUrl =
    '$kcBase/realms/$kcRealm/.well-known/openid-configuration';
const List<String> kcScopes = ['openid', 'profile', 'email'];
