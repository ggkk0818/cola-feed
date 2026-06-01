#include "modules/display/DisplayModule.h"

#include <Adafruit_GFX.h>
#include <SPI.h>

#include "modules/display/BatteryImage128x128.h"
#include "modules/display/BitmapFontRenderer.h"
#include "modules/display/LogoImage64x64.h"

namespace {
constexpr uint8_t EPD_SCK_PIN = 14;
constexpr uint8_t EPD_MOSI_PIN = 13;
constexpr uint8_t EPD_CS_PIN = 4;
constexpr uint8_t EPD_RST_PIN = 23;
constexpr uint8_t EPD_DC_PIN = 5;
constexpr uint8_t EPD_BUSY_PIN = 24;
constexpr int16_t kMainPageTopHeight = 120;
constexpr int16_t kMainPageSidebarWidth = 300;
constexpr int16_t kMainPageBorderThickness = 2;
constexpr uint8_t kMainPageMaxConsecutivePartialRefreshes = 5;
}  // namespace

DisplayModule::DisplayModule()
    : display_(GxEPD2_750c_GDEY075Z08(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN)) {}

void DisplayModule::begin() {
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display_.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  display_.init(0);
  display_.setRotation(0);
  display_.setFullWindow();
}

void DisplayModule::renderMainPage() {
  if (shouldRenderMainPageFullRefresh()) {
    renderMainPageFullRefresh();
    resetMainPageRegionStateAfterFullRefresh();
    hasRenderedMainPage_ = true;
    hibernate();
    return;
  }

  const MainPageRegion regions[] = {
      MainPageRegion::kTop,
      MainPageRegion::kSidebar,
      MainPageRegion::kContent,
  };

  bool renderedAnyRegion = false;
  for (MainPageRegion region : regions) {
    if (!shouldRenderMainPageRegion(region)) {
      continue;
    }

    renderMainPagePartialRefresh(region);
    updateMainPageRegionStateAfterRefresh(region, false);
    renderedAnyRegion = true;
  }

  if (renderedAnyRegion) {
    hasRenderedMainPage_ = true;
    hibernate();
  }
}

DisplayModule::MainPageRegionBounds DisplayModule::getMainPageTopBounds() const {
  return MainPageRegionBounds{0, 0, static_cast<int16_t>(display_.width()),
                              kMainPageTopHeight};
}

DisplayModule::MainPageRegionBounds DisplayModule::getMainPageSidebarBounds() const {
  return MainPageRegionBounds{0, kMainPageTopHeight, kMainPageSidebarWidth,
                              static_cast<int16_t>(display_.height() - kMainPageTopHeight)};
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
    case MainPageRegion::kSidebar:
      return getMainPageSidebarBounds();
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
    case MainPageRegion::kSidebar:
      return mainPageSidebarState_;
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
    case MainPageRegion::kSidebar:
      return mainPageSidebarState_;
    case MainPageRegion::kContent:
      return mainPageContentState_;
  }

  return mainPageContentState_;
}

bool DisplayModule::shouldRenderMainPageFullRefresh() const {
  if (!hasRenderedMainPage_) {
    return true;
  }

  const MainPageRegion regions[] = {
      MainPageRegion::kTop,
      MainPageRegion::kSidebar,
      MainPageRegion::kContent,
  };

  for (MainPageRegion region : regions) {
    const MainPageRegionState& state = getMainPageRegionState(region);
    if (!state.dirty) {
      continue;
    }

    if (state.consecutivePartialRefreshes >= kMainPageMaxConsecutivePartialRefreshes ||
        state.containsRed || state.willDrawRed) {
      return true;
    }
  }

  return false;
}

bool DisplayModule::shouldRenderMainPageRegion(MainPageRegion region) const {
  return getMainPageRegionState(region).dirty;
}

void DisplayModule::renderMainPageFullRefresh() {
  display_.setFullWindow();
  display_.firstPage();
  do {
    renderMainPageRegion(MainPageRegion::kTop);
    renderMainPageRegion(MainPageRegion::kSidebar);
    renderMainPageRegion(MainPageRegion::kContent);
    renderMainPageLayout();
  } while (display_.nextPage());
}

void DisplayModule::renderMainPagePartialRefresh(MainPageRegion region) {
  const MainPageRegionBounds bounds = getMainPageRegionBounds(region);

  display_.setPartialWindow(bounds.x, bounds.y, bounds.width, bounds.height);
  display_.firstPage();
  do {
    renderMainPageRegion(region);
    renderMainPageLayout();
  } while (display_.nextPage());
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
    case MainPageRegion::kSidebar:
      renderMainPageSidebarRegion(bounds);
      return;
    case MainPageRegion::kContent:
      renderMainPageContentRegion(bounds);
      return;
  }
}

void DisplayModule::renderMainPageTopRegion(const MainPageRegionBounds& bounds) {
  display_.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, GxEPD_WHITE);

  // TODO: Render the top bar content.
}

void DisplayModule::renderMainPageSidebarRegion(const MainPageRegionBounds& bounds) {
  display_.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, GxEPD_WHITE);

  // TODO: Render the left sidebar content.
}

void DisplayModule::renderMainPageContentRegion(const MainPageRegionBounds& bounds) {
  display_.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, GxEPD_WHITE);

  // TODO: Render the main content area.
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

void DisplayModule::resetMainPageRegionStateAfterFullRefresh() {
  const MainPageRegion regions[] = {
      MainPageRegion::kTop,
      MainPageRegion::kSidebar,
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

void DisplayModule::renderFontCN16Test() {
  static constexpr char kTestText[] = "温！湿度23:15晴AB吃cd奶时!间";
  display_.setFullWindow();

  auto bitmapFonts = createFontCN16Renderer(display_);
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;

  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    drawCenterBitmapText(bitmapFonts, String(kTestText), centerX, centerY);
  } while (display_.nextPage());

  hibernate();
}

void DisplayModule::renderFontCN32Test() {
  static constexpr char kTestText[] = "温！湿度23:15晴AB吃cd奶时!间";
  display_.setFullWindow();

  auto bitmapFonts = createFontCN32Renderer(display_);
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;

  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    drawCenterBitmapText(bitmapFonts, String(kTestText), centerX, centerY);
  } while (display_.nextPage());

  hibernate();
}

void DisplayModule::hibernate() {
  display_.hibernate();
}
