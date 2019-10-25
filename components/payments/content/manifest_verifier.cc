// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/manifest_verifier.h"

#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/payments/core/payment_manifest_downloader.h"
#include "components/payments/core/url_util.h"
#include "components/webdata/common/web_data_results.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

const char* const kAllOriginsSupportedIndicator = "*";

// Enables |method_manifest_url| in those |apps| that are from |app_origins|, if
// either |all_origins_supported| is true or |supported_origin_strings| contains
// the origin of the app.
void EnableMethodManifestUrlForSupportedApps(
    const GURL& method_manifest_url,
    const std::vector<std::string>& supported_origin_strings,
    bool all_origins_supported,
    const std::set<url::Origin>& app_origins,
    content::PaymentAppProvider::PaymentApps* apps,
    std::map<GURL, std::set<GURL>>* prohibited_payment_methods) {
  for (const auto& app_origin : app_origins) {
    for (auto& app : *apps) {
      if (app_origin.IsSameOriginWith(
              url::Origin::Create(app.second->scope.GetOrigin()))) {
        app.second->has_explicitly_verified_methods =
            base::Contains(supported_origin_strings, app_origin.Serialize());
        if (all_origins_supported ||
            app.second->has_explicitly_verified_methods) {
          app.second->enabled_methods.emplace_back(method_manifest_url.spec());
          prohibited_payment_methods->at(app.second->scope)
              .erase(method_manifest_url);
        }
      }
    }
  }
}

}  // namespace

ManifestVerifier::ManifestVerifier(content::WebContents* web_contents,
                                   PaymentManifestDownloader* downloader,
                                   PaymentManifestParser* parser,
                                   PaymentManifestWebDataService* cache)
    : log_(web_contents),
      downloader_(downloader),
      parser_(parser),
      cache_(cache),
      number_of_manifests_to_verify_(0),
      number_of_manifests_to_download_(0) {}

ManifestVerifier::~ManifestVerifier() {
  for (const auto& handle : cache_request_handles_) {
    cache_->CancelRequest(handle.first);
  }
}

void ManifestVerifier::Verify(content::PaymentAppProvider::PaymentApps apps,
                              VerifyCallback finished_verification,
                              base::OnceClosure finished_using_resources) {
  DCHECK(apps_.empty());
  DCHECK(finished_verification_callback_.is_null());
  DCHECK(finished_using_resources_callback_.is_null());

  apps_ = std::move(apps);
  finished_verification_callback_ = std::move(finished_verification);
  finished_using_resources_callback_ = std::move(finished_using_resources);

  std::set<GURL> manifests_to_download;
  for (auto& app : apps_) {
    std::vector<std::string> verified_method_names;
    for (const auto& method : app.second->enabled_methods) {
      // For non-URL payment method names, only names published by W3C should be
      // supported. Keep this in sync with AndroidPaymentAppFinder.java.
      // https://w3c.github.io/payment-method-basic-card/
      // https://w3c.github.io/webpayments/proposals/interledger-payment-method.html
      // https://w3c.github.io/webpayments-methods-credit-transfer-direct-debit/
      // https://w3c.github.io/webpayments-methods-tokenization/
      if (method == "basic-card" || method == "interledger" ||
          method == "payee-credit-transfer" ||
          method == "payer-credit-transfer" || method == "tokenized-card") {
        verified_method_names.emplace_back(method);
        continue;
      }

      // GURL constructor may crash with some invalid unicode strings.
      if (!base::IsStringUTF8(method)) {
        log_.Warn("Payment method name \"" + method +
                  "\" in payment handler \"" + app.second->scope.spec() +
                  "\"  is not valid unicode.");
        continue;
      }

      GURL method_manifest_url = GURL(method);
      if (!UrlUtil::IsValidUrlBasedPaymentMethodIdentifier(
              method_manifest_url)) {
        log_.Warn(
            "\"" + method +
            "\" is not a valid payment method name in payment handler \"" +
            app.second->scope.spec() + "\".");
        continue;
      }

      // Same origin payment methods are always allowed.
      url::Origin app_origin =
          url::Origin::Create(app.second->scope.GetOrigin());
      if (url::Origin::Create(method_manifest_url.GetOrigin())
              .IsSameOriginWith(app_origin)) {
        verified_method_names.emplace_back(method);
        app.second->has_explicitly_verified_methods = true;
        continue;
      }

      manifests_to_download.insert(method_manifest_url);
      manifest_url_to_app_origins_map_[method_manifest_url].insert(app_origin);
      prohibited_payment_methods_[app.second->scope].insert(
          method_manifest_url);
    }

    app.second->enabled_methods.swap(verified_method_names);
  }

  number_of_manifests_to_verify_ = number_of_manifests_to_download_ =
      manifests_to_download.size();
  if (number_of_manifests_to_verify_ == 0) {
    RemoveInvalidPaymentApps();
    std::move(finished_verification_callback_)
        .Run(std::move(apps_), first_error_message_);
    std::move(finished_using_resources_callback_).Run();
    return;
  }

  for (const auto& method_manifest_url : manifests_to_download) {
    cache_request_handles_[cache_->GetPaymentMethodManifest(
        method_manifest_url.spec(), this)] = method_manifest_url;
  }
}

void ManifestVerifier::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK_LT(0U, number_of_manifests_to_verify_);

  auto it = cache_request_handles_.find(h);
  if (it == cache_request_handles_.end())
    return;

  GURL method_manifest_url = it->second;
  cache_request_handles_.erase(it);

  const std::vector<std::string>& cached_strings =
      (static_cast<const WDResult<std::vector<std::string>>*>(result.get()))
          ->GetValue();

  bool all_origins_supported = false;
  std::vector<std::string> native_app_ids;
  std::vector<std::string> supported_origin_strings;
  for (const auto& origin_or_id : cached_strings) {
    // The string could be "*", origin or native payment app package Id on
    // Android.
    if (origin_or_id == kAllOriginsSupportedIndicator) {
      all_origins_supported = true;
      continue;
    }

    if (base::IsStringUTF8(origin_or_id) && GURL(origin_or_id).is_valid()) {
      supported_origin_strings.emplace_back(origin_or_id);
    } else {
      native_app_ids.emplace_back(origin_or_id);
    }
  }
  cached_supported_native_app_ids_[method_manifest_url] = native_app_ids;

  EnableMethodManifestUrlForSupportedApps(
      method_manifest_url, supported_origin_strings, all_origins_supported,
      manifest_url_to_app_origins_map_[method_manifest_url], &apps_,
      &prohibited_payment_methods_);

  if (!supported_origin_strings.empty()) {
    cached_manifest_urls_.insert(method_manifest_url);
    if (--number_of_manifests_to_verify_ == 0) {
      RemoveInvalidPaymentApps();
      std::move(finished_verification_callback_)
          .Run(std::move(apps_), first_error_message_);
    }
  }

  downloader_->DownloadPaymentMethodManifest(
      method_manifest_url,
      base::BindOnce(&ManifestVerifier::OnPaymentMethodManifestDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), method_manifest_url));
}

void ManifestVerifier::OnPaymentMethodManifestDownloaded(
    const GURL& method_manifest_url,
    const GURL& unused_method_manifest_url_after_redirects,
    const std::string& content,
    const std::string& error_message) {
  DCHECK_LT(0U, number_of_manifests_to_download_);

  if (content.empty()) {
    if (first_error_message_.empty())
      first_error_message_ = error_message;
    if (cached_manifest_urls_.find(method_manifest_url) ==
            cached_manifest_urls_.end() &&
        --number_of_manifests_to_verify_ == 0) {
      RemoveInvalidPaymentApps();
      std::move(finished_verification_callback_)
          .Run(std::move(apps_), first_error_message_);
    }

    if (--number_of_manifests_to_download_ == 0)
      std::move(finished_using_resources_callback_).Run();

    return;
  }

  parser_->ParsePaymentMethodManifest(
      content,
      base::BindOnce(&ManifestVerifier::OnPaymentMethodManifestParsed,
                     weak_ptr_factory_.GetWeakPtr(), method_manifest_url));
}

void ManifestVerifier::OnPaymentMethodManifestParsed(
    const GURL& method_manifest_url,
    const std::vector<GURL>& default_applications,
    const std::vector<url::Origin>& supported_origins,
    bool all_origins_supported) {
  DCHECK_LT(0U, number_of_manifests_to_download_);

  std::vector<std::string> supported_origin_strings(supported_origins.size());
  std::transform(supported_origins.begin(), supported_origins.end(),
                 supported_origin_strings.begin(),
                 [](const auto& origin) { return origin.Serialize(); });

  if (cached_manifest_urls_.find(method_manifest_url) ==
      cached_manifest_urls_.end()) {
    EnableMethodManifestUrlForSupportedApps(
        method_manifest_url, supported_origin_strings, all_origins_supported,
        manifest_url_to_app_origins_map_[method_manifest_url], &apps_,
        &prohibited_payment_methods_);

    if (--number_of_manifests_to_verify_ == 0) {
      RemoveInvalidPaymentApps();
      std::move(finished_verification_callback_)
          .Run(std::move(apps_), first_error_message_);
    }
  }

  if (all_origins_supported)
    supported_origin_strings.emplace_back(kAllOriginsSupportedIndicator);

  // Keep Android native payment app Ids in cache.
  std::map<GURL, std::vector<std::string>>::const_iterator it =
      cached_supported_native_app_ids_.find(method_manifest_url);
  if (it != cached_supported_native_app_ids_.end()) {
    supported_origin_strings.insert(supported_origin_strings.end(),
                                    it->second.begin(), it->second.end());
  }

  cache_->AddPaymentMethodManifest(method_manifest_url.spec(),
                                   supported_origin_strings);

  if (--number_of_manifests_to_download_ == 0)
    std::move(finished_using_resources_callback_).Run();
}

void ManifestVerifier::RemoveInvalidPaymentApps() {
  // Notify the web developer that a payment app cannot use certain payment
  // methods.
  for (const auto& it : prohibited_payment_methods_) {
    DCHECK(it.first.is_valid());
    std::string app_scope = it.first.spec();
    std::string app_origin = it.first.GetOrigin().spec();
    const std::set<GURL>& methods = it.second;
    for (const GURL& method : methods) {
      DCHECK(method.is_valid());
      log_.Warn("The payment handler \"" + app_scope +
                "\" is not allowed to use payment method \"" + method.spec() +
                "\", because the payment handler origin \"" + app_origin +
                "\" is different from the payment method origin \"" +
                method.GetOrigin().spec() +
                "\" and the \"supported_origins\" field in the payment method "
                "manifest for \"" +
                method.spec() +
                "\" is not \"*\" and is not a list that includes \"" +
                app_origin + "\".");
    }
  }

  // Remove apps without enabled methods.
  for (auto it = apps_.begin(); it != apps_.end();) {
    if (it->second->enabled_methods.empty())
      it = apps_.erase(it);
    else
      ++it;
  }
}

}  // namespace payments
