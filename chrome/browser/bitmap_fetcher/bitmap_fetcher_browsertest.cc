// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

const bool kAsyncCall = true;
const bool kSyncCall = false;
const char kStartTestURL[] = "/this-should-work";
const char kOnImageDecodedTestURL[] = "/this-should-work-as-well";
const char kOnURLFetchFailureTestURL[] = "/this-should-be-fetch-failure";

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

// Class to catch events from the BitmapFetcher for testing.
class BitmapFetcherTestDelegate : public BitmapFetcherDelegate {
 public:
  explicit BitmapFetcherTestDelegate(bool async) : called_(false),
                                                   success_(false),
                                                   async_(async) {}

  ~BitmapFetcherTestDelegate() override { EXPECT_TRUE(called_); }

  // Method inherited from BitmapFetcherDelegate.
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override {
    called_ = true;
    url_ = url;
    if (bitmap) {
      success_ = true;
      if (bitmap_.tryAllocPixels(bitmap->info())) {
        bitmap->readPixels(bitmap_.info(), bitmap_.getPixels(),
                           bitmap_.rowBytes(), 0, 0);
      }
    }
    // For async calls, we need to quit the run loop so the test can continue.
    if (async_)
      run_loop_.Quit();
  }

  // Waits until OnFetchComplete() is called. Should only be used for
  // async tests.
  void Wait() {
    ASSERT_TRUE(async_);
    run_loop_.Run();
  }

  GURL url() const { return url_; }
  bool success() const { return success_; }
  const SkBitmap& bitmap() const { return bitmap_; }

 private:
  base::RunLoop run_loop_;
  bool called_;
  GURL url_;
  bool success_;
  bool async_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(BitmapFetcherTestDelegate);
};

class BitmapFetcherBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Set up the test server.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &BitmapFetcherBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Tear down the test server.
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  SkBitmap test_bitmap() const {
    // Return some realistic looking bitmap data
    SkBitmap image;

    // Put a real bitmap into "image".  2x2 bitmap of green 32 bit pixels.
    image.allocN32Pixels(2, 2);
    image.eraseColor(SK_ColorGREEN);
    return image;
  }

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    std::unique_ptr<BasicHttpResponse> response(new BasicHttpResponse);
    if (request.relative_url == kStartTestURL) {
      // Encode the bits as a PNG.
      std::vector<unsigned char> compressed;
      gfx::PNGCodec::EncodeBGRASkBitmap(test_bitmap(), true, &compressed);
      // Copy the bits into a string and return them through the embedded
      // test server
      std::string image_string(compressed.begin(), compressed.end());
      response->set_code(net::HTTP_OK);
      response->set_content(image_string);
    } else if (request.relative_url == kOnImageDecodedTestURL) {
      response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    } else if (request.relative_url == kOnURLFetchFailureTestURL) {
      response->set_code(net::HTTP_OK);
      response->set_content(std::string("Not a real bitmap"));
    }
    return std::move(response);
  }
};

// WARNING:  These tests work with --single-process-tests, but not
// --single-process.  The reason is that the sandbox does not get created
// for us by the test process if --single-process is used.

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, StartTest) {
  GURL url = embedded_test_server()->GetURL(kStartTestURL);

  // Set up a delegate to wait for the callback.
  BitmapFetcherTestDelegate delegate(kAsyncCall);

  BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  // We expect that the image decoder will get called and return
  // an image in a callback to OnImageDecoded().
  fetcher.Init(
      std::string(),
      net::URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      net::LOAD_NORMAL);
  fetcher.Start(
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());

  // Blocks until test delegate is notified via a callback.
  delegate.Wait();

  ASSERT_TRUE(delegate.success());

  // Make sure we get back the bitmap we expect.
  const SkBitmap& found_image = delegate.bitmap();
  EXPECT_TRUE(gfx::BitmapsAreEqual(test_bitmap(), found_image));
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, OnImageDecodedTest) {
  GURL url = embedded_test_server()->GetURL(kOnImageDecodedTestURL);
  SkBitmap image;

  // Put a real bitmap into "image".  2x2 bitmap of green 16 bit pixels.
  image.allocN32Pixels(2, 2);
  image.eraseColor(SK_ColorGREEN);

  BitmapFetcherTestDelegate delegate(kSyncCall);

  BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  fetcher.OnImageDecoded(image);

  // Ensure image is marked as succeeded.
  EXPECT_TRUE(delegate.success());

  // Test that the image is what we expect.
  EXPECT_TRUE(gfx::BitmapsAreEqual(image, delegate.bitmap()));
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, OnURLFetchFailureTest) {
  GURL url = embedded_test_server()->GetURL(kOnURLFetchFailureTestURL);

  // We intentionally put no data into the bitmap to simulate a failure.

  // Set up a delegate to wait for the callback.
  BitmapFetcherTestDelegate delegate(kAsyncCall);

  BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  fetcher.Init(
      std::string(),
      net::URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      net::LOAD_NORMAL);
  fetcher.Start(
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());

  // Blocks until test delegate is notified via a callback.
  delegate.Wait();

  EXPECT_FALSE(delegate.success());
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, HandleImageFailedTest) {
  GURL url("http://example.com/this-should-be-a-decode-failure");
  BitmapFetcherTestDelegate delegate(kAsyncCall);
  BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  fetcher.Init(
      std::string(),
      net::URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      net::LOAD_NORMAL);
  fetcher.Start(
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());

  // Blocks until test delegate is notified via a callback.
  delegate.Wait();

  EXPECT_FALSE(delegate.success());
}
