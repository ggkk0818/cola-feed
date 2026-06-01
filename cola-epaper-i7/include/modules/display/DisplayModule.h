#pragma once

#include <Arduino.h>

#include "modules/display/DisplayDriver.h"

class DisplayModule {
 public:
  DisplayModule();

  void begin();
  void renderMainPage();
  void renderLowBattery();
  void renderLogo();
  void renderFontCN16Test();
  void renderFontCN32Test();
  void hibernate();

 private:
  enum class MainPageRegion : uint8_t {
    kTop,
    kSidebar,
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
  MainPageRegionBounds getMainPageContentBounds() const;
  MainPageRegionBounds getMainPageRegionBounds(MainPageRegion region) const;

  MainPageRegionState& getMainPageRegionState(MainPageRegion region);
  const MainPageRegionState& getMainPageRegionState(MainPageRegion region) const;

  bool shouldRenderMainPageFullRefresh() const;
  bool shouldRenderMainPageRegion(MainPageRegion region) const;

  void renderMainPageFullRefresh();
  void renderMainPagePartialRefresh(MainPageRegion region);
  void renderMainPageLayout();
  void renderMainPageRegion(MainPageRegion region);
  void renderMainPageTopRegion(const MainPageRegionBounds& bounds);
  void renderMainPageSidebarRegion(const MainPageRegionBounds& bounds);
  void renderMainPageContentRegion(const MainPageRegionBounds& bounds);
  void updateMainPageRegionStateAfterRefresh(MainPageRegion region, bool fullRefresh);
  void resetMainPageRegionStateAfterFullRefresh();

  DisplayDriver display_;
  MainPageRegionState mainPageTopState_{0, true, false, false};
  MainPageRegionState mainPageSidebarState_{0, true, false, false};
  MainPageRegionState mainPageContentState_{0, true, false, false};
  bool hasRenderedMainPage_ = false;
};
