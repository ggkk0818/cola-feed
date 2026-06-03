#pragma once

#include <Arduino.h>

#include "modules/display/DisplayDriver.h"
#include "modules/weather/WeatherData.h"

class DisplayModule {
 public:
  DisplayModule();

  void begin();
  void renderMainPage();
  void setMainPageBatteryStatus(uint8_t batteryPercentage, bool isCharging,
                                bool isPowerConnected);
  void setMainPageTopTime(const String& topTimeText);
  void setMainPageSidebarWeatherData(const WeatherData::SidebarWeatherData& data);
  void setMainPageOutdoorEnvironmentData(const WeatherData::OutdoorEnvironmentData& data);
  void setMainPageIndoorEnvironmentData(const WeatherData::IndoorEnvironmentData& data);
  void setMainPageForecastData(const WeatherData::DailyForecastData* forecastDays,
                               size_t forecastDayCount);
  void setMainPageContentData(const String& durationText, const String& timestampText,
                              bool showTimestamp);
  void renderLowBattery();
  void renderLogo();
  void renderFontCN16Test();
  void renderFontCN32Test();
  void hibernate();

 private:
  enum class MainPageRegion : uint8_t {
    kTop,
    kSidebarOutdoor,
    kSidebarIndoor,
    kSidebarForecast,
    kContent,
  };

  struct MainPageRegionBounds {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
  };

  struct MainPageRegionState {
    uint8_t consecutivePartialRefreshes;
    bool dirty;
    bool containsRed;
    bool willDrawRed;
  };

  MainPageRegionBounds getMainPageTopBounds() const;
  MainPageRegionBounds getMainPageSidebarBounds() const;
  MainPageRegionBounds getMainPageSidebarUpperBounds() const;
  MainPageRegionBounds getMainPageSidebarOutdoorBounds() const;
  MainPageRegionBounds getMainPageSidebarIndoorBounds() const;
  MainPageRegionBounds getMainPageSidebarForecastBounds() const;
  MainPageRegionBounds getMainPageContentBounds() const;
  MainPageRegionBounds getMainPageRegionBounds(MainPageRegion region) const;

  MainPageRegionState& getMainPageRegionState(MainPageRegion region);
  const MainPageRegionState& getMainPageRegionState(MainPageRegion region) const;

  bool shouldRenderMainPageFullRefresh() const;
  bool shouldRenderMainPageRegion(MainPageRegion region) const;
  bool shouldRenderMainPageSidebarUpperRegion() const;

  void renderMainPageFullRefresh();
  void renderMainPagePartialRefresh(MainPageRegion region);
  void renderMainPageSidebarUpperPartialRefresh();
  void renderMainPageLayout();
  void renderMainPageRegion(MainPageRegion region);
  void renderBatteryStatusIcon(int16_t x, int16_t y, uint8_t batteryPercentage);
  void renderMainPageTopRegion(const MainPageRegionBounds& bounds);
  void renderMainPageSidebarRegion(const MainPageRegionBounds& bounds);
  void renderMainPageSidebarOutdoorRegion(const MainPageRegionBounds& bounds);
  void renderMainPageSidebarIndoorRegion(const MainPageRegionBounds& bounds);
  void renderMainPageSidebarForecastRegion(const MainPageRegionBounds& bounds);
  void renderMainPageContentRegion(const MainPageRegionBounds& bounds);
  void updateMainPageRegionStateAfterRefresh(MainPageRegion region, bool fullRefresh);
  void resetMainPageRegionStateAfterFullRefresh();
  void markMainPageRegionDirty(MainPageRegion region, bool willDrawRed = false);

  DisplayDriver display_;
  MainPageRegionState mainPageTopState_{0, true, false, false};
  MainPageRegionState mainPageSidebarOutdoorState_{0, true, false, false};
  MainPageRegionState mainPageSidebarIndoorState_{0, true, false, false};
  MainPageRegionState mainPageSidebarForecastState_{0, true, false, false};
  MainPageRegionState mainPageContentState_{0, true, false, false};
  uint8_t mainPageBatteryPercentage_ = 70;
  bool mainPageCharging_ = true;
  bool mainPagePowerConnected_ = true;
  String mainPageTopTime_ = "--:--";
  String mainPageContentDuration_ = "--";
  String mainPageContentTimestamp_;
  bool mainPageContentTimestampVisible_ = false;
  WeatherData::SidebarWeatherData sidebarWeatherData_;
  bool hasRenderedMainPage_ = false;
};
