// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_THUMBNAIL_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_THUMBNAIL_UTIL_H_

#include "base/callback.h"

class SkBitmap;

// Scale down the bitmap. The result is returned in the |callback|. The aspect
// ratio is kept. The maximum size of the width or height will be no greater
// than |icon_size|.
void ScaleDownBitmap(int icon_size,
                     const SkBitmap& bitmap,
                     base::OnceCallback<void(SkBitmap)> callback);

#endif  // CHROME_BROWSER_DOWNLOAD_THUMBNAIL_UTIL_H_
