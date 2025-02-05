// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/receiver/receiver.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace mixer_service {
namespace {

constexpr int kMaxAcceptLoop = 5;

std::string GetEndpoint() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string path =
      command_line->GetSwitchValueASCII(switches::kMixerServiceEndpoint);
  if (path.empty()) {
    return mixer_service::kDefaultUnixDomainSocketPath;
  }
  return path;
}

class LocalReceiverInstance {
 public:
  LocalReceiverInstance() = default;

  void SetInstance(Receiver* receiver) {
    base::AutoLock lock(lock_);
    receiver_ = receiver;
  }

  void RemoveInstance(Receiver* receiver) {
    base::AutoLock lock(lock_);
    if (receiver_ == receiver) {
      receiver_ = nullptr;
    }
  }

  std::unique_ptr<MixerSocket> CreateLocalSocket() {
    base::AutoLock lock(lock_);
    if (receiver_) {
      return receiver_->LocalConnect();
    }
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalReceiverInstance);

  base::Lock lock_;
  Receiver* receiver_ = nullptr;
};

LocalReceiverInstance* GetLocalReceiver() {
  static base::NoDestructor<LocalReceiverInstance> instance;
  return instance.get();
}

}  // namespace

std::unique_ptr<MixerSocket> CreateLocalMixerServiceConnection() {
  return GetLocalReceiver()->CreateLocalSocket();
}

class Receiver::InitialSocket : public MixerSocket::Delegate {
 public:
  InitialSocket(Receiver* receiver, std::unique_ptr<MixerSocket> socket)
      : receiver_(receiver), socket_(std::move(socket)) {
    DCHECK(receiver_);
    socket_->SetDelegate(this);
  }

  ~InitialSocket() override = default;

 private:
  // MixerSocket::Delegate implementation:
  bool HandleMetadata(const Generic& message) override {
    if (message.has_output_stream_params()) {
      receiver_->CreateOutputStream(std::move(socket_), message);
      receiver_->RemoveInitialSocket(this);
    } else if (message.has_loopback_request()) {
      receiver_->CreateLoopbackConnection(std::move(socket_), message);
      receiver_->RemoveInitialSocket(this);
    } else if (message.has_redirection_request()) {
      receiver_->CreateAudioRedirection(std::move(socket_), message);
      receiver_->RemoveInitialSocket(this);
    } else if (message.has_set_device_volume() ||
               message.has_set_device_muted() ||
               message.has_set_volume_limit() ||
               message.has_configure_postprocessor() ||
               message.has_reload_postprocessors() ||
               message.has_request_stream_count()) {
      receiver_->CreateControlConnection(std::move(socket_), message);
      receiver_->RemoveInitialSocket(this);
    }

    return true;
  }

  void OnConnectionError() override { receiver_->RemoveInitialSocket(this); }

  Receiver* const receiver_;
  std::unique_ptr<MixerSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(InitialSocket);
};

Receiver::Receiver()
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      socket_service_(
          GetEndpoint(),
          GetSwitchValueNonNegativeInt(switches::kMixerServiceEndpoint,
                                       mixer_service::kDefaultTcpPort),
          kMaxAcceptLoop,
          this),
      weak_factory_(this) {
  socket_service_.Accept();
  GetLocalReceiver()->SetInstance(this);
}

Receiver::~Receiver() {
  GetLocalReceiver()->RemoveInstance(this);
}

std::unique_ptr<MixerSocket> Receiver::LocalConnect() {
  std::unique_ptr<MixerSocket> receiver_socket(new MixerSocket);
  std::unique_ptr<MixerSocket> caller_socket(new MixerSocket);

  receiver_socket->SetLocalCounterpart(caller_socket->GetWeakPtr(),
                                       base::SequencedTaskRunnerHandle::Get());
  caller_socket->SetLocalCounterpart(receiver_socket->GetWeakPtr(),
                                     task_runner_);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Receiver::HandleLocalConnection,
                     weak_factory_.GetWeakPtr(), std::move(receiver_socket)));

  return caller_socket;
}

void Receiver::HandleAcceptedSocket(std::unique_ptr<net::StreamSocket> socket) {
  AddInitialSocket(std::make_unique<InitialSocket>(
      this, std::make_unique<MixerSocket>(std::move(socket))));
}

void Receiver::HandleLocalConnection(std::unique_ptr<MixerSocket> socket) {
  AddInitialSocket(std::make_unique<InitialSocket>(this, std::move(socket)));
}

void Receiver::AddInitialSocket(std::unique_ptr<InitialSocket> initial_socket) {
  InitialSocket* ptr = initial_socket.get();
  initial_sockets_[ptr] = std::move(initial_socket);
}

void Receiver::RemoveInitialSocket(InitialSocket* socket) {
  initial_sockets_.erase(socket);
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
