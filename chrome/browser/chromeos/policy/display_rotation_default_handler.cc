// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/display_rotation_default_handler.h"
#include "base/bind_helpers.h"

#include <utility>

namespace policy {

DisplayRotationDefaultHandler::DisplayRotationDefaultHandler() = default;

DisplayRotationDefaultHandler::~DisplayRotationDefaultHandler() = default;

const char* DisplayRotationDefaultHandler::SettingName() {
  return chromeos::kDisplayRotationDefault;
}

// Reads |chromeos::kDisplayRotationDefault| from CrosSettings and stores
// its value, and whether it has a value, in member variables
// |display_rotation_default_| and |policy_enabled_|.
void DisplayRotationDefaultHandler::OnSettingUpdate() {
  int new_rotation;
  bool new_policy_enabled = chromeos::CrosSettings::Get()->GetInteger(
      chromeos::kDisplayRotationDefault, &new_rotation);
  display::Display::Rotation new_display_rotation_default =
      display::Display::ROTATE_0;
  if (new_policy_enabled) {
    if (new_rotation >= display::Display::ROTATE_0 &&
        new_rotation <= display::Display::ROTATE_270) {
      new_display_rotation_default =
          static_cast<display::Display::Rotation>(new_rotation);
    } else {
      LOG(ERROR) << "CrosSettings contains invalid value " << new_rotation
                 << " for DisplayRotationDefault. Ignoring setting.";
      new_policy_enabled = false;
    }
  }
  if (new_policy_enabled != policy_enabled_ ||
      (new_policy_enabled &&
       new_display_rotation_default != display_rotation_default_)) {
    policy_enabled_ = new_policy_enabled;
    display_rotation_default_ = new_display_rotation_default;
    rotated_display_ids_.clear();
  }
}

void DisplayRotationDefaultHandler::ApplyChanges(
    ash::mojom::CrosDisplayConfigController* cros_display_config,
    const std::vector<ash::mojom::DisplayUnitInfoPtr>& info_list) {
  if (!policy_enabled_)
    return;
  for (const ash::mojom::DisplayUnitInfoPtr& display_unit_info : info_list) {
    std::string display_id = display_unit_info->id;

    if (rotated_display_ids_.find(display_id) != rotated_display_ids_.end())
      continue;

    rotated_display_ids_.insert(display_id);
    display::Display::Rotation rotation(display_unit_info->rotation);
    if (rotation == display_rotation_default_)
      continue;

    // The following sets only the |rotation| property of the display
    // configuration; no other properties will be affected.
    auto config_properties = ash::mojom::DisplayConfigProperties::New();
    config_properties->rotation =
        ash::mojom::DisplayRotation::New(display_rotation_default_);
    cros_display_config->SetDisplayProperties(
        display_unit_info->id, std::move(config_properties),
        ash::mojom::DisplayConfigSource::kPolicy, base::DoNothing());
  }
}

}  // namespace policy
