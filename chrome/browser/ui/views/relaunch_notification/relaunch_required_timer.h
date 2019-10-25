// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_REQUIRED_TIMER_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_REQUIRED_TIMER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/relaunch_notification/wall_clock_timer.h"

// Timer that handles notification title refresh for relaunch required
// notification. Created either by RelaunchRequiredDialogView for Chrome
// desktop or directly by the controller in the Chrome OS implementation.
// Title refresh is invoked with the |callback| provided at creation.
class RelaunchRequiredTimer {
 public:
  // |deadline| is the time at which Chrome will be forcefully relaunched by
  // the controller, here it is used to compose the notification's title
  // accordingly (e.g. "Chrome will restart in 3 minutes").
  // |callback| is called every time the notification title has to be updated.
  RelaunchRequiredTimer(base::Time deadline, base::RepeatingClosure callback);

  ~RelaunchRequiredTimer();

  // Sets the relaunch deadline to |deadline| and refreshes the notifications's
  // title accordingly.
  void SetDeadline(base::Time deadline);

  // Returns current notification's title, composed depending on how much time
  // is left until the deadline.
  base::string16 GetWindowTitle() const;

 private:
  // Schedules a timer to fire the next time the title must be updated.
  void ScheduleNextTitleRefresh();

  // Invoked when the timer fires to refresh the title text.
  void OnTitleRefresh();

  // The time at which Chrome will be forcefully relaunched.
  base::Time deadline_;

  // A timer with which title refreshes are scheduled.
  WallClockTimer refresh_timer_;

  // Callback which triggers the actual title update, which differs on Chrome
  // for desktop vs for Chrome OS.
  base::RepeatingClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(RelaunchRequiredTimer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_REQUIRED_TIMER_H_
