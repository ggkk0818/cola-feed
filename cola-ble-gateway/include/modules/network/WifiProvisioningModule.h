#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>

#include <functional>

#include "modules/display/LcdModule.h"
#include "modules/storage/TfCardModule.h"

class WifiProvisioningModule {
 public:
  enum class ConnectionState {
    kDisconnected,
    kConnecting,
    kConnected,
    kProvisioning,
  };

  using StateChangedCallback = std::function<void(ConnectionState)>;

  WifiProvisioningModule(LcdModule& lcd, TfCardModule& tfCard);

  void begin();
  void loop();
  void setStateChangedCallback(StateChangedCallback callback);

  bool isConnected() const;
  bool isProvisioningMode() const;
  ConnectionState currentState() const;

 private:
  struct WifiCredentials {
    String ssid;
    String password;

    bool isValid() const { return !ssid.isEmpty() && !password.isEmpty(); }
  };

  bool loadSavedCredentials(WifiCredentials* credentials);
  void saveCredentials(const WifiCredentials& credentials);

  bool tryConnectStation(const WifiCredentials& credentials, bool saveOnSuccess);
  void enterProvisioningMode();

  void setupWebServer();
  void handleRootPage();
  void handleWifiList();
  void handleRsaPublicKey();
  void handleWifiConnect();
  void handleNotFound();

  bool serveFileFromSd(const String& requestUri);
  String normalizeUriPath(const String& requestUri) const;
  bool isCaptivePortalUri(const String& requestUri) const;
  String contentTypeForPath(const String& filePath) const;

  String makeApSsid() const;

  bool decryptPassword(const String& encryptedPassword, String* decryptedPassword) const;

  void setState(ConnectionState state);

  void showProvisioningScreen() const;
  void showConnectingScreen(const String& ssid) const;
  void showConnectResult(bool success, const String& message) const;

  LcdModule& lcd_;
  TfCardModule& tfCard_;

  Preferences preferences_;
  DNSServer dnsServer_;
  WebServer webServer_;

  bool sdMounted_ = false;
  bool provisioningModeActive_ = false;
  bool webServerConfigured_ = false;

  bool pendingConnect_ = false;
  WifiCredentials pendingCredentials_;
  String apSsid_;

  ConnectionState state_ = ConnectionState::kDisconnected;
  StateChangedCallback stateChangedCallback_;
};
