// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/test/pixel/browser_skia_gold_pixel_diff.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"  // mash-ok
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/ui/test/test_browser_dialog_mac.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "base/strings/strcat.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_observer.h"
#endif

namespace {

#if defined(TOOLKIT_VIEWS)
// Helper to close a Widget.
class WidgetCloser {
 public:
  WidgetCloser(views::Widget* widget, bool async) : widget_(widget) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&WidgetCloser::CloseWidget,
                                  weak_ptr_factory_.GetWeakPtr(), async));
  }

 private:
  void CloseWidget(bool async) {
    if (async)
      widget_->Close();
    else
      widget_->CloseNow();
  }

  views::Widget* widget_;

  base::WeakPtrFactory<WidgetCloser> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WidgetCloser);
};

class CompositorObserverEnd : public ui::CompositorObserver {
 public:
  void OnCompositingEnded(ui::Compositor* compositor) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }
  void WaitForComplete() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

#endif  // defined(TOOLKIT_VIEWS)

}  // namespace

TestBrowserDialog::TestBrowserDialog() : TestBrowserUi() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "browser-ui-tests-verify-pixels")) {
    pixel_diff_ = std::make_unique<BrowserSkiaGoldPixelDiff>();
  }
}

TestBrowserDialog::~TestBrowserDialog() = default;

void TestBrowserDialog::PreShow() {
  UpdateWidgets();
}

void CompareAsync(const BrowserSkiaGoldPixelDiff* pixel_diff,
                  const std::string& name,
                  const views::View* view,
                  bool* result) {
  *result = pixel_diff->CompareScreenshot(name, view);
}

// This returns true if exactly one views widget was shown that is a dialog or
// has a name matching the test-specified name, and if that window is in the
// work area (if |should_verify_dialog_bounds_| is true).
bool TestBrowserDialog::VerifyUi() {
#if defined(TOOLKIT_VIEWS)
  views::Widget::Widgets widgets_before = widgets_;
  UpdateWidgets();

  // Get the list of added dialog widgets. Ignore non-dialog widgets, including
  // those added by tests to anchor dialogs and the browser's status bubble.
  // Non-dialog widgets matching the test-specified name will also be included.
  auto added =
      base::STLSetDifference<views::Widget::Widgets>(widgets_, widgets_before);
  std::string name = GetNonDialogName();
  base::EraseIf(added, [&](views::Widget* widget) {
    return !widget->widget_delegate()->AsDialogDelegate() &&
           (name.empty() || widget->GetName() != name);
  });
  widgets_ = added;

  if (added.size() != 1)
    return false;

  views::Widget* dialog_widget = *(added.begin());
// TODO(https://crbug.com/958242) support Mac for pixel tests.
#if !defined(OS_MACOSX)
  if (pixel_diff_) {
    // Wait for painting complete.
    auto* compositor = dialog_widget->GetNativeView()->layer()->GetCompositor();
    CompositorObserverEnd obend;
    compositor->AddObserver(&obend);
    obend.WaitForComplete();
    compositor->RemoveObserver(&obend);

    base::ScopedAllowBlockingForTesting allow_blocking;
    pixel_diff_->Init(dialog_widget, "BrowserUiDialog");
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string test_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});
    bool result = false;
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(CompareAsync, pixel_diff_.get(), test_name,
                       dialog_widget->GetContentsView(), &result),
        run_loop.QuitClosure());
    run_loop.Run();
    if (!result)
      return false;
  }
#endif  // OS_MACOSX

  if (!should_verify_dialog_bounds_)
    return true;

  // Verify that the dialog's dimensions do not exceed the display's work area
  // bounds, which may be smaller than its bounds(), e.g. in the case of the
  // docked magnifier or Chromevox being enabled.
  const gfx::Rect dialog_bounds = dialog_widget->GetWindowBoundsInScreen();
  gfx::NativeWindow native_window = dialog_widget->GetNativeWindow();
  DCHECK(native_window);
  display::Screen* screen = display::Screen::GetScreen();
  const gfx::Rect display_work_area =
      screen->GetDisplayNearestWindow(native_window).work_area();

  return display_work_area.Contains(dialog_bounds);
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

void TestBrowserDialog::WaitForUserDismissal() {
#if defined(OS_MACOSX)
  internal::TestBrowserDialogInteractiveSetUp();
#endif

#if defined(TOOLKIT_VIEWS)
  ASSERT_FALSE(widgets_.empty());
  views::test::WidgetDestroyedWaiter waiter(*widgets_.begin());
  waiter.Wait();
#else
  NOTIMPLEMENTED();
#endif
}

void TestBrowserDialog::DismissUi() {
#if defined(TOOLKIT_VIEWS)
  ASSERT_FALSE(widgets_.empty());
  views::test::WidgetDestroyedWaiter waiter(*widgets_.begin());
  WidgetCloser closer(*widgets_.begin(), AlwaysCloseAsynchronously());
  waiter.Wait();
#else
  NOTIMPLEMENTED();
#endif
}

bool TestBrowserDialog::AlwaysCloseAsynchronously() {
  // TODO(tapted): Iterate over close methods for greater test coverage.
  return false;
}

std::string TestBrowserDialog::GetNonDialogName() {
  return std::string();
}

void TestBrowserDialog::UpdateWidgets() {
  widgets_.clear();
#if defined(OS_CHROMEOS)
  for (aura::Window* root_window : ash::Shell::GetAllRootWindows())
    views::Widget::GetAllChildWidgets(root_window, &widgets_);
#elif defined(TOOLKIT_VIEWS)
  widgets_ = views::test::WidgetTest::GetAllWidgets();
#else
  NOTIMPLEMENTED();
#endif
}
