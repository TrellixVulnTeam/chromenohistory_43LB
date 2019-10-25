// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_BASE_EARL_GREY_TEST_CASE_H_
#define IOS_TESTING_EARL_GREY_BASE_EARL_GREY_TEST_CASE_H_

#import <XCTest/XCTest.h>

// Base class for all Earl Grey tests.
// Provides EG1-compatible start-of-test-case hooks for EG2 tests,
// as well as handling common EG2 app-launching logic.
// This class also sets up code coverage by default.
@interface BaseEarlGreyTestCase : XCTestCase

// Invoked once per test case after launching test app from -setUp.
// Subclasses can use this method to perform class level setup instead of
// overriding +setUp, as due to EG2 limitations (crbug.com/961879) +setUp would
// execute before the application is launched and thus not function in the
// expected way. Subclasses must not call this method directly. Protected
// method.
+ (void)setUpForTestCase;

// Invoked upon starting each test method in a test case.
- (void)setUp NS_REQUIRES_SUPER;

// Responsible for launching the app under test in EG2 tests.
// By default, will launch the app once per test process launch with
// default parameters; individual test cases can override this method if they
// wish to pass different parameters or provide different behavior.
// Protected method.
- (void)launchAppForTestMethod;

@end

#endif  // IOS_TESTING_EARL_GREY_BASE_EARL_GREY_TEST_CASE_H_
