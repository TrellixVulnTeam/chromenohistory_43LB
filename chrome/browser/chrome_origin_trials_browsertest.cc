// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct DisabledItemsTestData {
  const std::vector<std::string> input_list;
  const std::string expected_switch;
};

static const char kNewPublicKey[] = "new public key";

const DisabledItemsTestData kDisabledFeaturesTests[] = {
    // One feature
    {{"A"}, "A"},
    // Two features
    {{"A", "B"}, "A|B"},
    // Three features
    {{"A", "B", "C"}, "A|B|C"},
    // Spaces in feature name
    {{"A", "B C"}, "A|B C"},
};

const DisabledItemsTestData kDisabledTokensTests[] = {
    // One token
    {{"t1"}, "t1"},
    // Two tokens
    {{"t1", "t2"}, "t1|t2"},
    // Three tokens
    {{"t1", "t2", "t3"}, "t1|t2|t3"},
};

class ChromeOriginTrialsTest : public InProcessBrowserTest {
 protected:
  ChromeOriginTrialsTest() {}

  std::string GetCommandLineSwitch(const base::StringPiece& switch_name) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    EXPECT_TRUE(command_line->HasSwitch(switch_name));
    return command_line->GetSwitchValueASCII(switch_name);
  }

  void AddDisabledFeaturesToPrefs(const std::vector<std::string>& features) {
    base::ListValue disabled_feature_list;
    disabled_feature_list.AppendStrings(features);
    ListPrefUpdate update(local_state(), prefs::kOriginTrialDisabledFeatures);
    update->Swap(&disabled_feature_list);
  }

  void AddDisabledTokensToPrefs(const std::vector<std::string>& tokens) {
    base::ListValue disabled_token_list;
    disabled_token_list.AppendStrings(tokens);
    ListPrefUpdate update(local_state(), prefs::kOriginTrialDisabledTokens);
    update->Swap(&disabled_token_list);
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeOriginTrialsTest);
};

// Tests to verify that the command line is not set, when no prefs exist for
// the various updates.

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsTest, NoPublicKeySet) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  EXPECT_FALSE(command_line->HasSwitch(switches::kOriginTrialPublicKey));
}

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsTest, NoDisabledFeatures) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  EXPECT_FALSE(command_line->HasSwitch(switches::kOriginTrialDisabledFeatures));
}

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsTest, NoDisabledTokens) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  EXPECT_FALSE(command_line->HasSwitch(switches::kOriginTrialDisabledTokens));
}

// Tests to verify that the public key is correctly read from prefs and
// added to the command line
IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsTest, PRE_PublicKeySetOnCommandLine) {
  local_state()->Set(prefs::kOriginTrialPublicKey, base::Value(kNewPublicKey));
  ASSERT_EQ(kNewPublicKey,
            local_state()->GetString(prefs::kOriginTrialPublicKey));
}

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsTest, PublicKeySetOnCommandLine) {
  ASSERT_EQ(kNewPublicKey,
            local_state()->GetString(prefs::kOriginTrialPublicKey));
  std::string actual = GetCommandLineSwitch(switches::kOriginTrialPublicKey);
  EXPECT_EQ(kNewPublicKey, actual);
}

// Tests to verify that disabled features are correctly read from prefs and
// added to the command line
class ChromeOriginTrialsDisabledFeaturesTest
    : public ChromeOriginTrialsTest,
      public ::testing::WithParamInterface<DisabledItemsTestData> {};

IN_PROC_BROWSER_TEST_P(ChromeOriginTrialsDisabledFeaturesTest,
                       PRE_DisabledFeaturesSetOnCommandLine) {
  AddDisabledFeaturesToPrefs(GetParam().input_list);
  ASSERT_TRUE(local_state()->HasPrefPath(prefs::kOriginTrialDisabledFeatures));
}

IN_PROC_BROWSER_TEST_P(ChromeOriginTrialsDisabledFeaturesTest,
                       DisabledFeaturesSetOnCommandLine) {
  ASSERT_TRUE(local_state()->HasPrefPath(prefs::kOriginTrialDisabledFeatures));
  std::string actual =
      GetCommandLineSwitch(switches::kOriginTrialDisabledFeatures);
  EXPECT_EQ(GetParam().expected_switch, actual);
}

INSTANTIATE_TEST_SUITE_P(,
                         ChromeOriginTrialsDisabledFeaturesTest,
                         ::testing::ValuesIn(kDisabledFeaturesTests));

// Tests to verify that disabled tokens are correctly read from prefs and
// added to the command line
class ChromeOriginTrialsDisabledTokensTest
    : public ChromeOriginTrialsTest,
      public ::testing::WithParamInterface<DisabledItemsTestData> {};

IN_PROC_BROWSER_TEST_P(ChromeOriginTrialsDisabledTokensTest,
                       PRE_DisabledTokensSetOnCommandLine) {
  AddDisabledTokensToPrefs(GetParam().input_list);
  ASSERT_TRUE(local_state()->HasPrefPath(prefs::kOriginTrialDisabledTokens));
}

IN_PROC_BROWSER_TEST_P(ChromeOriginTrialsDisabledTokensTest,
                       DisabledTokensSetOnCommandLine) {
  ASSERT_TRUE(local_state()->HasPrefPath(prefs::kOriginTrialDisabledTokens));
  std::string actual =
      GetCommandLineSwitch(switches::kOriginTrialDisabledTokens);
  EXPECT_EQ(GetParam().expected_switch, actual);
}

INSTANTIATE_TEST_SUITE_P(,
                         ChromeOriginTrialsDisabledTokensTest,
                         ::testing::ValuesIn(kDisabledTokensTests));

}  // namespace
