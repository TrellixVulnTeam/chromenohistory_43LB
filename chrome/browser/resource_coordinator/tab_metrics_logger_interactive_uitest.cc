// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_metrics_logger.h"

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_metrics_event.pb.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/window_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using metrics::WindowMetricsEvent;
using resource_coordinator::TabLifecycleUnitExternal;
using tab_ranker::WindowFeatures;

const char* kTestUrl = "https://example.com/";
constexpr char kBeforeUnloadHtml[] =
    "data:text/html,<html><body><script>window.onbeforeunload=function(e) {}"
    "</script></body></html>";

// Tests WindowFeatures generated by TabMetricsLogger::CreateWindowFeatures due
// to interactive changes to window state.
class TabMetricsLoggerTest : public InProcessBrowserTest {
 protected:
  TabMetricsLoggerTest() = default;

  // InProcessBrowserTest:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  void SetUpOnMainThread() override {
#if defined(OS_MACOSX)
    // On Mac, the browser window needs to be forced to the front. This will
    // create a UKM entry for the activation because it happens after the
    // WindowActivityWatcher creation. On other platforms, activation happens
    // before creation, and as a result, no UKM entry is created.
    // TODO(crbug.com/650859): Reassess after activation is restored in the
    // focus manager.
    ui_test_utils::BrowserActivationWaiter waiter(browser());
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    waiter.WaitForActivation();
    ASSERT_TRUE(browser()->window()->IsActive());
#endif
  }

  // Returns TabFeatures of Tab at |index|.
  tab_ranker::TabFeatures CurrentTabFeatures(const int index) {
    auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(index);
    return TabMetricsLogger::GetTabFeatures(TabMetricsLogger::PageMetrics(),
                                            web_contents)
        .value();
  }

  void DiscardTabAt(const int index) {
    auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(index);
    auto* external = TabLifecycleUnitExternal::FromWebContents(web_contents);
    external->DiscardTab();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabMetricsLoggerTest);
};

// Tests before unload handler is calculated correctly.
IN_PROC_BROWSER_TEST_F(TabMetricsLoggerTest, GetBeforeUnloadHandler) {
  EXPECT_FALSE(CurrentTabFeatures(0).has_before_unload_handler);
  ui_test_utils::NavigateToURL(browser(), GURL(kBeforeUnloadHtml));
  EXPECT_TRUE(CurrentTabFeatures(0).has_before_unload_handler);
}

// Tests WindowMetrics of the current browser window.
IN_PROC_BROWSER_TEST_F(TabMetricsLoggerTest, CreateWindowFeaturesTest) {
  WindowFeatures expected_metrics{WindowMetricsEvent::TYPE_TABBED,
                                  WindowMetricsEvent::SHOW_STATE_NORMAL, true,
                                  1};
  EXPECT_EQ(TabMetricsLogger::CreateWindowFeatures(browser()),
            expected_metrics);

  // Updated metrics after adding tabs.
  {
    SCOPED_TRACE("");
    AddTabAtIndex(1, GURL(kTestUrl), ui::PAGE_TRANSITION_LINK);
    expected_metrics.tab_count++;
    EXPECT_EQ(TabMetricsLogger::CreateWindowFeatures(browser()),
              expected_metrics);
  }
  {
    SCOPED_TRACE("");
    AddTabAtIndex(0, GURL(kTestUrl), ui::PAGE_TRANSITION_LINK);
    expected_metrics.tab_count++;
    EXPECT_EQ(TabMetricsLogger::CreateWindowFeatures(browser()),
              expected_metrics);
  }

  // Closing the window doesn't log more WindowMetrics UKMs.
  CloseBrowserSynchronously(browser());
}

// Tests GetDiscardCount.
IN_PROC_BROWSER_TEST_F(TabMetricsLoggerTest, GetDiscardCount) {
  // We need at least two tabs because "transition from DISCARDED to DISCARDED
  // is not allowed".
  AddTabAtIndex(1, GURL(kTestUrl), ui::PAGE_TRANSITION_LINK);
  EXPECT_EQ(CurrentTabFeatures(0).discard_count, 0);
  DiscardTabAt(0);
  EXPECT_EQ(CurrentTabFeatures(0).discard_count, 1);
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(CurrentTabFeatures(0).discard_count, 1);

  DiscardTabAt(1);
  EXPECT_EQ(CurrentTabFeatures(1).discard_count, 1);
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(CurrentTabFeatures(1).discard_count, 1);

  DiscardTabAt(0);
  EXPECT_EQ(CurrentTabFeatures(0).discard_count, 2);

  CloseBrowserSynchronously(browser());
}

// TODO(https://crbug.com/51364): Implement BrowserWindow::Deactivate() on Mac.
#if !defined(OS_MACOSX)
// Tests WindowMetrics by activating/deactivating the window.
IN_PROC_BROWSER_TEST_F(TabMetricsLoggerTest,
                       CreateWindowFeaturesTestWindowActivation) {
  WindowFeatures expected_metrics{WindowMetricsEvent::TYPE_TABBED,
                                  WindowMetricsEvent::SHOW_STATE_NORMAL, false,
                                  1};

  {
    SCOPED_TRACE("");
    ui_test_utils::BrowserDeactivationWaiter waiter(browser());
    browser()->window()->Deactivate();
    waiter.WaitForDeactivation();
    EXPECT_EQ(TabMetricsLogger::CreateWindowFeatures(browser()),
              expected_metrics);
  }

  {
    SCOPED_TRACE("");
    ui_test_utils::BrowserActivationWaiter waiter(browser());
    browser()->window()->Activate();
    waiter.WaitForActivation();
    ASSERT_TRUE(browser()->window()->IsActive());
    expected_metrics.is_active = true;
    EXPECT_EQ(TabMetricsLogger::CreateWindowFeatures(browser()),
              expected_metrics);
  }

  // Closing the window doesn't log more WindowMetrics UKMs.
  CloseBrowserSynchronously(browser());
}

// Tests WindowMetrics when switching between windows.
IN_PROC_BROWSER_TEST_F(TabMetricsLoggerTest,
                       CreateWindowFeaturesTestMultipleWindows) {
  // Create a new browser window.
  Browser* browser_2 = CreateBrowser(browser()->profile());
  WindowFeatures expected_metrics{WindowMetricsEvent::TYPE_TABBED,
                                  WindowMetricsEvent::SHOW_STATE_NORMAL, false,
                                  1};
  WindowFeatures expected_metrics_2{WindowMetricsEvent::TYPE_TABBED,
                                    WindowMetricsEvent::SHOW_STATE_NORMAL,
                                    false, 1};

  // Wait for the old window to be deactivated and the new window to be
  // activated if they aren't yet.
  {
    ui_test_utils::BrowserActivationWaiter waiter(browser_2);
    waiter.WaitForActivation();
    expected_metrics_2.is_active = true;
  }
  {
    ui_test_utils::BrowserDeactivationWaiter waiter(browser());
    waiter.WaitForDeactivation();
  }
  {
    SCOPED_TRACE("");
    // Check for activation and deactivation.
    EXPECT_EQ(TabMetricsLogger::CreateWindowFeatures(browser_2),
              expected_metrics_2);
    EXPECT_EQ(TabMetricsLogger::CreateWindowFeatures(browser()),
              expected_metrics);
  }
  {
    SCOPED_TRACE("");
    ui_test_utils::BrowserActivationWaiter activation_waiter(browser());
    ui_test_utils::BrowserDeactivationWaiter deactivation_waiter(browser_2);

    // Switch back to the first window.
    // Note: use Activate() rather than Show() because on some platforms, Show()
    // calls SetLastActive() before doing anything else.
    browser()->window()->Activate();

    activation_waiter.WaitForActivation();
    deactivation_waiter.WaitForDeactivation();

    expected_metrics.is_active = true;
    expected_metrics_2.is_active = false;
    EXPECT_EQ(TabMetricsLogger::CreateWindowFeatures(browser()),
              expected_metrics);
    EXPECT_EQ(TabMetricsLogger::CreateWindowFeatures(browser_2),
              expected_metrics_2);
  }

  CloseBrowserSynchronously(browser());

  // GetLastActive could return browser2 if browser1 was active and then was
  // removed, so browser2 might not actually be active.
  {
    ui_test_utils::BrowserActivationWaiter activation_waiter(browser_2);
    browser_2->window()->Activate();
    activation_waiter.WaitForActivation();
  }
  EXPECT_TRUE(BrowserList::GetInstance()->GetLastActive() == browser_2);
  EXPECT_TRUE(browser_2->window()->IsActive());
  {
    SCOPED_TRACE("");
    expected_metrics_2.is_active = true;
    EXPECT_EQ(TabMetricsLogger::CreateWindowFeatures(browser_2),
              expected_metrics_2);
  }
}

#endif  // !defined(OS_MACOSX)
