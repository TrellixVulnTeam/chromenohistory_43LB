// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine_impl/loopback_server/loopback_server.h"
#include "components/sync/engine_impl/loopback_server/loopback_server_entity.h"
#include "components/sync/engine_impl/loopback_server/persistent_bookmark_entity.h"
#include "components/sync/engine_impl/loopback_server/persistent_tombstone_entity.h"
#include "components/sync/engine_impl/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/client_commands.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "net/http/http_status_code.h"

namespace fake_server {

// This function only compares one part of the markers, the time-independent
// hashes of the data served in the update. Apart from this, the progress
// markers for fake wallet data also include information about fetch time. This
// is in-line with the server behavior and -- as it keeps changing -- allows
// integration tests to wait for a GetUpdates call to finish, even if they don't
// contain data updates.
bool AreWalletDataProgressMarkersEquivalent(
    const sync_pb::DataTypeProgressMarker& marker1,
    const sync_pb::DataTypeProgressMarker& marker2);

// A fake version of the Sync server used for testing. This class is not thread
// safe.
class FakeServer : public syncer::LoopbackServer::ObserverForTests {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called after FakeServer has processed a successful commit. The types
    // updated as part of the commit are passed in |committed_model_types|.
    virtual void OnCommit(const std::string& committer_id,
                          syncer::ModelTypeSet committed_model_types) = 0;
  };

  FakeServer();
  // A directory will be created under |user_data_dir| to persist sync server
  // state. It's necessary for supporting PRE_ tests.
  explicit FakeServer(const base::FilePath& user_data_dir);
  ~FakeServer() override;

  // Handles a /command POST (with the given |request|) to the server.
  // |response| must not be null.
  net::HttpStatusCode HandleCommand(const std::string& request,
                                    std::string* response);

  // Helpers for fetching the last Commit or GetUpdates messages, respectively.
  // Returns true if the specified message existed, and false if no message has
  // been received.
  bool GetLastCommitMessage(sync_pb::ClientToServerMessage* message);
  bool GetLastGetUpdatesMessage(sync_pb::ClientToServerMessage* message);

  // Creates a DicionaryValue representation of all entities present in the
  // server. The dictionary keys are the strings generated by ModelTypeToString
  // and the values are ListValues containing StringValue versions of entity
  // names.
  std::unique_ptr<base::DictionaryValue> GetEntitiesAsDictionaryValue();

  // Returns all entities stored by the server of the given |model_type|.
  // This method returns SyncEntity protocol buffer objects (instead of
  // LoopbackServerEntity) so that callers can inspect datatype-specific data
  // (e.g., the URL of a session tab). Permanent entities are excluded.
  std::vector<sync_pb::SyncEntity> GetSyncEntitiesByModelType(
      syncer::ModelType model_type);

  // Returns all permanent entities stored by the server of the given
  // |model_type|. This method returns SyncEntity protocol buffer objects
  // (instead of LoopbackServerEntity) so that callers can inspect
  // datatype-specific data (e.g., the URL of a session tab).
  std::vector<sync_pb::SyncEntity> GetPermanentSyncEntitiesByModelType(
      syncer::ModelType model_type);

  // Returns an empty string if no top-level permanent item of the given type
  // was created.
  std::string GetTopLevelPermanentItemId(syncer::ModelType model_type);

  // Returns all keystore keys from the server.
  const std::vector<std::string>& GetKeystoreKeys() const;

  // Adds |entity| to the server's collection of entities. This method makes no
  // guarantees that the added entity will result in successful server
  // operations.
  void InjectEntity(std::unique_ptr<syncer::LoopbackServerEntity> entity);

  // Sets the Wallet card and address data to be served in following GetUpdates
  // requests (any further GetUpdates response will be empty, indicating no
  // change, if the client already has received |wallet_entities|).
  void SetWalletData(const std::vector<sync_pb::SyncEntity>& wallet_entities);

  // Modifies the entity on the server with the given |id|. The entity's
  // EntitySpecifics are replaced with |updated_specifics| and its version is
  // updated. If the given |id| does not exist or the ModelType of
  // |updated_specifics| does not match the entity, false is returned.
  // Otherwise, true is returned to represent a successful modification.
  //
  // This method sometimes updates entity data beyond EntitySpecifics. For
  // example, in the case of a bookmark, changing the BookmarkSpecifics title
  // field will modify the top-level entity's name field.
  bool ModifyEntitySpecifics(const std::string& id,
                             const sync_pb::EntitySpecifics& updated_specifics);

  bool ModifyBookmarkEntity(const std::string& id,
                            const std::string& parent_id,
                            const sync_pb::EntitySpecifics& updated_specifics);

  // Clears server data simulating a "dashboard stop and clear" and sets a new
  // store birthday.
  void ClearServerData();

  // Causes future calls to HandleCommand() fail with the given response code.
  void SetHttpError(net::HttpStatusCode http_status_code);

  // Undoes previous calls to SetHttpError().
  void ClearHttpError();

  // Sets the provided |client_command| in all subsequent successful requests.
  void SetClientCommand(const sync_pb::ClientCommand& client_command);

  // Force the server to return |error_type| in the error_code field of
  // ClientToServerResponse on all subsequent commit requests. If any of errors
  // triggerings currently configured it must be called only with
  // sync_pb::SyncEnums::SUCCESS.
  void TriggerCommitError(const sync_pb::SyncEnums::ErrorType& error_type);

  // Force the server to return |error_type| in the error_code field of
  // ClientToServerResponse on all subsequent sync requests. If any of errors
  // triggerings currently configured it must be called only with
  // sync_pb::SyncEnums::SUCCESS.
  void TriggerError(const sync_pb::SyncEnums::ErrorType& error_type);

  // Force the server to return the given data as part of the error field of
  // ClientToServerResponse on all subsequent sync requests. Must not be called
  // if any of errors triggerings currently configured.
  void TriggerActionableError(const sync_pb::SyncEnums::ErrorType& error_type,
                              const std::string& description,
                              const std::string& url,
                              const sync_pb::SyncEnums::Action& action);

  void ClearActionableError();

  // Instructs the server to send triggered errors on every other request
  // (starting with the first one after this call). This feature can be used to
  // test the resiliency of the client when communicating with a problematic
  // server or flaky network connection. This method should only be called
  // after a call to TriggerError or TriggerActionableError. Returns true if
  // triggered error alternating was successful.
  bool EnableAlternatingTriggeredErrors();

  // Adds |observer| to FakeServer's observer list. This should be called
  // before the Profile associated with |observer| is connected to the server.
  void AddObserver(Observer* observer);

  // Removes |observer| from the FakeServer's observer list. This method
  // must be called if AddObserver was ever called with |observer|.
  void RemoveObserver(Observer* observer);

  // Enables strong consistency model (i.e. server detects conflicts).
  void EnableStrongConsistencyWithConflictDetectionModel();

  // Sets a maximum batch size for GetUpdates requests.
  void SetMaxGetUpdatesBatchSize(int batch_size);

  // Sets the bag of chips returned by the server.
  void SetBagOfChips(const sync_pb::ChipBag& bag_of_chips);

  void TriggerMigrationDoneError(syncer::ModelTypeSet types);

  // Implement LoopbackServer::ObserverForTests:
  void OnCommit(const std::string& committer_id,
                syncer::ModelTypeSet committed_model_types) override;
  void OnHistoryCommit(const std::string& url) override;

  const std::set<std::string>& GetCommittedHistoryURLs() const;

  // Returns the current FakeServer as a WeakPtr.
  base::WeakPtr<FakeServer> AsWeakPtr();

  // Use this callback to generate response types for entities. They will still
  // be "committed" and stored as normal, this only affects the response type
  // the client sees. This allows tests to still inspect what the client has
  // done, although not as useful of a mechanism for multi client tests. Care
  // should be taken when failing responses, as the client will go into
  // exponential backoff, which can cause tests to be slow or time out.
  void OverrideResponseType(
      syncer::LoopbackServer::ResponseTypeProvider response_type_override);

 private:
  // Returns whether a triggered error should be sent for the request.
  bool ShouldSendTriggeredError() const;
  bool HasTriggeredError() const;
  net::HttpStatusCode SendToLoopbackServer(const std::string& request,
                                           std::string* response);
  void InjectClientCommand(std::string* response);
  void HandleWalletRequest(
      const sync_pb::ClientToServerMessage& request,
      const sync_pb::DataTypeProgressMarker& old_wallet_marker,
      std::string* response_string);

  // If set, the server will return HTTP errors.
  base::Optional<net::HttpStatusCode> http_error_status_code_;

  // All URLs received via history sync (powered by SESSIONS).
  std::set<std::string> committed_history_urls_;

  // Used as the error_code field of ClientToServerResponse on all commit
  // requests.
  sync_pb::SyncEnums::ErrorType commit_error_type_;

  // Used as the error_code field of ClientToServerResponse on all responses.
  sync_pb::SyncEnums::ErrorType error_type_;

  // Used as the error field of ClientToServerResponse when its pointer is not
  // null.
  std::unique_ptr<sync_pb::ClientToServerResponse_Error>
      triggered_actionable_error_;

  // These values are used in tandem to return a triggered error (either
  // |error_type_| or |triggered_actionable_error_|) on every other request.
  // |alternate_triggered_errors_| is set if this feature is enabled and
  // |request_counter_| is used to send triggered errors on odd-numbered
  // requests. Note that |request_counter_| can be reset and is not necessarily
  // indicative of the total number of requests handled during the object's
  // lifetime.
  bool alternate_triggered_errors_;
  int request_counter_;

  // Client command to be included in every response.
  sync_pb::ClientCommand client_command_;

  // FakeServer's observers.
  base::ObserverList<Observer, true>::Unchecked observers_;

  // The last received client to server messages.
  sync_pb::ClientToServerMessage last_commit_message_;
  sync_pb::ClientToServerMessage last_getupdates_message_;

  // Used to verify that FakeServer is only used from one thread.
  base::ThreadChecker thread_checker_;

  std::unique_ptr<syncer::LoopbackServer> loopback_server_;
  std::unique_ptr<base::ScopedTempDir> loopback_server_storage_;

  // The LoopbackServer does not know how to handle Wallet data properly, so
  // the FakeServer handles those itself.
  std::vector<sync_pb::SyncEntity> wallet_entities_;

  // Creates WeakPtr versions of the current FakeServer. This must be the last
  // data member!
  base::WeakPtrFactory<FakeServer> weak_ptr_factory_{this};
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_H_
