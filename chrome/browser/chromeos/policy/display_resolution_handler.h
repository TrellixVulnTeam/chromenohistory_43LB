// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DISPLAY_RESOLUTION_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DISPLAY_RESOLUTION_HANDLER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/mojom/cros_display_config.mojom.h"
#include "chrome/browser/chromeos/policy/display_settings_handler.h"

namespace policy {

// Implements DeviceDisplayResolution device policy.
//
// Whenever there is a change in the display configration, any new display will
// be resized according to the policy (only if the policy is enabled and the
// display supports specified resolution and scale factor).
//
// Whenever there is a change in |kDeviceDisplayResolution| setting from
// CrosSettings, the new policy is reapplied to all displays.
//
// If the specified resolution or scale factor is not supported by some display,
// the resolution won't change.
//
// Once resolution or scale factor for some display was set by this policy it
// won't be reapplied until next reboot or policy change (i.e. user can manually
// override the settings for that display via settings page).
class DisplayResolutionHandler : public DisplaySettingsPolicyHandler {
 public:
  DisplayResolutionHandler();

  ~DisplayResolutionHandler() override;

  // DisplaySettingsPolicyHandler
  const char* SettingName() override;
  void OnSettingUpdate() override;
  void ApplyChanges(
      ash::mojom::CrosDisplayConfigController* cros_display_config,
      const std::vector<ash::mojom::DisplayUnitInfoPtr>& info_list) override;

 private:
  struct InternalDisplaySettings;
  struct ExternalDisplaySettings;

  bool policy_enabled_ = false;
  bool recommended_ = false;
  std::unique_ptr<ExternalDisplaySettings> external_display_settings_;
  std::unique_ptr<InternalDisplaySettings> internal_display_settings_;
  std::set<std::string> resized_display_ids_;

  DISALLOW_COPY_AND_ASSIGN(DisplayResolutionHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DISPLAY_RESOLUTION_HANDLER_H_
