// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_policy_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

class SecureDnsPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicyValue(const std::string& policy,
                      std::unique_ptr<base::Value> value) {
    policies_.Set(policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, std::move(value), nullptr);
  }
  bool CheckPolicySettings() {
    return handler_.CheckPolicySettings(policies_, &errors_);
  }
  void ApplyPolicySettings() {
    handler_.ApplyPolicySettings(policies_, &prefs_);
  }

  void CheckAndApplyPolicySettings() {
    if (CheckPolicySettings())
      ApplyPolicySettings();
  }

  PolicyErrorMap& errors() { return errors_; }
  PrefValueMap& prefs() { return prefs_; }

 private:
  PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
  SecureDnsPolicyHandler handler_;
};

TEST_F(SecureDnsPolicyHandlerTest, PolicyNotSet) {
  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
}

TEST_F(SecureDnsPolicyHandlerTest, EmptyPolicyValue) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(std::string()));

  CheckAndApplyPolicySettings();

  // Should have an error
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_NOT_SPECIFIED_ERROR);
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
}

TEST_F(SecureDnsPolicyHandlerTest, InvalidPolicyValue) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>("invalid"));

  CheckAndApplyPolicySettings();

  // Should have an error
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_INVALID_SECURE_DNS_MODE_ERROR);
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
}

TEST_F(SecureDnsPolicyHandlerTest, InvalidPolicyType) {
  // Give an int to a string-enum policy.
  SetPolicyValue(key::kDnsOverHttpsMode, std::make_unique<base::Value>(1));

  CheckAndApplyPolicySettings();

  // Should have an error
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_TYPE_ERROR,
      base::ASCIIToUTF16(base::Value::GetTypeName(base::Value::Type::STRING)));
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
}

// TODO(http://crbug.com/955454) This test should be removed once secure is a
// valid policy value.
TEST_F(SecureDnsPolicyHandlerTest, PolicyValueSecureShouldError) {
  // Secure will eventually be a valid option, but for the moment it should
  // error.
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(
                     chrome_browser_net::kDnsOverHttpsModeSecure));

  CheckAndApplyPolicySettings();

  // Should have an error
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_SECURE_DNS_MODE_NOT_SUPPORTED_ERROR);
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  std::string mode;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsMode, &mode));
  // Pref should have changed to "off."
  EXPECT_EQ(mode, chrome_browser_net::kDnsOverHttpsModeOff);
}

TEST_F(SecureDnsPolicyHandlerTest, ValidPolicyValueOff) {
  const std::string test_policy_value =
      chrome_browser_net::kDnsOverHttpsModeOff;

  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(test_policy_value));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  std::string mode;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsMode, &mode));
  // Pref should now be the test value.
  EXPECT_EQ(mode, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, ValidPolicyValueAutomatic) {
  const std::string test_policy_value =
      chrome_browser_net::kDnsOverHttpsModeAutomatic;

  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(test_policy_value));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  std::string mode;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsMode, &mode));
  // Pref should now be the test value.
  EXPECT_EQ(mode, test_policy_value);
}

}  // namespace policy
