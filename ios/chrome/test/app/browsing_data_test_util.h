// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_BROWSING_DATA_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_BROWSING_DATA_TEST_UTIL_H_

#include "base/compiler_specific.h"

namespace chrome_test_util {

// Clears browsing history and returns whether clearing the history was
// successful or timed out.
bool ClearBrowsingHistory() WARN_UNUSED_RESULT;

// Clears browsing data and returns whether clearing was successful or timed
// out.
bool ClearAllBrowsingData(bool off_the_record) WARN_UNUSED_RESULT;

// Clears user decisions cache and returns whether clearing was successful or
// timed out.
bool ClearCertificatePolicyCache(bool off_the_record) WARN_UNUSED_RESULT;

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_BROWSING_DATA_TEST_UTIL_H_
