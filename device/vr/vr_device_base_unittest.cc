// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/test/fake_vr_device.h"
#include "device/vr/vr_device_base.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class VRDeviceBaseForTesting : public VRDeviceBase {
 public:
  VRDeviceBaseForTesting() : VRDeviceBase(mojom::XRDeviceId::FAKE_DEVICE_ID) {}
  ~VRDeviceBaseForTesting() override = default;

  void SetVRDisplayInfoForTest(mojom::VRDisplayInfoPtr display_info) {
    SetVRDisplayInfo(std::move(display_info));
  }

  void FireDisplayActivate() {
    OnActivate(device::mojom::VRDisplayEventReason::MOUNTED, base::DoNothing());
  }

  bool ListeningForActivate() { return listening_for_activate; }

  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override {}

 private:
  void OnListeningForActivate(bool listening) override {
    listening_for_activate = listening;
  }

  bool listening_for_activate = false;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceBaseForTesting);
};

class StubVRDeviceEventListener : public mojom::XRRuntimeEventListener {
 public:
  StubVRDeviceEventListener() = default;
  ~StubVRDeviceEventListener() override = default;

  MOCK_METHOD1(DoOnChanged, void(mojom::VRDisplayInfo* vr_device_info));
  void OnDisplayInfoChanged(mojom::VRDisplayInfoPtr vr_device_info) override {
    DoOnChanged(vr_device_info.get());
  }

  MOCK_METHOD2(DoOnDeviceActivated,
               void(mojom::VRDisplayEventReason,
                    base::OnceCallback<void(bool)>));
  void OnDeviceActivated(mojom::VRDisplayEventReason reason,
                         base::OnceCallback<void(bool)> callback) override {
    DoOnDeviceActivated(reason, base::DoNothing());
    // For now keep the test simple, and just call the callback:
    std::move(callback).Run(true);
  }

  MOCK_METHOD0(OnExitPresent, void());
  MOCK_METHOD1(OnVisibilityStateChanged, void(mojom::XRVisibilityState));
  MOCK_METHOD1(OnDeviceIdle, void(mojom::VRDisplayEventReason));
  MOCK_METHOD0(OnInitialized, void());

  mojo::PendingAssociatedRemote<mojom::XRRuntimeEventListener>
  BindPendingRemote() {
    return receiver_.BindNewEndpointAndPassRemote();
  }

  mojo::AssociatedReceiver<mojom::XRRuntimeEventListener> receiver_{this};
};

}  // namespace

class VRDeviceTest : public testing::Test {
 public:
  VRDeviceTest() {}
  ~VRDeviceTest() override {}

 protected:
  std::unique_ptr<VRDeviceBaseForTesting> MakeVRDevice() {
    std::unique_ptr<VRDeviceBaseForTesting> device =
        std::make_unique<VRDeviceBaseForTesting>();
    device->SetVRDisplayInfoForTest(MakeVRDisplayInfo(device->GetId()));
    return device;
  }

  mojom::VRDisplayInfoPtr MakeVRDisplayInfo(mojom::XRDeviceId device_id) {
    mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
    display_info->id = device_id;
    display_info->capabilities = mojom::VRDisplayCapabilities::New();
    return display_info;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceTest);
};

// Tests VRDevice class default behaviour when it dispatches "vrdevicechanged"
// event. The expected behaviour is all of the services related with this device
// will receive the "vrdevicechanged" event.
TEST_F(VRDeviceTest, DeviceChangedDispatched) {
  auto device = MakeVRDevice();
  mojo::Remote<mojom::XRRuntime> device_remote(device->BindXRRuntime());
  StubVRDeviceEventListener listener;
  device_remote->ListenToDeviceChanges(
      listener.BindPendingRemote(),
      base::DoNothing());  // TODO: consider getting initial info
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(listener, DoOnChanged(testing::_)).Times(1);
  device->SetVRDisplayInfoForTest(MakeVRDisplayInfo(device->GetId()));
  base::RunLoop().RunUntilIdle();
}

TEST_F(VRDeviceTest, DisplayActivateRegsitered) {
  device::mojom::VRDisplayEventReason mounted =
      device::mojom::VRDisplayEventReason::MOUNTED;
  auto device = MakeVRDevice();
  mojo::Remote<mojom::XRRuntime> device_remote(device->BindXRRuntime());
  StubVRDeviceEventListener listener;
  device_remote->ListenToDeviceChanges(
      listener.BindPendingRemote(),
      base::DoNothing());  // TODO: consider getting initial data
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(device->ListeningForActivate());
  device->SetListeningForActivate(true);
  EXPECT_TRUE(device->ListeningForActivate());

  EXPECT_CALL(listener, DoOnDeviceActivated(mounted, testing::_)).Times(1);
  device->FireDisplayActivate();
  base::RunLoop().RunUntilIdle();
}

}  // namespace device
