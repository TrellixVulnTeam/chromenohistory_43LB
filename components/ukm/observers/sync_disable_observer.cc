// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/sync_disable_observer.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "google_apis/gaia/google_service_auth_error.h"

using unified_consent::UrlKeyedDataCollectionConsentHelper;

namespace ukm {

namespace {

enum DisableInfo {
  DISABLED_BY_NONE,
  DISABLED_BY_HISTORY,
  DISABLED_BY_INITIALIZED,
  DISABLED_BY_HISTORY_INITIALIZED,
  DISABLED_BY_CONNECTED,
  DISABLED_BY_HISTORY_CONNECTED,
  DISABLED_BY_INITIALIZED_CONNECTED,
  DISABLED_BY_HISTORY_INITIALIZED_CONNECTED,
  DISABLED_BY_PASSPHRASE,
  DISABLED_BY_HISTORY_PASSPHRASE,
  DISABLED_BY_INITIALIZED_PASSPHRASE,
  DISABLED_BY_HISTORY_INITIALIZED_PASSPHRASE,
  DISABLED_BY_CONNECTED_PASSPHRASE,
  DISABLED_BY_HISTORY_CONNECTED_PASSPHRASE,
  DISABLED_BY_INITIALIZED_CONNECTED_PASSPHRASE,
  DISABLED_BY_HISTORY_INITIALIZED_CONNECTED_PASSPHRASE,
  DISABLED_BY_ANONYMIZED_DATA_COLLECTION,
  MAX_DISABLE_INFO
};

void RecordDisableInfo(DisableInfo info) {
  UMA_HISTOGRAM_ENUMERATION("UKM.SyncDisable.Info", info, MAX_DISABLE_INFO);
}

}  // namespace

SyncDisableObserver::SyncDisableObserver() : sync_observer_(this) {}

SyncDisableObserver::~SyncDisableObserver() {
  for (const auto& entry : consent_helpers_) {
    entry.second->RemoveObserver(this);
  }
}

bool SyncDisableObserver::SyncState::AllowsUkm() const {
  if (anonymized_data_collection_state == DataCollectionState::kIgnored)
    return history_enabled && initialized && connected && !passphrase_protected;
  else
    return anonymized_data_collection_state == DataCollectionState::kEnabled;
}

bool SyncDisableObserver::SyncState::AllowsUkmWithExtension() const {
  return AllowsUkm() && extensions_enabled && initialized && connected &&
         !passphrase_protected;
}

// static
SyncDisableObserver::SyncState SyncDisableObserver::GetSyncState(
    syncer::SyncService* sync_service,
    UrlKeyedDataCollectionConsentHelper* consent_helper) {
  SyncState state;

  // For the following two settings, we want them to match the state of a user
  // having history/extensions enabled in their Sync settings. Using it to track
  // active connections here is undesirable as changes in these states trigger
  // data purges.
  state.history_enabled = sync_service->GetPreferredDataTypes().Has(
                              syncer::HISTORY_DELETE_DIRECTIVES) &&
                          sync_service->IsSyncFeatureEnabled();
  state.extensions_enabled =
      sync_service->GetPreferredDataTypes().Has(syncer::EXTENSIONS) &&
      sync_service->IsSyncFeatureEnabled();

  state.initialized = sync_service->IsEngineInitialized();

  // Reasoning for the individual checks:
  // - IsSyncFeatureActive() makes sure Sync is enabled and initialized.
  // - HasCompletedSyncCycle() makes sure Sync has actually talked to the
  //   server. Without this, it's possible that there is some auth issue that we
  //   just haven't detected yet.
  // - Finally, GetAuthError() makes sure Sync is not in an auth error state. In
  //   particular, this includes the "Sync paused" state (which is implemented
  //   as an auth error).
  state.connected =
      (sync_service->IsSyncFeatureActive() &&
       sync_service->HasCompletedSyncCycle() &&
       sync_service->GetAuthError() == GoogleServiceAuthError::AuthErrorNone());

  state.passphrase_protected =
      state.initialized &&
      sync_service->GetUserSettings()->IsUsingSecondaryPassphrase();
  if (consent_helper) {
    state.anonymized_data_collection_state =
        consent_helper->IsEnabled() ? DataCollectionState::kEnabled
                                    : DataCollectionState::kDisabled;
  }
  return state;
}

void SyncDisableObserver::ObserveServiceForSyncDisables(
    syncer::SyncService* sync_service,
    PrefService* prefs) {
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> consent_helper;
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    consent_helper = UrlKeyedDataCollectionConsentHelper::
        NewAnonymizedDataCollectionConsentHelper(prefs, sync_service);
  }

  SyncState state = GetSyncState(sync_service, consent_helper.get());
  previous_states_[sync_service] = state;

  if (consent_helper) {
    consent_helper->AddObserver(this);
    consent_helpers_[sync_service] = std::move(consent_helper);
  }
  sync_observer_.Add(sync_service);
  UpdateAllProfileEnabled(false);
}

void SyncDisableObserver::UpdateAllProfileEnabled(bool must_purge) {
  bool all_sync_states_allow_ukm = CheckSyncStateOnAllProfiles();
  bool all_sync_states_allow_extension_ukm =
      all_sync_states_allow_ukm && CheckSyncStateForExtensionsOnAllProfiles();
  // Any change in sync settings needs to call OnSyncPrefsChanged so that the
  // new settings take effect.
  if (must_purge || (all_sync_states_allow_ukm != all_sync_states_allow_ukm_) ||
      (all_sync_states_allow_extension_ukm !=
       all_sync_states_allow_extension_ukm_)) {
    all_sync_states_allow_ukm_ = all_sync_states_allow_ukm;
    all_sync_states_allow_extension_ukm_ = all_sync_states_allow_extension_ukm;
    OnSyncPrefsChanged(must_purge);
  }
}

bool SyncDisableObserver::CheckSyncStateOnAllProfiles() {
  if (previous_states_.empty())
    return false;
  for (const auto& kv : previous_states_) {
    const SyncDisableObserver::SyncState& state = kv.second;

    if (state.AllowsUkm())
      continue;

    // Used as output for the disable reason UMA metrics.
    int disabled_by = 0;
    if (state.anonymized_data_collection_state ==
        DataCollectionState::kIgnored) {
      if (!state.history_enabled)
        disabled_by |= 1 << 0;
      if (!state.initialized)
        disabled_by |= 1 << 1;
      if (!state.connected)
        disabled_by |= 1 << 2;
      if (state.passphrase_protected)
        disabled_by |= 1 << 3;
    } else {
      DCHECK_EQ(DataCollectionState::kDisabled,
                state.anonymized_data_collection_state);
      disabled_by |= 1 << 4;
    }
    RecordDisableInfo(DisableInfo(disabled_by));
    return false;
  }
  RecordDisableInfo(DISABLED_BY_NONE);
  return true;
}

bool SyncDisableObserver::CheckSyncStateForExtensionsOnAllProfiles() {
  if (previous_states_.empty())
    return false;
  for (const auto& kv : previous_states_) {
    const SyncDisableObserver::SyncState& state = kv.second;
    if (!state.extensions_enabled)
      return false;
  }
  return true;
}

void SyncDisableObserver::OnStateChanged(syncer::SyncService* sync) {
  UrlKeyedDataCollectionConsentHelper* consent_helper = nullptr;
  auto found = consent_helpers_.find(sync);
  if (found != consent_helpers_.end())
    consent_helper = found->second.get();
  UpdateSyncState(sync, consent_helper);
}

void SyncDisableObserver::OnUrlKeyedDataCollectionConsentStateChanged(
    unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(consent_helper);
  syncer::SyncService* sync_service = nullptr;
  for (const auto& entry : consent_helpers_) {
    if (consent_helper == entry.second.get()) {
      sync_service = entry.first;
      break;
    }
  }
  DCHECK(sync_service);
  UpdateSyncState(sync_service, consent_helper);
}

void SyncDisableObserver::UpdateSyncState(
    syncer::SyncService* sync,
    UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(base::Contains(previous_states_, sync));
  const SyncDisableObserver::SyncState& previous_state = previous_states_[sync];
  DCHECK(previous_state.anonymized_data_collection_state ==
             DataCollectionState::kIgnored ||
         consent_helper);
  SyncDisableObserver::SyncState state = GetSyncState(sync, consent_helper);

  // Trigger a purge if the current state no longer allows UKM, ignoring
  // connection issues.
  bool must_purge;

  if (state.anonymized_data_collection_state != DataCollectionState::kIgnored) {
    // Verify that previous_state also uses unified consent.
    DCHECK_NE(DataCollectionState::kIgnored,
              previous_state.anonymized_data_collection_state);
    must_purge = previous_state.AllowsUkm() && !state.AllowsUkm();
  } else {
    // This differs from AllowsUkm() as it intentionally ignores connected
    // status.
    bool previous_state_allowed_ukm = previous_state.history_enabled &&
                                      previous_state.initialized &&
                                      !previous_state.passphrase_protected;
    bool current_state_allows_ukm = state.history_enabled &&
                                    state.initialized &&
                                    !state.passphrase_protected;
    must_purge = previous_state_allowed_ukm && !current_state_allows_ukm;
  }

  UMA_HISTOGRAM_BOOLEAN("UKM.SyncDisable.Purge", must_purge);

  previous_states_[sync] = state;
  UpdateAllProfileEnabled(must_purge);
}

void SyncDisableObserver::OnSyncShutdown(syncer::SyncService* sync) {
  DCHECK(base::Contains(previous_states_, sync));
  auto found = consent_helpers_.find(sync);
  if (found != consent_helpers_.end()) {
    found->second->RemoveObserver(this);
    consent_helpers_.erase(found);
  }
  sync_observer_.Remove(sync);
  previous_states_.erase(sync);
  UpdateAllProfileEnabled(false);
}

bool SyncDisableObserver::SyncStateAllowsUkm() {
  return all_sync_states_allow_ukm_;
}

bool SyncDisableObserver::SyncStateAllowsExtensionUkm() {
  return all_sync_states_allow_extension_ukm_;
}

}  // namespace ukm
