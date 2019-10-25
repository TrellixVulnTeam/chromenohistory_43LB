// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_PLATFORM_TOAST_H_
#define CHROME_BROWSER_VR_MODEL_PLATFORM_TOAST_H_

#include "base/strings/string16.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

// Represent a request to show a text only Toast.
struct VR_UI_EXPORT PlatformToast {
  PlatformToast();
  explicit PlatformToast(base::string16 text);

  base::string16 text;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_PLATFORM_TOAST_H_
