#include "modules/display/DisplayModule.h"

#include <Adafruit_GFX.h>
#include <SPI.h>

#include "modules/display/BatteryImage128x128.h"
#include "modules/display/BitmapFontRenderer.h"
#include "modules/display/ChargingImage24x24.h"
#include "modules/display/HumidityImage32x32.h"
#include "modules/display/LogoImage64x64.h"
#include "modules/display/PowerImage24x24.h"
#include "modules/display/TemperatureImage32x32.h"
#include "modules/display/weather-icons/100.h"
#include "modules/display/weather-icons/101.h"
#include "modules/display/weather-icons/102.h"
#include "modules/display/weather-icons/103.h"
#include "modules/display/weather-icons/104.h"
#include "modules/display/weather-icons/999.h"

namespace {
constexpr uint8_t EPD_SCK_PIN = 14;
constexpr uint8_t EPD_MOSI_PIN = 13;
constexpr uint8_t EPD_CS_PIN = 4;
constexpr uint8_t EPD_RST_PIN = 23;
constexpr uint8_t EPD_DC_PIN = 5;
constexpr uint8_t EPD_BUSY_PIN = 24;
constexpr int16_t kMainPageTopHeight = 120;
constexpr int16_t kMainPageSidebarWidth = 300;
constexpr int16_t kMainPageSidebarTopSectionHeight = 120;
constexpr int16_t kMainPageSidebarPanelWidth = kMainPageSidebarWidth / 2;
constexpr int16_t kMainPageBorderThickness = 2;
constexpr int16_t kMainPageTopTimeAreaWidth = 300;
constexpr uint8_t kMainPageMaxConsecutivePartialRefreshes = 5;
constexpr int16_t kMainPageSidebarPadding = 5;
constexpr int16_t kMainPageSidebarDashLength = 4;
constexpr int16_t kMainPageSidebarDashGap = 4;
constexpr int16_t kMainPageContentTextEdgeOffset = 12;
constexpr int16_t kBatteryStatusIconWidth = 44;
constexpr int16_t kBatteryStatusIconHeight = 24;
constexpr int16_t kBatteryStatusHeadWidth = 5;
constexpr int16_t kBatteryStatusBodyWidth = kBatteryStatusIconWidth - kBatteryStatusHeadWidth;
constexpr int16_t kBatteryStatusSegmentGap = 3;
constexpr int16_t kBatteryStatusInnerPadding = 3;
constexpr int16_t kBatteryStatusTopMargin = 5;
constexpr int16_t kBatteryStatusRightMargin = 5;
constexpr int16_t kBatteryStatusAuxIconSize = 24;
constexpr int16_t kBatteryStatusAuxIconGap = 4;
constexpr char kMainPageContentTitleText[] = "距离上次吃奶";

struct BitmapAsset {
  const uint8_t* bitmap;
  uint16_t width;
  uint16_t height;
};

WeatherData::SidebarWeatherData createMockSidebarWeatherData() {
  WeatherData::SidebarWeatherData data;
  data.outdoor.temp = "--";
  data.outdoor.icon = 999;
  data.outdoor.text = "未知";
  data.indoor.temp = "--";
  data.indoor.humidity = "--";
  return data;
}

bool isForecastDataBlank(const WeatherData::DailyForecastData& data) {
  return data.fxDate.isEmpty() && data.tempMax.isEmpty() && data.tempMin.isEmpty() &&
         data.iconDay == 0 && data.textDay.isEmpty();
}

bool isOutdoorEnvironmentDataEqual(const WeatherData::OutdoorEnvironmentData& left,
                                   const WeatherData::OutdoorEnvironmentData& right) {
  return left.temp == right.temp && left.icon == right.icon && left.text == right.text;
}

bool isIndoorEnvironmentDataEqual(const WeatherData::IndoorEnvironmentData& left,
                                  const WeatherData::IndoorEnvironmentData& right) {
  return left.temp == right.temp && left.humidity == right.humidity;
}

bool isDailyForecastDataEqual(const WeatherData::DailyForecastData& left,
                              const WeatherData::DailyForecastData& right) {
  return left.fxDate == right.fxDate && left.tempMax == right.tempMax &&
         left.tempMin == right.tempMin && left.iconDay == right.iconDay &&
         left.textDay == right.textDay;
}

bool tryParseUintValue(const String& text, uint32_t* value) {
  if (value == nullptr) {
    return false;
  }

  String normalized = text;
  normalized.trim();
  if (normalized.isEmpty()) {
    return false;
  }

  uint32_t parsedValue = 0;
  for (size_t index = 0; index < normalized.length(); ++index) {
    const char ch = normalized[index];
    if (ch < '0' || ch > '9') {
      return false;
    }

    parsedValue = (parsedValue * 10UL) + static_cast<uint32_t>(ch - '0');
  }

  *value = parsedValue;
  return true;
}

bool tryParseElapsedDurationMinutes(const String& durationText, uint32_t* totalMinutes) {
  if (totalMinutes == nullptr) {
    return false;
  }

  String normalized = durationText;
  normalized.trim();
  if (normalized.isEmpty() || normalized == "--") {
    return false;
  }

  const int minuteMarkerIndex = normalized.lastIndexOf('M');
  if (minuteMarkerIndex <= 0) {
    return false;
  }

  const int hourMarkerIndex = normalized.indexOf('H');
  uint32_t hours = 0;
  uint32_t minutes = 0;

  if (hourMarkerIndex >= 0) {
    if (!tryParseUintValue(normalized.substring(0, hourMarkerIndex), &hours) ||
        !tryParseUintValue(normalized.substring(hourMarkerIndex + 1, minuteMarkerIndex),
                           &minutes)) {
      return false;
    }
  } else if (!tryParseUintValue(normalized.substring(0, minuteMarkerIndex), &minutes)) {
    return false;
  }

  *totalMinutes = (hours * 60UL) + minutes;
  return true;
}

bool isElapsedDurationRefreshBoundary(const String& durationText) {
  uint32_t totalMinutes = 0;
  return tryParseElapsedDurationMinutes(durationText, &totalMinutes) && totalMinutes > 0 &&
         (totalMinutes % 5UL) == 0;
}

bool resolveWeatherIconAsset(uint16_t iconCode, BitmapAsset& asset) {
  switch (iconCode) {
    case 100:
      asset = BitmapAsset{WeatherIcon100::kBitmap, WeatherIcon100::kWidth, WeatherIcon100::kHeight};
      return true;
    case 101:
      asset = BitmapAsset{WeatherIcon101::kBitmap, WeatherIcon101::kWidth, WeatherIcon101::kHeight};
      return true;
    case 102:
      asset = BitmapAsset{WeatherIcon102::kBitmap, WeatherIcon102::kWidth, WeatherIcon102::kHeight};
      return true;
    case 103:
      asset = BitmapAsset{WeatherIcon103::kBitmap, WeatherIcon103::kWidth, WeatherIcon103::kHeight};
      return true;
    case 104:
      asset = BitmapAsset{WeatherIcon104::kBitmap, WeatherIcon104::kWidth, WeatherIcon104::kHeight};
      return true;
    case 999:
      asset = BitmapAsset{WeatherIcon999::kBitmap, WeatherIcon999::kWidth, WeatherIcon999::kHeight};
      return true;
    default:
      asset = BitmapAsset{WeatherIcon999::kBitmap, WeatherIcon999::kWidth, WeatherIcon999::kHeight};
      return false;
  }
}

void drawWeatherIcon(DisplayDriver& display, int16_t x, int16_t y, uint16_t iconCode) {
  BitmapAsset asset{};
  resolveWeatherIconAsset(iconCode, asset);
  display.drawBitmap(x, y, asset.bitmap, asset.width, asset.height, GxEPD_BLACK);
}

void drawHorizontalDashedLine(DisplayDriver& display, int16_t y, int16_t startX, int16_t endX) {
  if (endX < startX) {
    return;
  }

  for (int16_t x = startX; x <= endX; x += kMainPageSidebarDashLength + kMainPageSidebarDashGap) {
    int16_t dashEndX = x + kMainPageSidebarDashLength - 1;
    if (dashEndX > endX) {
      dashEndX = endX;
    }

    display.drawLine(x, y, dashEndX, y, GxEPD_BLACK);
  }
}

void drawVerticalDashedLine(DisplayDriver& display, int16_t x, int16_t startY, int16_t endY) {
  if (endY < startY) {
    return;
  }

  for (int16_t y = startY; y <= endY; y += kMainPageSidebarDashLength + kMainPageSidebarDashGap) {
    int16_t dashEndY = y + kMainPageSidebarDashLength - 1;
    if (dashEndY > endY) {
      dashEndY = endY;
    }

    display.drawLine(x, y, x, dashEndY, GxEPD_BLACK);
  }
}

String formatTemperatureValue(const String& temp) {
  if (temp.isEmpty()) {
    return String();
  }

  return temp + "°C";
}

String formatHumidityValue(const String& humidity) {
  if (humidity.isEmpty()) {
    return String();
  }

  return humidity + "%";
}

String formatForecastTemperatureRange(const WeatherData::DailyForecastData& forecast) {
  if (forecast.tempMin.isEmpty() || forecast.tempMax.isEmpty()) {
    return String();
  }

  return forecast.tempMin + "~" + forecast.tempMax + "°C";
}

String formatForecastTitle(size_t forecastIndex, const WeatherData::DailyForecastData& forecast) {
  if (forecastIndex == 0) {
    return String("今日");
  }

  if (forecastIndex == 1) {
    return String("明日");
  }

  if (forecast.fxDate.length() >= 2) {
    return forecast.fxDate.substring(forecast.fxDate.length() - 2) + "日";
  }

  return String("--日");
}

uint8_t resolveBatterySegmentCount(uint8_t batteryPercentage) {
  if (batteryPercentage >= 90) {
    return 3;
  }

  if (batteryPercentage >= 60) {
    return 2;
  }

  if (batteryPercentage >= 30) {
    return 1;
  }

  return 0;
}
}  // namespace

DisplayModule::DisplayModule()
    : display_(GxEPD2_750c_GDEY075Z08(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN)),
      sidebarWeatherData_(createMockSidebarWeatherData()) {}

void DisplayModule::begin() {
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display_.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  display_.init(0);
  display_.setRotation(0);
  display_.setFullWindow();
}

void DisplayModule::setDeviceOrientation(DeviceOrientation orientation) {
  if (deviceOrientation_ == orientation) {
    return;
  }

  deviceOrientation_ = orientation;
  display_.setRotation(deviceOrientation_ == DeviceOrientation::kTopEdgeDown ? 2 : 0);
  forceMainPageFullRefresh_ = true;
  markAllMainPageRegionsDirty();
}

void DisplayModule::renderMainPage() {
  if (shouldRenderMainPageFullRefresh()) {
    renderMainPageFullRefresh();
    forceMainPageFullRefresh_ = false;
    resetMainPageRegionStateAfterFullRefresh();
    hasRenderedMainPage_ = true;
    hibernate();
    return;
  }

  if (!mainPageTopState_.dirty) {
    return;
  }

  renderMainPagePartialRefresh(MainPageRegion::kTop);
  updateMainPageRegionStateAfterRefresh(MainPageRegion::kTop, false);
  hasRenderedMainPage_ = true;
  hibernate();
}

DisplayModule::MainPageRegionBounds DisplayModule::getMainPageTopBounds() const {
  return MainPageRegionBounds{0, 0, static_cast<int16_t>(display_.width()),
                              kMainPageTopHeight};
}

DisplayModule::MainPageRegionBounds DisplayModule::getMainPageSidebarBounds() const {
  return MainPageRegionBounds{0, kMainPageTopHeight, kMainPageSidebarWidth,
                              static_cast<int16_t>(display_.height() - kMainPageTopHeight)};
}

DisplayModule::MainPageRegionBounds DisplayModule::getMainPageSidebarUpperBounds() const {
  return MainPageRegionBounds{0, kMainPageTopHeight, kMainPageSidebarWidth,
                              kMainPageSidebarTopSectionHeight};
}

DisplayModule::MainPageRegionBounds DisplayModule::getMainPageSidebarOutdoorBounds() const {
  return MainPageRegionBounds{0, kMainPageTopHeight, kMainPageSidebarPanelWidth,
                              kMainPageSidebarTopSectionHeight};
}

DisplayModule::MainPageRegionBounds DisplayModule::getMainPageSidebarIndoorBounds() const {
  return MainPageRegionBounds{kMainPageSidebarPanelWidth, kMainPageTopHeight,
                              kMainPageSidebarPanelWidth, kMainPageSidebarTopSectionHeight};
}

DisplayModule::MainPageRegionBounds DisplayModule::getMainPageSidebarForecastBounds() const {
  return MainPageRegionBounds{0, static_cast<int16_t>(kMainPageTopHeight + kMainPageSidebarTopSectionHeight),
                              kMainPageSidebarWidth,
                              static_cast<int16_t>(display_.height() - kMainPageTopHeight -
                                                   kMainPageSidebarTopSectionHeight)};
}

DisplayModule::MainPageRegionBounds DisplayModule::getMainPageContentBounds() const {
  return MainPageRegionBounds{kMainPageSidebarWidth, kMainPageTopHeight,
                              static_cast<int16_t>(display_.width() - kMainPageSidebarWidth),
                              static_cast<int16_t>(display_.height() - kMainPageTopHeight)};
}

DisplayModule::MainPageRegionBounds DisplayModule::getMainPageRegionBounds(
    MainPageRegion region) const {
  switch (region) {
    case MainPageRegion::kTop:
      return getMainPageTopBounds();
    case MainPageRegion::kSidebarOutdoor:
      return getMainPageSidebarOutdoorBounds();
    case MainPageRegion::kSidebarIndoor:
      return getMainPageSidebarIndoorBounds();
    case MainPageRegion::kSidebarForecast:
      return getMainPageSidebarForecastBounds();
    case MainPageRegion::kContent:
      return getMainPageContentBounds();
  }

  return getMainPageContentBounds();
}

DisplayModule::MainPageRegionState& DisplayModule::getMainPageRegionState(
    MainPageRegion region) {
  switch (region) {
    case MainPageRegion::kTop:
      return mainPageTopState_;
    case MainPageRegion::kSidebarOutdoor:
      return mainPageSidebarOutdoorState_;
    case MainPageRegion::kSidebarIndoor:
      return mainPageSidebarIndoorState_;
    case MainPageRegion::kSidebarForecast:
      return mainPageSidebarForecastState_;
    case MainPageRegion::kContent:
      return mainPageContentState_;
  }

  return mainPageContentState_;
}

const DisplayModule::MainPageRegionState& DisplayModule::getMainPageRegionState(
    MainPageRegion region) const {
  switch (region) {
    case MainPageRegion::kTop:
      return mainPageTopState_;
    case MainPageRegion::kSidebarOutdoor:
      return mainPageSidebarOutdoorState_;
    case MainPageRegion::kSidebarIndoor:
      return mainPageSidebarIndoorState_;
    case MainPageRegion::kSidebarForecast:
      return mainPageSidebarForecastState_;
    case MainPageRegion::kContent:
      return mainPageContentState_;
  }

  return mainPageContentState_;
}

bool DisplayModule::shouldRenderMainPageFullRefresh() const {
  if (forceMainPageFullRefresh_) {
    return true;
  }

  if (!hasRenderedMainPage_) {
    return true;
  }

  if (hasDirtyMainPageNonTopRegion()) {
    return true;
  }

  const MainPageRegionState& topState = getMainPageRegionState(MainPageRegion::kTop);
  if (!topState.dirty) {
    return false;
  }

  return topState.consecutivePartialRefreshes >= kMainPageMaxConsecutivePartialRefreshes ||
         topState.containsRed || topState.willDrawRed;
}

void DisplayModule::renderMainPageFullRefresh() {
  display_.setFullWindow();
  display_.firstPage();
  do {
    renderMainPageRegion(MainPageRegion::kTop);
    renderMainPageRegion(MainPageRegion::kSidebarOutdoor);
    renderMainPageRegion(MainPageRegion::kSidebarIndoor);
    renderMainPageRegion(MainPageRegion::kSidebarForecast);
    renderMainPageRegion(MainPageRegion::kContent);
    renderMainPageLayout();
  } while (display_.nextPage());
}

void DisplayModule::renderMainPagePartialRefresh(MainPageRegion region) {
  const MainPageRegionBounds bounds = getMainPageRegionBounds(region);
  // ================= 第一步：主动擦除旧区域 =================
  display_.setPartialWindow(bounds.x, bounds.y, bounds.width, bounds.height);
  display_.firstPage();
  do {
      // 填充纯白背景，覆盖旧内容
      display_.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, GxEPD_WHITE);
  } while (display_.nextPageBW()); // 使用 nextPageBW 发送白屏差异
  // ================= 第二步：绘制新时间 =================
  display_.setPartialWindow(bounds.x, bounds.y, bounds.width, bounds.height);
  display_.firstPage();
  do {
    renderMainPageRegion(region);
    renderMainPageLayout();
  } while (display_.nextPageBW());
}

bool DisplayModule::hasDirtyMainPageNonTopRegion() const {
  const MainPageRegion regions[] = {
      MainPageRegion::kSidebarOutdoor,
      MainPageRegion::kSidebarIndoor,
      MainPageRegion::kSidebarForecast,
      MainPageRegion::kContent,
  };

  for (MainPageRegion region : regions) {
    if (getMainPageRegionState(region).dirty) {
      return true;
    }
  }

  return false;
}

bool DisplayModule::shouldRefreshMainPageContent(const String& durationText,
                                                 const String& timestampText,
                                                 bool showTimestamp) const {
  const bool timestampChanged = mainPageContentTimestamp_ != timestampText;
  const bool visibilityChanged = mainPageContentTimestampVisible_ != showTimestamp;
  if (timestampChanged || visibilityChanged) {
    return true;
  }

  if (mainPageContentDuration_ == durationText) {
    return false;
  }

  if (!showTimestamp || timestampText.isEmpty()) {
    return false;
  }

  return isElapsedDurationRefreshBoundary(durationText);
}

void DisplayModule::renderMainPageLayout() {
  const MainPageRegionBounds topBounds = getMainPageTopBounds();
  const MainPageRegionBounds sidebarBounds = getMainPageSidebarBounds();

  display_.fillRect(topBounds.x,
                    topBounds.y + topBounds.height - kMainPageBorderThickness,
                    topBounds.width, kMainPageBorderThickness, GxEPD_BLACK);
  display_.fillRect(sidebarBounds.x + sidebarBounds.width - kMainPageBorderThickness,
                    sidebarBounds.y, kMainPageBorderThickness, sidebarBounds.height,
                    GxEPD_BLACK);
}

void DisplayModule::renderMainPageRegion(MainPageRegion region) {
  const MainPageRegionBounds bounds = getMainPageRegionBounds(region);

  switch (region) {
    case MainPageRegion::kTop:
      renderMainPageTopRegion(bounds);
      return;
    case MainPageRegion::kSidebarOutdoor:
      renderMainPageSidebarOutdoorRegion(bounds);
      return;
    case MainPageRegion::kSidebarIndoor:
      renderMainPageSidebarIndoorRegion(bounds);
      return;
    case MainPageRegion::kSidebarForecast:
      renderMainPageSidebarForecastRegion(bounds);
      return;
    case MainPageRegion::kContent:
      renderMainPageContentRegion(bounds);
      return;
  }
}

void DisplayModule::setMainPageBatteryStatus(uint8_t batteryPercentage, bool isCharging,
                                             bool isPowerConnected) {
  if (mainPageBatteryPercentage_ == batteryPercentage && mainPageCharging_ == isCharging &&
      mainPagePowerConnected_ == isPowerConnected) {
    return;
  }

  mainPageBatteryPercentage_ = batteryPercentage;
  mainPageCharging_ = isCharging;
  mainPagePowerConnected_ = isPowerConnected;
  markMainPageRegionDirty(MainPageRegion::kTop);
}

void DisplayModule::setMainPageTopTime(const String& topTimeText) {
  if (mainPageTopTime_ == topTimeText) {
    return;
  }

  mainPageTopTime_ = topTimeText;
  markMainPageRegionDirty(MainPageRegion::kTop);
}

void DisplayModule::setMainPageSidebarWeatherData(const WeatherData::SidebarWeatherData& data) {
  const bool outdoorChanged = !isOutdoorEnvironmentDataEqual(sidebarWeatherData_.outdoor, data.outdoor);
  const bool indoorChanged = !isIndoorEnvironmentDataEqual(sidebarWeatherData_.indoor, data.indoor);

  bool forecastChanged = false;
  for (size_t index = 0; index < WeatherData::kForecastDayCount; ++index) {
    if (!isDailyForecastDataEqual(sidebarWeatherData_.forecast[index], data.forecast[index])) {
      forecastChanged = true;
      break;
    }
  }

  sidebarWeatherData_ = data;

  if (mainPageContentTimestampVisible_) {
    return;
  }

  if (outdoorChanged) {
    markMainPageRegionDirty(MainPageRegion::kSidebarOutdoor);
  }

  if (indoorChanged) {
    markMainPageRegionDirty(MainPageRegion::kSidebarIndoor);
  }

  if (forecastChanged) {
    markMainPageRegionDirty(MainPageRegion::kSidebarForecast);
  }
}

void DisplayModule::setMainPageOutdoorEnvironmentData(
    const WeatherData::OutdoorEnvironmentData& data) {
  if (isOutdoorEnvironmentDataEqual(sidebarWeatherData_.outdoor, data)) {
    return;
  }

  sidebarWeatherData_.outdoor = data;

  if (mainPageContentTimestampVisible_) {
    return;
  }

  markMainPageRegionDirty(MainPageRegion::kSidebarOutdoor);
}

void DisplayModule::setMainPageIndoorEnvironmentData(
    const WeatherData::IndoorEnvironmentData& data) {
  if (isIndoorEnvironmentDataEqual(sidebarWeatherData_.indoor, data)) {
    return;
  }

  sidebarWeatherData_.indoor = data;

  if (mainPageContentTimestampVisible_) {
    return;
  }

  markMainPageRegionDirty(MainPageRegion::kSidebarIndoor);
}

void DisplayModule::setMainPageForecastData(const WeatherData::DailyForecastData* forecastDays,
                                            size_t forecastDayCount) {
  bool forecastChanged = false;

  for (size_t index = 0; index < WeatherData::kForecastDayCount; ++index) {
    WeatherData::DailyForecastData nextForecast;
    if (forecastDays != nullptr && index < forecastDayCount) {
      nextForecast = forecastDays[index];
    }

    if (!isDailyForecastDataEqual(sidebarWeatherData_.forecast[index], nextForecast)) {
      sidebarWeatherData_.forecast[index] = nextForecast;
      forecastChanged = true;
    }
  }

  if (mainPageContentTimestampVisible_) {
    return;
  }

  if (forecastChanged) {
    markMainPageRegionDirty(MainPageRegion::kSidebarForecast);
  }
}

void DisplayModule::setMainPageContentData(const String& durationText, const String& timestampText,
                                           bool showTimestamp) {
  if (mainPageContentDuration_ == durationText && mainPageContentTimestamp_ == timestampText &&
      mainPageContentTimestampVisible_ == showTimestamp) {
    return;
  }

  const bool shouldRefresh =
      shouldRefreshMainPageContent(durationText, timestampText, showTimestamp);

  mainPageContentDuration_ = durationText;
  mainPageContentTimestamp_ = timestampText;
  mainPageContentTimestampVisible_ = showTimestamp;

  if (shouldRefresh) {
    markMainPageRegionDirty(MainPageRegion::kContent);
  }
}

void DisplayModule::renderBatteryStatusIcon(int16_t x, int16_t y, uint8_t batteryPercentage) {
  const uint8_t segmentCount = resolveBatterySegmentCount(batteryPercentage);
  const int16_t headHeight = 10;
  const int16_t headY = y + ((kBatteryStatusIconHeight - headHeight) / 2);
  const int16_t segmentAreaX = x + 1 + kBatteryStatusInnerPadding;
  const int16_t segmentAreaY = y + 1 + kBatteryStatusInnerPadding;
  const int16_t segmentAreaWidth = kBatteryStatusBodyWidth - 2 - (kBatteryStatusInnerPadding * 2);
  const int16_t segmentAreaHeight = kBatteryStatusIconHeight - 2 - (kBatteryStatusInnerPadding * 2);
  const int16_t segmentWidth =
      (segmentAreaWidth - (kBatteryStatusSegmentGap * 2)) / 3;

  display_.drawRoundRect(x, y, kBatteryStatusBodyWidth, kBatteryStatusIconHeight, 5,
                         GxEPD_BLACK);
  display_.fillRoundRect(x + kBatteryStatusBodyWidth, headY, kBatteryStatusHeadWidth, headHeight,
                         1, GxEPD_BLACK);

  for (uint8_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
    const int16_t segmentX =
        segmentAreaX + (segmentIndex * (segmentWidth + kBatteryStatusSegmentGap));
    display_.fillRect(segmentX, segmentAreaY, segmentWidth, segmentAreaHeight, GxEPD_BLACK);
  }
}

void DisplayModule::renderMainPageTopRegion(const MainPageRegionBounds& bounds) {
  display_.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, GxEPD_WHITE);

  const int16_t timeAreaWidth =
      bounds.width < kMainPageTopTimeAreaWidth ? bounds.width : kMainPageTopTimeAreaWidth;
  const int16_t timeAreaHeight =
      bounds.height > kMainPageBorderThickness ? bounds.height - kMainPageBorderThickness : 0;
  const int16_t timeCenterX = bounds.x + (timeAreaWidth / 2);
  const int16_t timeCenterY = bounds.y + (timeAreaHeight / 2);
  const int16_t batteryIconX = bounds.x + bounds.width - kBatteryStatusIconWidth -
                               kBatteryStatusRightMargin;
  const int16_t batteryIconY = bounds.y + kBatteryStatusTopMargin;
  const int16_t auxIconY =
      batteryIconY + ((kBatteryStatusIconHeight - kBatteryStatusAuxIconSize) / 2);
  int16_t nextAuxIconX = batteryIconX - kBatteryStatusAuxIconGap - kBatteryStatusAuxIconSize;

  auto bitmapFonts = createFontCN96Renderer(display_);
  drawCenterBitmapText(bitmapFonts, mainPageTopTime_, timeCenterX, timeCenterY);

  if (mainPageCharging_) {
    display_.drawBitmap(nextAuxIconX, auxIconY, ChargingImage24x24::kBitmap,
                        ChargingImage24x24::kWidth, ChargingImage24x24::kHeight,
                        GxEPD_BLACK);
    nextAuxIconX -= kBatteryStatusAuxIconGap + kBatteryStatusAuxIconSize;
  }

  if (mainPagePowerConnected_) {
    display_.drawBitmap(nextAuxIconX, auxIconY, PowerImage24x24::kBitmap,
                        PowerImage24x24::kWidth, PowerImage24x24::kHeight, GxEPD_BLACK);
  }

  renderBatteryStatusIcon(batteryIconX, batteryIconY, mainPageBatteryPercentage_);
}

void DisplayModule::renderMainPageSidebarRegion(const MainPageRegionBounds& bounds) {
  display_.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, GxEPD_WHITE);

  renderMainPageSidebarOutdoorRegion(getMainPageSidebarOutdoorBounds());
  renderMainPageSidebarIndoorRegion(getMainPageSidebarIndoorBounds());
  renderMainPageSidebarForecastRegion(getMainPageSidebarForecastBounds());
}

void DisplayModule::renderMainPageSidebarOutdoorRegion(const MainPageRegionBounds& bounds) {
  display_.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, GxEPD_WHITE);

  auto labelFonts = createFontCN12Renderer(display_);
  auto textFonts = createFontCN16Renderer(display_);
  auto valueFonts = createFontCN24Renderer(display_);

  drawLeftBitmapText(labelFonts, String("室外"), bounds.x + 5, bounds.y + 15);
  drawWeatherIcon(display_, bounds.x + 5, bounds.y + 35, sidebarWeatherData_.outdoor.icon);
  drawLeftBitmapText(textFonts, sidebarWeatherData_.outdoor.text, bounds.x + 74, bounds.y + 45);
  drawLeftBitmapText(valueFonts, formatTemperatureValue(sidebarWeatherData_.outdoor.temp),
                     bounds.x + 74, bounds.y + 75);

  const int16_t dividerX = bounds.x + bounds.width - 1;
  const int16_t dividerStartY = bounds.y + kMainPageSidebarPadding;
  const int16_t dividerEndY = bounds.y + bounds.height - kMainPageSidebarPadding - 1;
  drawVerticalDashedLine(display_, dividerX, dividerStartY, dividerEndY);

  const int16_t dividerY = bounds.y + bounds.height - 1;
  drawHorizontalDashedLine(display_, dividerY, bounds.x + kMainPageSidebarPadding,
                           bounds.x + bounds.width - 1);
}

void DisplayModule::renderMainPageSidebarIndoorRegion(const MainPageRegionBounds& bounds) {
  display_.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, GxEPD_WHITE);

  auto labelFonts = createFontCN12Renderer(display_);
  auto valueFonts = createFontCN24Renderer(display_);

  drawLeftBitmapText(labelFonts, String("室内"), bounds.x + 5, bounds.y + 15);
  display_.drawBitmap(bounds.x + 5, bounds.y + 38, TemperatureImage32x32::kBitmap,
                      TemperatureImage32x32::kWidth, TemperatureImage32x32::kHeight,
                      GxEPD_BLACK);
  display_.drawBitmap(bounds.x + 5, bounds.y + 75, HumidityImage32x32::kBitmap,
                      HumidityImage32x32::kWidth, HumidityImage32x32::kHeight, GxEPD_BLACK);
  drawLeftBitmapText(valueFonts, formatTemperatureValue(sidebarWeatherData_.indoor.temp),
                     bounds.x + 42, bounds.y + 54);
  drawLeftBitmapText(valueFonts, formatHumidityValue(sidebarWeatherData_.indoor.humidity),
                     bounds.x + 42, bounds.y + 91);

  const int16_t dividerY = bounds.y + bounds.height - 1;
  drawHorizontalDashedLine(display_, dividerY, bounds.x,
                           bounds.x + bounds.width - kMainPageSidebarPadding - 1);
}

void DisplayModule::renderMainPageSidebarForecastRegion(const MainPageRegionBounds& bounds) {
  display_.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, GxEPD_WHITE);

  const MainPageRegionBounds gridBounds[WeatherData::kForecastDayCount] = {
      MainPageRegionBounds{bounds.x, bounds.y, kMainPageSidebarPanelWidth,
                           kMainPageSidebarTopSectionHeight},
      MainPageRegionBounds{static_cast<int16_t>(bounds.x + kMainPageSidebarPanelWidth), bounds.y,
                           kMainPageSidebarPanelWidth, kMainPageSidebarTopSectionHeight},
      MainPageRegionBounds{bounds.x,
                           static_cast<int16_t>(bounds.y + kMainPageSidebarTopSectionHeight),
                           kMainPageSidebarPanelWidth, kMainPageSidebarTopSectionHeight},
      MainPageRegionBounds{static_cast<int16_t>(bounds.x + kMainPageSidebarPanelWidth),
                           static_cast<int16_t>(bounds.y + kMainPageSidebarTopSectionHeight),
                           kMainPageSidebarPanelWidth, kMainPageSidebarTopSectionHeight},
  };

  auto labelFonts = createFontCN12Renderer(display_);
  for (size_t index = 0; index < WeatherData::kForecastDayCount; ++index) {
    const MainPageRegionBounds& grid = gridBounds[index];
    const WeatherData::DailyForecastData& forecast = sidebarWeatherData_.forecast[index];

    if (isForecastDataBlank(forecast)) {
      continue;
    }

    drawLeftBitmapText(labelFonts, formatForecastTitle(index, forecast), grid.x + 5, grid.y + 15);
    drawWeatherIcon(display_, grid.x + 5, grid.y + 35, forecast.iconDay);
    drawLeftBitmapText(labelFonts, forecast.textDay, grid.x + 74, grid.y + 51);
    drawLeftBitmapText(labelFonts, formatForecastTemperatureRange(forecast), grid.x + 74,
                       grid.y + 71);
  }

  const int16_t splitX = bounds.x + kMainPageSidebarPanelWidth - 1;
  const int16_t splitY = bounds.y + kMainPageSidebarTopSectionHeight - 1;
  const int16_t boundsRight = bounds.x + bounds.width - 1;
  const int16_t boundsBottom = bounds.y + bounds.height - 1;

  drawVerticalDashedLine(display_, splitX, bounds.y + kMainPageSidebarPadding,
                         splitY - kMainPageSidebarPadding);
  drawVerticalDashedLine(display_, splitX, splitY + kMainPageSidebarPadding,
                         boundsBottom - kMainPageSidebarPadding);
  drawHorizontalDashedLine(display_, splitY, bounds.x + kMainPageSidebarPadding,
                           splitX - kMainPageSidebarPadding);
  drawHorizontalDashedLine(display_, splitY, splitX + kMainPageSidebarPadding,
                           boundsRight - kMainPageSidebarPadding);
}

void DisplayModule::renderMainPageContentRegion(const MainPageRegionBounds& bounds) {
  display_.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, GxEPD_WHITE);

  auto headerFonts = createFontCN16Renderer(display_);
  auto durationFonts = createFontCN96Renderer(display_);
  auto footerFonts = createFontCN16Renderer(display_);

  const int16_t centerX = bounds.x + (bounds.width / 2);
  const int16_t centerY = bounds.y + (bounds.height / 2);
  const int16_t headerHeight =
      headerFonts.getFontAscent() - headerFonts.getFontDescent();
  const int16_t footerHeight =
      footerFonts.getFontAscent() - footerFonts.getFontDescent();
  const int16_t headerCenterY =
      bounds.y + kMainPageContentTextEdgeOffset + (headerHeight / 2);
  const int16_t footerCenterY = bounds.y + bounds.height - kMainPageContentTextEdgeOffset -
                                (footerHeight / 2);

  drawCenterBitmapText(headerFonts, String(kMainPageContentTitleText), centerX, headerCenterY);
  drawCenterBitmapText(durationFonts, mainPageContentDuration_, centerX, centerY);
  if (mainPageContentTimestampVisible_ && !mainPageContentTimestamp_.isEmpty()) {
    drawCenterBitmapText(footerFonts, mainPageContentTimestamp_, centerX, footerCenterY);
  }
}

void DisplayModule::markAllMainPageRegionsDirty() {
  const MainPageRegion regions[] = {
      MainPageRegion::kTop,
      MainPageRegion::kSidebarOutdoor,
      MainPageRegion::kSidebarIndoor,
      MainPageRegion::kSidebarForecast,
      MainPageRegion::kContent,
  };

  for (MainPageRegion region : regions) {
    markMainPageRegionDirty(region);
  }
}

void DisplayModule::updateMainPageRegionStateAfterRefresh(MainPageRegion region,
                                                          bool fullRefresh) {
  MainPageRegionState& state = getMainPageRegionState(region);

  if (fullRefresh) {
    state.consecutivePartialRefreshes = 0;
  } else if (state.consecutivePartialRefreshes < 0xFF) {
    ++state.consecutivePartialRefreshes;
  }

  state.containsRed = state.willDrawRed;
  state.willDrawRed = false;
  state.dirty = false;
}

void DisplayModule::markMainPageRegionDirty(MainPageRegion region, bool willDrawRed) {
  MainPageRegionState& state = getMainPageRegionState(region);
  state.dirty = true;
  state.willDrawRed = state.willDrawRed || willDrawRed;
}

void DisplayModule::resetMainPageRegionStateAfterFullRefresh() {
  const MainPageRegion regions[] = {
      MainPageRegion::kTop,
      MainPageRegion::kSidebarOutdoor,
      MainPageRegion::kSidebarIndoor,
      MainPageRegion::kSidebarForecast,
      MainPageRegion::kContent,
  };

  for (MainPageRegion region : regions) {
    updateMainPageRegionStateAfterRefresh(region, true);
  }
}

void DisplayModule::renderLowBattery() {
  const int16_t imageX =
      (display_.width() - static_cast<int16_t>(BatteryImage128x128::kWidth)) / 2;
  const int16_t imageY =
      (display_.height() - static_cast<int16_t>(BatteryImage128x128::kHeight)) / 2;

  display_.setFullWindow();
  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    display_.drawBitmap(imageX, imageY, BatteryImage128x128::kBitmap,
                        BatteryImage128x128::kWidth, BatteryImage128x128::kHeight,
                        GxEPD_BLACK);
  } while (display_.nextPage());

  hibernate();
}

void DisplayModule::renderLogo() {
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;
  const int16_t outerRadius = 56;
  const int16_t innerRadius = 50;
  const char* logoText = "ColaFeed";
  const String logoTextStr(logoText);
  const int16_t logoGraphicCenterY = centerY;
  const int16_t logoTextCenterY = centerY + 110;
  const int16_t imageX = centerX - static_cast<int16_t>(LogoImage64x64::kWidth / 2);
  const int16_t imageY = logoGraphicCenterY - static_cast<int16_t>(LogoImage64x64::kHeight / 2);
  auto bitmapFonts = createFontCN32Renderer(display_);

  display_.setFullWindow();
  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    display_.fillCircle(centerX, logoGraphicCenterY, outerRadius, GxEPD_RED);
    display_.fillCircle(centerX, logoGraphicCenterY, innerRadius, GxEPD_WHITE);
    display_.drawBitmap(imageX, imageY, LogoImage64x64::kBitmap, LogoImage64x64::kWidth,
                        LogoImage64x64::kHeight, GxEPD_RED);

    drawCenterBitmapText(bitmapFonts, logoTextStr, centerX, logoTextCenterY);
  } while (display_.nextPage());

  hibernate();
}

void DisplayModule::hibernate() {
  display_.hibernate();
}
