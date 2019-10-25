// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_THIN_WEBVIEW_INTERNAL_COMPOSITOR_VIEW_IMPL_H_
#define CHROME_BROWSER_ANDROID_THIN_WEBVIEW_INTERNAL_COMPOSITOR_VIEW_IMPL_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "cc/layers/layer.h"
#include "chrome/browser/android/thin_webview/compositor_view.h"
#include "content/public/browser/android/compositor_client.h"

namespace cc {
class SolidColorLayer;
}  // namespace cc

namespace content {
class Compositor;
}  // namespace content

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace thin_webview {
namespace android {

// Native counterpart of CompositorViewImpl.java.
class CompositorViewImpl : public CompositorView,
                           public content::CompositorClient {
 public:
  CompositorViewImpl(JNIEnv* env,
                     jobject obj,
                     ui::WindowAndroid* window_android);
  ~CompositorViewImpl() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& object);

  void SetNeedsComposite(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& object);
  void SurfaceCreated(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& object);
  void SurfaceDestroyed(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& object);
  void SurfaceChanged(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& object,
                      jint format,
                      jint width,
                      jint height,
                      const base::android::JavaParamRef<jobject>& surface);

  // CompositorView implementation.
  void SetRootLayer(scoped_refptr<cc::Layer> layer) override;

  // CompositorClient implementation.
  void RecreateSurface() override;
  void UpdateLayerTreeHost() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> obj_;
  std::unique_ptr<content::Compositor> compositor_;
  scoped_refptr<cc::SolidColorLayer> root_layer_;

  int current_surface_format_;

  DISALLOW_COPY_AND_ASSIGN(CompositorViewImpl);
};

}  // namespace android
}  // namespace thin_webview

#endif  // CHROME_BROWSER_ANDROID_THIN_WEBVIEW_INTERNAL_COMPOSITOR_VIEW_IMPL_H_
