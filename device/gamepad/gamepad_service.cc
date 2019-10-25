// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_service.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/gamepad/gamepad_consumer.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_data_fetcher_manager.h"
#include "device/gamepad/gamepad_provider.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

namespace {
GamepadService* g_gamepad_service = nullptr;
}  // namespace

GamepadService::GamepadService()
    : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  SetInstance(this);
}

GamepadService::GamepadService(std::unique_ptr<GamepadDataFetcher> fetcher)
    : provider_(std::make_unique<GamepadProvider>(
          /*connection_change_client=*/this,
          /*service_manager_connector=*/nullptr,
          std::move(fetcher),
          /*polling_thread=*/nullptr)),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  SetInstance(this);
}

GamepadService::~GamepadService() = default;

void GamepadService::SetInstance(GamepadService* instance) {
  // Unit tests can create multiple instances but only one should exist at any
  // given time so |g_gamepad_service| should only go from nullptr to
  // non-nullptr and vice versa.
  CHECK(!!instance != !!g_gamepad_service);
  if (g_gamepad_service)
    delete g_gamepad_service;
  g_gamepad_service = instance;
}

GamepadService* GamepadService::GetInstance() {
  if (!g_gamepad_service)
    g_gamepad_service = new GamepadService;
  return g_gamepad_service;
}

void GamepadService::StartUp(
    std::unique_ptr<service_manager::Connector> service_manager_connector) {
  if (!service_manager_connector_)
    service_manager_connector_ = std::move(service_manager_connector);

  // Ensures GamepadDataFetcherManager is created on UI thread. Otherwise,
  // GamepadPlatformDataFetcherLinux::Factory would be created with the
  // wrong thread for its |dbus_runner_|.
  GamepadDataFetcherManager::GetInstance();
}

bool GamepadService::ConsumerBecameActive(GamepadConsumer* consumer) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  if (!provider_) {
    provider_ = std::make_unique<GamepadProvider>(
        /*connection_change_client=*/this, service_manager_connector_->Clone());
  }

  std::pair<ConsumerSet::iterator, bool> insert_result =
      consumers_.insert(consumer);
  const ConsumerInfo& info = *insert_result.first;
  if (info.is_active)
    return false;
  info.is_active = true;
  if (info.did_observe_user_gesture) {
    auto consumer_state_it = inactive_consumer_state_.find(consumer);
    if (consumer_state_it != inactive_consumer_state_.end()) {
      const std::vector<bool>& old_connected_state = consumer_state_it->second;
      Gamepads gamepads;
      provider_->GetCurrentGamepadData(&gamepads);
      for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
        const Gamepad& gamepad = gamepads.items[i];
        if (gamepad.connected) {
          info.consumer->OnGamepadConnected(i, gamepad);
        } else if (old_connected_state[i] && !gamepad.connected) {
          info.consumer->OnGamepadDisconnected(i, gamepad);
        }
      }
      inactive_consumer_state_.erase(consumer_state_it);
    }
  } else if (!gesture_callback_pending_) {
    gesture_callback_pending_ = true;
    provider_->RegisterForUserGesture(
        base::Bind(&GamepadService::OnUserGesture, base::Unretained(this)));
  }

  if (num_active_consumers_++ == 0)
    provider_->Resume();
  return true;
}

bool GamepadService::ConsumerBecameInactive(GamepadConsumer* consumer) {
  DCHECK(provider_);
  auto consumer_it = consumers_.find(consumer);
  if (consumer_it == consumers_.end())
    return false;
  const ConsumerInfo& info = *consumer_it;
  if (!info.is_active)
    return false;
  DCHECK_GT(num_active_consumers_, 0);

  info.is_active = false;
  if (--num_active_consumers_ == 0)
    provider_->Pause();

  // Save the current state of connected gamepads.
  if (info.did_observe_user_gesture) {
    Gamepads gamepads;
    provider_->GetCurrentGamepadData(&gamepads);
    std::vector<bool> connected_state(Gamepads::kItemsLengthCap);
    for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i)
      connected_state[i] = gamepads.items[i].connected;
    inactive_consumer_state_[consumer] = connected_state;
  }
  return true;
}

bool GamepadService::RemoveConsumer(GamepadConsumer* consumer) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  auto it = consumers_.find(consumer);
  if (it == consumers_.end())
    return false;
  if (it->is_active && --num_active_consumers_ == 0)
    provider_->Pause();
  DCHECK_GE(num_active_consumers_, 0);
  consumers_.erase(it);
  inactive_consumer_state_.erase(consumer);
  return true;
}

void GamepadService::RegisterForUserGesture(const base::Closure& closure) {
  DCHECK(consumers_.size() > 0);
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  provider_->RegisterForUserGesture(closure);
}

void GamepadService::Terminate() {
  provider_.reset();
}

void GamepadService::OnGamepadConnectionChange(bool connected,
                                               uint32_t index,
                                               const Gamepad& pad) {
  if (connected) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GamepadService::OnGamepadConnected,
                                  base::Unretained(this), index, pad));
  } else {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GamepadService::OnGamepadDisconnected,
                                  base::Unretained(this), index, pad));
  }
}

void GamepadService::OnGamepadConnected(uint32_t index, const Gamepad& pad) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  for (auto it = consumers_.begin(); it != consumers_.end(); ++it) {
    if (it->did_observe_user_gesture && it->is_active)
      it->consumer->OnGamepadConnected(index, pad);
  }
}

void GamepadService::OnGamepadDisconnected(uint32_t index, const Gamepad& pad) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  for (auto it = consumers_.begin(); it != consumers_.end(); ++it) {
    if (it->did_observe_user_gesture && it->is_active)
      it->consumer->OnGamepadDisconnected(index, pad);
  }
}

void GamepadService::PlayVibrationEffectOnce(
    uint32_t pad_index,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
  if (!provider_) {
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  provider_->PlayVibrationEffectOnce(pad_index, type, std::move(params),
                                     std::move(callback));
}

void GamepadService::ResetVibrationActuator(
    uint32_t pad_index,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
  if (!provider_) {
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  provider_->ResetVibrationActuator(pad_index, std::move(callback));
}

base::ReadOnlySharedMemoryRegion GamepadService::DuplicateSharedMemoryRegion() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  return provider_->DuplicateSharedMemoryRegion();
}

void GamepadService::OnUserGesture() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  gesture_callback_pending_ = false;

  if (!provider_ || num_active_consumers_ == 0)
    return;

  for (auto it = consumers_.begin(); it != consumers_.end(); ++it) {
    if (!it->did_observe_user_gesture && it->is_active) {
      const ConsumerInfo& info = *it;
      info.did_observe_user_gesture = true;
      Gamepads gamepads;
      provider_->GetCurrentGamepadData(&gamepads);
      for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
        const Gamepad& pad = gamepads.items[i];
        if (pad.connected)
          info.consumer->OnGamepadConnected(i, pad);
      }
    }
  }
}

void GamepadService::SetSanitizationEnabled(bool sanitize) {
  provider_->SetSanitizationEnabled(sanitize);
}

}  // namespace device
