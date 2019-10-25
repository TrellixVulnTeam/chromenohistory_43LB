// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_APP_SHORTCUTS_ARC_APP_SHORTCUTS_REQUEST_H_
#define CHROME_BROWSER_CHROMEOS_ARC_APP_SHORTCUTS_ARC_APP_SHORTCUTS_REQUEST_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/app_shortcuts/arc_app_shortcut_item.h"
#include "components/arc/mojom/app.mojom.h"

namespace arc {

class IconDecodeRequest;

// A helper class that sends querying app shortcuts request to Android on behalf
// of Chrome.
class ArcAppShortcutsRequest {
 public:
  using GetAppShortcutItemsCallback =
      base::OnceCallback<void(std::unique_ptr<ArcAppShortcutItems>)>;

  explicit ArcAppShortcutsRequest(GetAppShortcutItemsCallback callback);
  ~ArcAppShortcutsRequest();

  // Starts querying app shortcuts for |package_name|. Results are retruned in
  // |callback_|. It shouldn't be called more than one time for the life cycle
  // of |this|.
  void StartForPackage(const std::string& package_name);

 private:
  // Returned from Android with a list of app shortcuts.
  void OnGetAppShortcutItems(
      std::vector<mojom::AppShortcutItemPtr> shortcut_items);

  // Bound by a barrier closure to wait all icon decode requests done.
  void OnAllIconDecodeRequestsDone();

  // Bound by each icon decode request.
  void OnSingleIconDecodeRequestDone(size_t index, const gfx::ImageSkia& icon);

  GetAppShortcutItemsCallback callback_;

  // Caches the app shortcut items to be sent to |callback_| when they are
  // ready.
  std::unique_ptr<ArcAppShortcutItems> items_;

  // A barrier closure to be run when all pending icon decode requests are done.
  base::RepeatingClosure barrier_closure_;

  // Icon decode request for each item.
  std::vector<std::unique_ptr<IconDecodeRequest>> icon_decode_requests_;

  base::WeakPtrFactory<ArcAppShortcutsRequest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcAppShortcutsRequest);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_APP_SHORTCUTS_ARC_APP_SHORTCUTS_REQUEST_H_
