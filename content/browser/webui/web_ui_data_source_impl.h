// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_DATA_SOURCE_IMPL_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_DATA_SOURCE_IMPL_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/values.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/template_expressions.h"

namespace content {

// A data source that can help with implementing the common operations
// needed by the chrome WEBUI settings/history/downloads pages.
class CONTENT_EXPORT WebUIDataSourceImpl : public URLDataSourceImpl,
                                           public WebUIDataSource {
 public:
  // WebUIDataSource:
  void AddString(base::StringPiece name, const base::string16& value) override;
  void AddString(base::StringPiece name, const std::string& value) override;
  void AddLocalizedString(base::StringPiece name, int ids) override;
  void AddLocalizedStrings(
      const base::DictionaryValue& localized_strings) override;
  void AddBoolean(base::StringPiece name, bool value) override;
  void AddInteger(base::StringPiece name, int32_t value) override;
  void UseStringsJs() override;
  void AddResourcePath(base::StringPiece path, int resource_id) override;
  void SetDefaultResource(int resource_id) override;
  void SetRequestFilter(const WebUIDataSource::ShouldHandleRequestCallback&
                            should_handle_request_callback,
                        const WebUIDataSource::HandleRequestCallback&
                            handle_request_callback) override;
  void DisableReplaceExistingSource() override;
  void DisableContentSecurityPolicy() override;
  void OverrideContentSecurityPolicyScriptSrc(const std::string& data) override;
  void OverrideContentSecurityPolicyObjectSrc(const std::string& data) override;
  void OverrideContentSecurityPolicyChildSrc(const std::string& data) override;
  void OverrideContentSecurityPolicyWorkerSrc(const std::string& data) override;
  void DisableDenyXFrameOptions() override;
  void EnableReplaceI18nInJS() override;
  std::string GetSource() override;

  // URLDataSourceImpl:
  const ui::TemplateReplacements* GetReplacements() const override;

  // Add the locale to the load time data defaults. May be called repeatedly.
  void EnsureLoadTimeDataDefaultsAdded();

  bool IsWebUIDataSourceImpl() const override;

 protected:
  explicit WebUIDataSourceImpl(const std::string& source_name);
  ~WebUIDataSourceImpl() override;

  // Completes a request by sending our dictionary of localized strings.
  void SendLocalizedStringsAsJSON(
      const URLDataSource::GotDataCallback& callback,
      bool from_js_module);

  // Protected for testing.
  virtual const base::DictionaryValue* GetLocalizedStrings() const;

 private:
  class InternalDataSource;
  friend class InternalDataSource;
  friend class WebUIDataSource;
  friend class WebUIDataSourceTest;

  // Methods that match URLDataSource which are called by
  // InternalDataSource.
  std::string GetMimeType(const std::string& path) const;
  void StartDataRequest(const GURL& url,
                        const WebContents::Getter& wc_getter,
                        const URLDataSource::GotDataCallback& callback);

  int PathToIdrOrDefault(const std::string& path) const;

  // Note: this must be called before StartDataRequest() to have an effect.
  void disable_load_time_data_defaults_for_testing() {
    add_load_time_data_defaults_ = false;
  }

  bool ShouldReplaceI18nInJS() const;

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source.
  std::string source_name_;
  int default_resource_;
  bool use_strings_js_ = false;
  std::map<std::string, int> path_to_idr_map_;
  // The replacements are initiallized in the main thread and then used in the
  // IO thread. The map is safe to read from multiple threads as long as no
  // futher changes are made to it after initialization.
  ui::TemplateReplacements replacements_;
  // The |replacements_| is intended to replace |localized_strings_|.
  // TODO(dschuyler): phase out |localized_strings_| in Q1 2017. (Or rename
  // to |load_time_flags_| if the usage is reduced to storing flags only).
  base::DictionaryValue localized_strings_;
  WebUIDataSource::HandleRequestCallback filter_callback_;
  WebUIDataSource::ShouldHandleRequestCallback should_handle_request_callback_;

  bool add_csp_ = true;
  bool script_src_set_ = false;
  std::string script_src_;
  bool object_src_set_ = false;
  std::string object_src_;
  bool frame_src_set_ = false;
  std::string frame_src_;
  bool worker_src_set_ = false;
  std::string worker_src_;
  bool deny_xframe_options_ = true;
  bool add_load_time_data_defaults_ = true;
  bool replace_existing_source_ = true;
  bool should_replace_i18n_in_js_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebUIDataSourceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_DATA_SOURCE_IMPL_H_
