// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/screen_lock_service_provider.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ScreenLockServiceProvider::ScreenLockServiceProvider() {}

ScreenLockServiceProvider::~ScreenLockServiceProvider() = default;

void ScreenLockServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kScreenLockServiceInterface, kScreenLockServiceShowLockScreenMethod,
      base::BindRepeating(&ScreenLockServiceProvider::ShowLockScreen,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ScreenLockServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ScreenLockServiceProvider::OnExported(const std::string& interface_name,
                                           const std::string& method_name,
                                           bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void ScreenLockServiceProvider::ShowLockScreen(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  // Please add any additional logic to
  // ScreenLocker::HandleShowLockScreenRequest() instead of placing it here.
  ScreenLocker::HandleShowLockScreenRequest();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace chromeos
