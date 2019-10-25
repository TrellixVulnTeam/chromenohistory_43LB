// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/stl_util.h"
#import "ios/web/navigation/nscoder_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

typedef PlatformTest NSCoderStdStringTest;

const char* testStrings[] = {
    "Arf",
    "",
    "This is working™",
    "古池や蛙飛込む水の音\nふるいけやかわずとびこむみずのおと",
    "ἀγεωμέτρητος μηδεὶς εἰσίτω",
    "Bang!\t\n"
};

TEST_F(NSCoderStdStringTest, encodeDecode) {
  for (size_t i = 0; i < base::size(testStrings); ++i) {
    NSKeyedArchiver* archiver =
        [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    nscoder_util::EncodeString(archiver, @"test", testStrings[i]);

    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:[archiver encodedData]
                                                    error:nil];
    const std::string decoded = nscoder_util::DecodeString(unarchiver, @"test");

    EXPECT_EQ(decoded, testStrings[i]);
  }
}

TEST_F(NSCoderStdStringTest, decodeEmpty) {
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:[archiver encodedData]
                                                  error:nil];
  const std::string decoded = nscoder_util::DecodeString(unarchiver, @"test");

  EXPECT_EQ(decoded, "");
}

}  // namespace
}  // namespace web
