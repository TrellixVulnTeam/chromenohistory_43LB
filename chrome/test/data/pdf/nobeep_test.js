// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  /**
   * Test that blocked JS was not able to call back via "app.beep()"
   */
  function testHasCorrectBeepCount() {
    chrome.test.assertEq(0, viewer.beepCount_);
    chrome.test.succeed();
  }
];

var scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
