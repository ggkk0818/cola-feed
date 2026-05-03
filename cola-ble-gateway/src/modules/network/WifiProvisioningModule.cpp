#include "modules/network/WifiProvisioningModule.h"

#include <ArduinoJson.h>
#include <ESP.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>

#include <cstring>
#include <memory>

namespace {
constexpr const char* kPrefsNamespace = "wifi_cfg";
constexpr const char* kPrefsSsidKey = "ssid";
constexpr const char* kPrefsPasswordKey = "password";

constexpr const char* kApPassword = "12345678";
constexpr uint8_t kDnsPort = 53;
constexpr uint32_t kConnectTimeoutMs = 15000;

constexpr const char* kHtmlRootPath = "/html";

constexpr const char* kRsaPublicKeyPem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA8cX7+asMYNEhAM9F3iTp\n"
    "woTeBWzhnQhibR1Taf9XwsqUQJXDHa/uNyoKRMPd49WKijvwtrOQuKxoVNlaSzUX\n"
    "X9y/4KB0cU0Zditdq5iGaN1kdt/tBrzUiG1kmJHCZfa2+G+n837tqYjbS8tLXRAX\n"
    "aqNEYf4NsNYoOD002/lmtBJPEMVqcCkGi0NBP03X5q/Otvh3uDm23Kann0yRp4dz\n"
    "0hPxzlid5f64L18sDMqZAvIewnh2O/baWNxvmgdwANX0iVFhBKIEIvioI2YMXZcy\n"
    "9m0HtZLCeH3p2UxMfbGWquAS7B41k/9mLBlq+JfJWJIVSdW2L/NVj3tN59oQ//Ij\n"
    "CQIDAQAB\n"
    "-----END PUBLIC KEY-----";

constexpr const char* kRsaPrivateKeyPem =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-128-CBC,9E39C7C1E892FDB24825F7549AB7B1EC\n"
    "\n"
    "zR4262DS6G5ka28Q38c3HhYudj8PKIh+dpCBENUeHYaEkIZyByutW43LZS3PqI+N\n"
    "hOqGjuG2SII9JjDiT/FAo/Vi9ahFeo1BjqL8t5jKAaxiCDtfsBNf/7yk4IQKdEw7\n"
    "tJndUfQtvV5JV4qM1gvJ3efNPEWEZg/OSgxnRucJrc2fVi538azdpQJUqPuNTkVr\n"
    "INbnlQ6rYg2nQlvCmeMKmhYN4lI/v3u0DL2FBytqGjabj3jm/MDj95QqxMNsLUJ+\n"
    "sAytNMckdDFEL4BTxttJ0f7F8rTzuL+xA6+XWKfKwIcvfWJvO/hCJO6ycQrPg315\n"
    "U5H/ZqGV7MnplNkD8/N+dKmtK839T022aDb2IvpKj3yNSwoXDqDLAuoC5SQEuGbe\n"
    "nAQ+wMeRBp0NPQxq3dE80xEG0wl08tMDmWQiaOoy3kbKacNdwlxeFl8/9u1f2XMD\n"
    "KB3hmf035JGagEhHZRBA6c1GMqggbGwceNI/XUQ3Hc/6/Juh/jM3bwNLwmQ0BZJR\n"
    "aDQIliQtJ/sIo66D9Ftz+5gJl6+Jv75Lr7OKlTNUvbV0xgT7EJsodj3mE5TxxSQF\n"
    "hFC61Z8XYXuS5e5nsySmO/Yt5J42PqKpd39o8EZ87LyVHkxkGsJMtDH5N5XWUh/K\n"
    "ZBNyUseK/zjosYA6T4sh6+yuiTuRp5KtRkTgVSecnsMDwW9Hdyuq/8tjO1jCnBOa\n"
    "6GeKcWXtUhI3p7IdMhvZYpBTAtflGya4vHbwQ0D5bY7gyuPsv3hePnr4iuGy5jX2\n"
    "fGBQo3B5RKKmBPz/PtXf6yWQxodrtlbfo2FCp1aN5Qx5WV+PtUhd/8fcZjK0459X\n"
    "T1K4SshOJ3IDF+bmhdOdZoJOa4w7oIKsAoCwF+JE/zhIY8+bh/A6s8HO5jZKXRr2\n"
    "rIrnH3LvN+0S5UpTPSfKBND3tTnV0g0D3KgfV0VDWo81qIUliVrk6Vi564WvvD01\n"
    "dyDJbiyv4sX0tkiJ32JbuoDYZcdfgNe52krNn1toyoBrHEhKuf7lBbYByEFss2fw\n"
    "a5H2dD/dFeHD8e03+qZ1uIJcTc0bgPgGDPjTutGC8mhOX5TXx3+m+p4FaZCsa3CA\n"
    "r/HQJT3t9LDAkELosaPhnijtlRA+7POBYvADc1nNQINUj0gbWrXEtqpJGgcBAfIf\n"
    "1VNLDrJbQs+wEpakwWL/KcI4dOcI4hy+JD3lMJEiN7M6hhuALfE4q9E05AzJiTaZ\n"
    "VfrnBjghxb0Sp043YTXc6ryjinADJy/UiYNMp7Xu5Fxu413bGruZyZrfxlvhSO+z\n"
    "wWQaM/mIsNvuu/Jhfo6TvEgTV0s5bPP8t3s6WHxiAj94yN58oZ0rzudiIPLsFnBD\n"
    "W/fHml3Cowii/f33aVNLRXq1Ok1QIb/4d7ZgFt3CQT2ItKcD31ls1uiG2HKFxi9V\n"
    "wABqIKChaTUBiQZmTjpcsUY8BJxeQB3J6VEfBWD3ym5zgAVqpmpZ6Cg20BBJtkPw\n"
    "3ggqGri+z1jGubnEtwJ7TIXD44qes62EIv2Gcis5K+sNTuXw3TNS2eOlNxp4HKjj\n"
    "232YqGW6vk3ojwQEoY+wGdfCM2UDeaM0qmub4UBZE/ITewr2qtYS3nwLfI9cKKP+\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr const char* kRsaPrivateKeyPassword = "abc123";

}  // namespace

WifiProvisioningModule::WifiProvisioningModule(LcdModule& lcd, TfCardModule& tfCard)
    : lcd_(lcd), tfCard_(tfCard), webServer_(80) {}

void WifiProvisioningModule::setStateChangedCallback(StateChangedCallback callback) {
  stateChangedCallback_ = callback;
}

bool WifiProvisioningModule::isConnected() const {
  return state_ == ConnectionState::kConnected;
}

bool WifiProvisioningModule::isProvisioningMode() const {
  return provisioningModeActive_;
}

WifiProvisioningModule::ConnectionState WifiProvisioningModule::currentState() const {
  return state_;
}

void WifiProvisioningModule::setState(ConnectionState state) {
  if (state_ == state) {
    return;
  }

  state_ = state;
  if (stateChangedCallback_) {
    stateChangedCallback_(state_);
  }
}

void WifiProvisioningModule::begin() {
  preferences_.begin(kPrefsNamespace, false);

  if (!tfCard_.isMounted()) {
    sdMounted_ = tfCard_.begin();
  } else {
    sdMounted_ = true;
  }

  WifiCredentials savedCredentials;
  if (loadSavedCredentials(&savedCredentials) && savedCredentials.isValid()) {
    if (tryConnectStation(savedCredentials, false)) {
      return;
    }
  }

  enterProvisioningMode();
}

void WifiProvisioningModule::loop() {
  if (provisioningModeActive_) {
    dnsServer_.processNextRequest();
    webServer_.handleClient();
  }

  // If station mode unexpectedly drops (router reboot, signal loss, etc.),
  // leave BLE mode and return to provisioning mode automatically.
  if (!provisioningModeActive_ && !pendingConnect_ && isConnected() && WiFi.status() != WL_CONNECTED) {
    setState(ConnectionState::kDisconnected);
    enterProvisioningMode();
    return;
  }

  if (!pendingConnect_) {
    return;
  }

  pendingConnect_ = false;
  provisioningModeActive_ = false;

  dnsServer_.stop();
  webServer_.stop();

  if (!tryConnectStation(pendingCredentials_, true)) {
    enterProvisioningMode();
  }
}

bool WifiProvisioningModule::loadSavedCredentials(WifiCredentials* credentials) {
  if (credentials == nullptr) {
    return false;
  }

  credentials->ssid = preferences_.getString(kPrefsSsidKey, "");
  credentials->password = preferences_.getString(kPrefsPasswordKey, "");
  credentials->ssid.trim();
  credentials->password.trim();
  return credentials->isValid();
}

void WifiProvisioningModule::saveCredentials(const WifiCredentials& credentials) {
  preferences_.putString(kPrefsSsidKey, credentials.ssid);
  preferences_.putString(kPrefsPasswordKey, credentials.password);
}

bool WifiProvisioningModule::tryConnectStation(const WifiCredentials& credentials, bool saveOnSuccess) {
  showConnectingScreen(credentials.ssid);
  setState(ConnectionState::kConnecting);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);

  WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str());

  const uint32_t startMs = millis();
  while (millis() - startMs < kConnectTimeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (saveOnSuccess) {
      saveCredentials(credentials);
    }

    provisioningModeActive_ = false;

    showConnectResult(true, WiFi.localIP().toString());

    delay(3000);

    // Notify external modules (such as BLE) after connection result prompt finishes.
    setState(ConnectionState::kConnected);

    return true;
  }

  setState(ConnectionState::kDisconnected);
  showConnectResult(false, "请检查账号或密码");
  delay(10000);
  return false;
}

void WifiProvisioningModule::enterProvisioningMode() {
  WiFi.disconnect(true, true);
  delay(200);

  WiFi.mode(WIFI_AP_STA);
  apSsid_ = makeApSsid();
  WiFi.softAP(apSsid_.c_str(), kApPassword);

  if (!tfCard_.isMounted()) {
    sdMounted_ = tfCard_.begin();
  } else {
    sdMounted_ = true;
  }

  setupWebServer();

  dnsServer_.stop();
  dnsServer_.start(kDnsPort, "*", WiFi.softAPIP());

  provisioningModeActive_ = true;
  setState(ConnectionState::kProvisioning);
  showProvisioningScreen();
}

void WifiProvisioningModule::setupWebServer() {
  if (!webServerConfigured_) {
    webServer_.on("/", HTTP_GET, [this]() { handleRootPage(); });

    webServer_.on("/api/wifi/list", HTTP_GET, [this]() { handleWifiList(); });
    webServer_.on("/api/rsa/public_key", HTTP_GET, [this]() { handleRsaPublicKey(); });
    webServer_.on("/api/wifi/connect", HTTP_POST, [this]() { handleWifiConnect(); });

    webServer_.on("/generate_204", HTTP_GET, [this]() {
      webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      webServer_.send(302, "text/plain", "");
    });
    webServer_.on("/hotspot-detect.html", HTTP_GET, [this]() {
      webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      webServer_.send(302, "text/plain", "");
    });
    webServer_.on("/connecttest.txt", HTTP_GET, [this]() {
      webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      webServer_.send(302, "text/plain", "");
    });
    webServer_.on("/ncsi.txt", HTTP_GET, [this]() {
      webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      webServer_.send(302, "text/plain", "");
    });
    webServer_.on("/fwlink", HTTP_GET, [this]() {
      webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      webServer_.send(302, "text/plain", "");
    });

    webServer_.onNotFound([this]() { handleNotFound(); });
    webServerConfigured_ = true;
  }

  webServer_.begin();
}

void WifiProvisioningModule::handleRootPage() {
  if (!serveFileFromSd("/")) {
    webServer_.send(404, "text/plain;charset=utf-8", "404 Not Found");
  }
}

void WifiProvisioningModule::handleWifiList() {
  DynamicJsonDocument responseDoc(6144);
  responseDoc["code"] = 200;
  JsonArray data = responseDoc.createNestedArray("data");

  const int16_t count = WiFi.scanNetworks(false, true);
  if (count > 0) {
    for (int16_t i = 0; i < count; ++i) {
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) {
        continue;
      }
      JsonObject item = data.createNestedObject();
      item["ssid"] = ssid;
    }
  }
  WiFi.scanDelete();

  String body;
  serializeJson(responseDoc, body);
  webServer_.send(200, "application/json", body);
}

void WifiProvisioningModule::handleRsaPublicKey() {
  DynamicJsonDocument responseDoc(2048);
  responseDoc["code"] = 200;
  JsonObject data = responseDoc.createNestedObject("data");
  data["public_key"] = kRsaPublicKeyPem;

  String body;
  serializeJson(responseDoc, body);
  webServer_.send(200, "application/json", body);
}

void WifiProvisioningModule::handleWifiConnect() {
  DynamicJsonDocument requestDoc(2048);
  DeserializationError parseError = deserializeJson(requestDoc, webServer_.arg("plain"));
  if (parseError) {
    DynamicJsonDocument errorDoc(256);
    errorDoc["code"] = 400;
    errorDoc["message"] = "请求体格式错误";

    String body;
    serializeJson(errorDoc, body);
    webServer_.send(400, "application/json", body);
    return;
  }

  const String ssid = String(requestDoc["ssid"] | "");
  const String incomingPassword = String(requestDoc["password"] | "");

  if (ssid.isEmpty() || incomingPassword.isEmpty()) {
    DynamicJsonDocument errorDoc(256);
    errorDoc["code"] = 400;
    errorDoc["message"] = "ssid 或 password 不能为空";

    String body;
    serializeJson(errorDoc, body);
    webServer_.send(400, "application/json", body);
    return;
  }

  String plainPassword;
  if (!decryptPassword(incomingPassword, &plainPassword)) {
    if (incomingPassword.length() > 63) {
      DynamicJsonDocument errorDoc(256);
      errorDoc["code"] = 400;
      errorDoc["message"] = "密码解密失败";

      String body;
      serializeJson(errorDoc, body);
      webServer_.send(400, "application/json", body);
      return;
    }

    plainPassword = incomingPassword;
  }

  pendingCredentials_.ssid = ssid;
  pendingCredentials_.password = plainPassword;
  pendingConnect_ = true;

  DynamicJsonDocument responseDoc(256);
  responseDoc["code"] = 200;
  responseDoc["message"] = "连接请求已发送";

  String body;
  serializeJson(responseDoc, body);
  webServer_.send(200, "application/json", body);
}

void WifiProvisioningModule::handleNotFound() {
  const String uri = webServer_.uri();

  if (isCaptivePortalUri(uri)) {
    webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    webServer_.send(302, "text/plain", "");
    return;
  }

  if (!serveFileFromSd(uri)) {
    webServer_.send(404, "text/plain;charset=utf-8", "404 Not Found");
  }
}

bool WifiProvisioningModule::serveFileFromSd(const String& requestUri) {
  if (!sdMounted_ || !tfCard_.isMounted()) {
    return false;
  }

  if (!SD_MMC.exists(kHtmlRootPath)) {
    return false;
  }

  String normalizedPath = normalizeUriPath(requestUri);
  if (normalizedPath.isEmpty()) {
    return false;
  }

  const String fullPath = String(kHtmlRootPath) + normalizedPath;
  if (!SD_MMC.exists(fullPath.c_str())) {
    return false;
  }

  File file = SD_MMC.open(fullPath, FILE_READ);
  if (!file || file.isDirectory()) {
    return false;
  }

  webServer_.streamFile(file, contentTypeForPath(fullPath));
  file.close();
  return true;
}

String WifiProvisioningModule::normalizeUriPath(const String& requestUri) const {
  String path = requestUri;

  const int32_t queryPos = path.indexOf('?');
  if (queryPos >= 0) {
    path = path.substring(0, queryPos);
  }

  if (path.isEmpty()) {
    path = "/";
  }

  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  if (path.indexOf("..") >= 0) {
    return "";
  }

  if (path.endsWith("/")) {
    path += "index.html";
  }

  if (path == "/") {
    path = "/index.html";
  }

  return path;
}

bool WifiProvisioningModule::isCaptivePortalUri(const String& requestUri) const {
  return requestUri == "/generate_204" || requestUri == "/hotspot-detect.html" ||
         requestUri == "/connecttest.txt" || requestUri == "/ncsi.txt" || requestUri == "/fwlink";
}

String WifiProvisioningModule::contentTypeForPath(const String& filePath) const {
  if (filePath.endsWith(".html")) {
    return "text/html;charset=utf-8";
  }
  if (filePath.endsWith(".css")) {
    return "text/css";
  }
  if (filePath.endsWith(".js")) {
    return "application/javascript";
  }
  if (filePath.endsWith(".json")) {
    return "application/json";
  }
  if (filePath.endsWith(".png")) {
    return "image/png";
  }
  if (filePath.endsWith(".jpg") || filePath.endsWith(".jpeg")) {
    return "image/jpeg";
  }
  if (filePath.endsWith(".svg")) {
    return "image/svg+xml";
  }
  if (filePath.endsWith(".ico")) {
    return "image/x-icon";
  }
  return "text/plain";
}

String WifiProvisioningModule::makeApSsid() const {
  const uint16_t suffix = static_cast<uint16_t>(ESP.getEfuseMac() & 0xFFFF);
  char nameBuffer[32] = {0};
  snprintf(nameBuffer, sizeof(nameBuffer), "Cola-gateway-%04X", suffix);
  return String(nameBuffer);
}

bool WifiProvisioningModule::decryptPassword(const String& encryptedPassword,
                                             String* decryptedPassword) const {
  if (decryptedPassword == nullptr || encryptedPassword.isEmpty()) {
    return false;
  }

  size_t decodedLen = 0;
  int ret = mbedtls_base64_decode(nullptr, 0, &decodedLen,
                                  reinterpret_cast<const unsigned char*>(encryptedPassword.c_str()),
                                  encryptedPassword.length());
  if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0) {
    return false;
  }

  if (decodedLen == 0) {
    return false;
  }

  std::unique_ptr<unsigned char[]> cipher(new unsigned char[decodedLen]);
  size_t actualDecodedLen = 0;
  ret = mbedtls_base64_decode(cipher.get(), decodedLen, &actualDecodedLen,
                              reinterpret_cast<const unsigned char*>(encryptedPassword.c_str()),
                              encryptedPassword.length());
  if (ret != 0 || actualDecodedLen == 0) {
    return false;
  }

  mbedtls_pk_context pk;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctrDrbg;

  mbedtls_pk_init(&pk);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctrDrbg);

  bool ok = false;

  do {
    const char* personal = "cola-wifi-rsa";
    ret = mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy,
                                reinterpret_cast<const unsigned char*>(personal),
                                strlen(personal));
    if (ret != 0) {
      break;
    }

    ret = mbedtls_pk_parse_key(
        &pk, reinterpret_cast<const unsigned char*>(kRsaPrivateKeyPem), strlen(kRsaPrivateKeyPem) + 1,
        reinterpret_cast<const unsigned char*>(kRsaPrivateKeyPassword), strlen(kRsaPrivateKeyPassword));
    if (ret != 0 || !mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) {
      break;
    }

    mbedtls_rsa_context* rsa = mbedtls_pk_rsa(pk);

    auto tryDecryptWithMd = [&](mbedtls_md_type_t mdType) -> bool {
      mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, mdType);

      std::unique_ptr<unsigned char[]> output(new unsigned char[512]);
      size_t outputLen = 0;
      const int decryptRet = mbedtls_rsa_rsaes_oaep_decrypt(
          rsa, mbedtls_ctr_drbg_random, &ctrDrbg, MBEDTLS_RSA_PRIVATE, nullptr, 0, &outputLen,
          cipher.get(), output.get(), 512);
      if (decryptRet != 0 || outputLen == 0) {
        return false;
      }

      *decryptedPassword = String(reinterpret_cast<const char*>(output.get()), outputLen);
      return true;
    };

    ok = tryDecryptWithMd(MBEDTLS_MD_SHA256) || tryDecryptWithMd(MBEDTLS_MD_SHA1);
  } while (false);

  mbedtls_pk_free(&pk);
  mbedtls_ctr_drbg_free(&ctrDrbg);
  mbedtls_entropy_free(&entropy);
  return ok;
}

void WifiProvisioningModule::showProvisioningScreen() const {
  lcd_.clearScreen(ST77XX_BLACK);
  lcd_.printUtf8("配网模式", 0, 18, ST77XX_YELLOW);
  lcd_.printText("WiFi:", 0, 38, ST77XX_WHITE, 1);
  lcd_.printText(apSsid_, 36, 38, ST77XX_WHITE, 1);
  lcd_.printText("PWD:", 0, 54, ST77XX_WHITE, 1);
  lcd_.printText(kApPassword, 36, 54, ST77XX_WHITE, 1);
}

void WifiProvisioningModule::showConnectingScreen(const String& ssid) const {
  lcd_.clearScreen(ST77XX_BLACK);
  lcd_.printUtf8("连接中...", 0, 20, ST77XX_WHITE);
  lcd_.printText(ssid, 0, 44, ST77XX_WHITE, 1);
}

void WifiProvisioningModule::showConnectResult(bool success, const String& message) const {
  lcd_.clearScreen(ST77XX_BLACK);
  lcd_.printUtf8(success ? "连接成功" : "连接失败", 0, 20, success ? ST77XX_GREEN : ST77XX_RED);
  lcd_.printText(message, 0, 44, ST77XX_WHITE, 1);
}
