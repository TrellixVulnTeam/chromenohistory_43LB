// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/fake_android_sms_app_setup_controller.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"

namespace chromeos {

namespace android_sms {

FakeAndroidSmsAppSetupController::AppMetadata::AppMetadata() = default;

FakeAndroidSmsAppSetupController::AppMetadata::AppMetadata(
    const AppMetadata& other) = default;

FakeAndroidSmsAppSetupController::AppMetadata::~AppMetadata() = default;

FakeAndroidSmsAppSetupController::FakeAndroidSmsAppSetupController() = default;

FakeAndroidSmsAppSetupController::~FakeAndroidSmsAppSetupController() = default;

const FakeAndroidSmsAppSetupController::AppMetadata*
FakeAndroidSmsAppSetupController::GetAppMetadataAtUrl(
    const GURL& install_url) const {
  const auto it = install_url_to_metadata_map_.find(install_url);
  if (it == install_url_to_metadata_map_.end())
    return nullptr;
  return std::addressof(it->second);
}

void FakeAndroidSmsAppSetupController::SetAppAtUrl(
    const GURL& install_url,
    const base::Optional<extensions::ExtensionId>& id_for_app) {
  if (!id_for_app) {
    install_url_to_metadata_map_.erase(install_url);
    return;
  }

  // Create a test Extension and add it to |install_url_to_metadata_map_|.
  base::FilePath path;
  base::PathService::Get(extensions::DIR_TEST_DATA, &path);
  install_url_to_metadata_map_[install_url].pwa =
      extensions::ExtensionBuilder(
          install_url.spec(), extensions::ExtensionBuilder::Type::PLATFORM_APP)
          .SetPath(path.AppendASCII(install_url.spec()))
          .SetID(*id_for_app)
          .Build();
}

void FakeAndroidSmsAppSetupController::CompletePendingSetUpAppRequest(
    const GURL& expected_app_url,
    const GURL& expected_install_url,
    const base::Optional<extensions::ExtensionId>& id_for_app) {
  DCHECK(!pending_set_up_app_requests_.empty());

  auto request = std::move(pending_set_up_app_requests_.front());
  pending_set_up_app_requests_.erase(pending_set_up_app_requests_.begin());
  DCHECK_EQ(expected_app_url, std::get<0>(*request));
  DCHECK_EQ(expected_install_url, std::get<1>(*request));

  if (!id_for_app) {
    std::move(std::get<2>(*request)).Run(false /* success */);
    return;
  }

  SetAppAtUrl(expected_install_url, *id_for_app);
  std::move(std::get<2>(*request)).Run(true /* success */);
}

void FakeAndroidSmsAppSetupController::CompletePendingDeleteCookieRequest(
    const GURL& expected_app_url,
    const GURL& expected_install_url) {
  DCHECK(!pending_delete_cookie_requests_.empty());

  auto request = std::move(pending_delete_cookie_requests_.front());
  pending_delete_cookie_requests_.erase(
      pending_delete_cookie_requests_.begin());
  DCHECK_EQ(expected_app_url, request->first);

  // The app must exist before the cookie is deleted.
  auto it = install_url_to_metadata_map_.find(expected_install_url);
  DCHECK(it != install_url_to_metadata_map_.end());

  it->second.is_cookie_present = false;

  std::move(request->second).Run(true /* success */);
}

void FakeAndroidSmsAppSetupController::CompleteRemoveAppRequest(
    const GURL& expected_app_url,
    const GURL& expected_install_url,
    const GURL& expected_migrated_to_app_url,
    bool should_succeed) {
  DCHECK(!pending_remove_app_requests_.empty());

  auto request = std::move(pending_remove_app_requests_.front());
  pending_remove_app_requests_.erase(pending_remove_app_requests_.begin());
  DCHECK_EQ(expected_app_url, std::get<0>(*request));
  DCHECK_EQ(expected_install_url, std::get<1>(*request));
  DCHECK_EQ(expected_migrated_to_app_url, std::get<2>(*request));

  if (should_succeed)
    SetAppAtUrl(expected_install_url, base::nullopt /* id_for_app */);

  std::move(std::get<3>(*request)).Run(should_succeed);
}

void FakeAndroidSmsAppSetupController::SetUpApp(const GURL& app_url,
                                                const GURL& install_url,
                                                SuccessCallback callback) {
  pending_set_up_app_requests_.push_back(std::make_unique<SetUpAppData>(
      app_url, install_url, std::move(callback)));
}

const extensions::Extension* FakeAndroidSmsAppSetupController::GetPwa(
    const GURL& install_url) {
  auto it = install_url_to_metadata_map_.find(install_url);
  if (it == install_url_to_metadata_map_.end())
    return nullptr;
  return it->second.pwa.get();
}

void FakeAndroidSmsAppSetupController::DeleteRememberDeviceByDefaultCookie(
    const GURL& app_url,
    SuccessCallback callback) {
  pending_delete_cookie_requests_.push_back(
      std::make_unique<DeleteCookieData>(app_url, std::move(callback)));
}

void FakeAndroidSmsAppSetupController::RemoveApp(
    const GURL& app_url,
    const GURL& install_url,
    const GURL& migrated_to_app_url,
    SuccessCallback callback) {
  pending_remove_app_requests_.push_back(std::make_unique<RemoveAppData>(
      app_url, install_url, migrated_to_app_url, std::move(callback)));
}

}  // namespace android_sms

}  // namespace chromeos
