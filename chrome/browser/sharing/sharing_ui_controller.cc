// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_ui_controller.h"

#include <utility>

#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/sharing/sharing_dialog_data.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

BrowserWindow* GetWindowFromWebContents(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser ? browser->window() : nullptr;
}

content::WebContents* GetCurrentWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser ? browser->tab_strip_model()->GetActiveWebContents() : nullptr;
}

SharingDialogType GetSharingDialogType(bool has_devices, bool has_apps) {
  if (has_devices)
    return SharingDialogType::kDialogWithDevicesMaybeApps;
  if (has_apps)
    return SharingDialogType::kDialogWithoutDevicesWithApp;
  return SharingDialogType::kEducationalDialog;
}

}  // namespace

SharingUiController::SharingUiController(content::WebContents* web_contents)
    : web_contents_(web_contents),
      sharing_service_(SharingServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())) {}

SharingUiController::~SharingUiController() = default;

base::string16 SharingUiController::GetTitle(SharingDialogType dialog_type) {
  // We only handle error messages generically.
  DCHECK_EQ(SharingDialogType::kErrorDialog, dialog_type);
  switch (send_result()) {
    case SharingSendMessageResult::kDeviceNotFound:
    case SharingSendMessageResult::kNetworkError:
    case SharingSendMessageResult::kAckTimeout:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TITLE_GENERIC_ERROR,
          base::ToLowerASCII(GetContentType()));

    case SharingSendMessageResult::kSuccessful:
      NOTREACHED();
      FALLTHROUGH;

    case SharingSendMessageResult::kPayloadTooLarge:
    case SharingSendMessageResult::kInternalError:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TITLE_INTERNAL_ERROR,
          base::ToLowerASCII(GetContentType()));
  }
}

base::string16 SharingUiController::GetErrorDialogText() const {
  switch (send_result()) {
    case SharingSendMessageResult::kDeviceNotFound:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TEXT_DEVICE_NOT_FOUND,
          GetTargetDeviceName());

    case SharingSendMessageResult::kNetworkError:
      return l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TEXT_NETWORK_ERROR);

    case SharingSendMessageResult::kAckTimeout:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TEXT_DEVICE_ACK_TIMEOUT,
          GetTargetDeviceName());

    case SharingSendMessageResult::kSuccessful:
      return base::string16();

    case SharingSendMessageResult::kPayloadTooLarge:
    case SharingSendMessageResult::kInternalError:
      return l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TEXT_INTERNAL_ERROR);
  }
}

void SharingUiController::OnDialogClosed(SharingDialog* dialog) {
  // Ignore already replaced dialogs.
  if (dialog != dialog_)
    return;

  dialog_ = nullptr;
  UpdateIcon();
}

void SharingUiController::OnHelpTextClicked(SharingDialogType dialog_type) {
  ShowSingletonTab(chrome::FindBrowserWithWebContents(web_contents()),
                   GURL(chrome::kSyncLearnMoreURL));
}

void SharingUiController::OnDialogShown(bool has_devices, bool has_apps) {
  if (on_dialog_shown_closure_for_testing_)
    std::move(on_dialog_shown_closure_for_testing_).Run();
}

void SharingUiController::ClearLastDialog() {
  last_dialog_id_++;
  is_loading_ = false;
  send_result_ = SharingSendMessageResult::kSuccessful;
  CloseDialog();
}

void SharingUiController::UpdateAndShowDialog(
    const base::Optional<url::Origin>& initiating_origin) {
  ClearLastDialog();
  DoUpdateApps(base::BindOnce(&SharingUiController::OnAppsReceived,
                              weak_ptr_factory_.GetWeakPtr(), last_dialog_id_,
                              initiating_origin));
}

std::vector<std::unique_ptr<syncer::DeviceInfo>>
SharingUiController::GetDevices() {
  return sharing_service_->GetDeviceCandidates(GetRequiredFeature());
}

bool SharingUiController::HasSendFailed() const {
  return send_result_ != SharingSendMessageResult::kSuccessful;
}

void SharingUiController::MaybeShowErrorDialog() {
  if (HasSendFailed() && web_contents_ == GetCurrentWebContents(web_contents_))
    ShowNewDialog(CreateDialogData(SharingDialogType::kErrorDialog));
}

SharingDialogData SharingUiController::CreateDialogData(
    SharingDialogType dialog_type) {
  SharingDialogData data;

  data.type = dialog_type;
  data.prefix = GetFeatureMetricsPrefix();
  data.title = GetTitle(data.type);
  data.error_text = GetErrorDialogText();

  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  data.help_callback =
      base::BindOnce(&SharingUiController::OnHelpTextClicked, weak_ptr);
  data.device_callback =
      base::BindOnce(&SharingUiController::OnDeviceChosen, weak_ptr);
  data.app_callback =
      base::BindOnce(&SharingUiController::OnAppChosen, weak_ptr);
  data.close_callback =
      base::BindOnce(&SharingUiController::OnDialogClosed, weak_ptr);
  return data;
}

void SharingUiController::SendMessageToDevice(
    const syncer::DeviceInfo& device,
    chrome_browser_sharing::SharingMessage sharing_message) {
  last_dialog_id_++;
  is_loading_ = true;
  send_result_ = SharingSendMessageResult::kSuccessful;
  target_device_name_ = device.client_name();
  UpdateIcon();

  sharing_service_->SendMessageToDevice(
      device.guid(), kSharingMessageTTL, std::move(sharing_message),
      base::Bind(&SharingUiController::OnMessageSentToDevice,
                 weak_ptr_factory_.GetWeakPtr(), last_dialog_id_));
}

void SharingUiController::UpdateIcon() {
  BrowserWindow* window = GetWindowFromWebContents(web_contents_);
  if (!window)
    return;

  window->UpdatePageActionIcon(GetIconType());
}

void SharingUiController::CloseDialog() {
  if (!dialog_)
    return;

  dialog_->Hide();

  // SharingDialog::Hide may close the dialog asynchronously, and therefore not
  // call OnDialogClosed immediately. If that is the case, call OnDialogClosed
  // now to notify subclasses and clear |dialog_|.
  if (dialog_)
    OnDialogClosed(dialog_);

  DCHECK(!dialog_);
}

void SharingUiController::ShowNewDialog(SharingDialogData dialog_data) {
  CloseDialog();
  BrowserWindow* window = GetWindowFromWebContents(web_contents_);
  if (!window)
    return;
  bool has_devices = !dialog_data.devices.empty();
  bool has_apps = !dialog_data.apps.empty();
  dialog_ = window->ShowSharingDialog(web_contents(), std::move(dialog_data));
  UpdateIcon();
  OnDialogShown(has_devices, has_apps);
}

base::string16 SharingUiController::GetTargetDeviceName() const {
  return base::UTF8ToUTF16(target_device_name_);
}

void SharingUiController::OnMessageSentToDevice(
    int dialog_id,
    SharingSendMessageResult result) {
  if (dialog_id != last_dialog_id_)
    return;

  is_loading_ = false;
  send_result_ = result;
  UpdateIcon();
}

void SharingUiController::OnAppsReceived(
    int dialog_id,
    const base::Optional<url::Origin>& initiating_origin,
    std::vector<SharingApp> apps) {
  if (dialog_id != last_dialog_id_)
    return;

  auto devices = GetDevices();

  SharingDialogData dialog_data =
      CreateDialogData(GetSharingDialogType(!devices.empty(), !apps.empty()));
  dialog_data.devices = std::move(devices);
  dialog_data.apps = std::move(apps);
  dialog_data.initiating_origin = initiating_origin;

  ShowNewDialog(std::move(dialog_data));
}
