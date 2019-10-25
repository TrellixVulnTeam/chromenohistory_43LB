// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DISPLAY_ROTATION_DEFAULT_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DISPLAY_ROTATION_DEFAULT_HANDLER_H_


#include <set>
#include <string>
#include <vector>

#include "ash/public/mojom/cros_display_config.mojom.h"
#include "chrome/browser/chromeos/policy/display_settings_handler.h"
#include "ui/display/display.h"

namespace policy {

// Implements DisplayRotationDefault device policy.
//
// Whenever there is a change in the display configuration, any new display will
// be rotated according to the policy.
//
// Whenever there is a change in |kDisplayResolutionDefault| setting from
// CrosSettings, the new policy is applied to all displays.
//
// Once rotation for some display was set by this policy it won't be reapplied
// until next reboot or policy change (i.e. user can manually override the
// rotation for that display via settings page).
class DisplayRotationDefaultHandler : public DisplaySettingsPolicyHandler {
 public:
  DisplayRotationDefaultHandler();

  ~DisplayRotationDefaultHandler() override;

  // DisplaySettingsPolicyHandler
  const char* SettingName() override;
  void OnSettingUpdate() override;
  void ApplyChanges(
      ash::mojom::CrosDisplayConfigController* cros_display_config,
      const std::vector<ash::mojom::DisplayUnitInfoPtr>& info_list) override;

 private:
  bool policy_enabled_ = false;
  display::Display::Rotation display_rotation_default_ =
      display::Display::ROTATE_0;
  std::set<std::string> rotated_display_ids_;

  DISALLOW_COPY_AND_ASSIGN(DisplayRotationDefaultHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DISPLAY_ROTATION_DEFAULT_HANDLER_H_
