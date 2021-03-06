// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/api/string_ordinal.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/syncable_service.h"
#include "sync/protocol/app_list_specifics.pb.h"

class ExtensionAppModelBuilder;
class Profile;

namespace extensions {
class ExtensionSystem;
}

namespace sync_pb {
class AppListSpecifics;
}

namespace app_list {

class AppListModel;
class AppListItem;

// Keyed Service that owns, stores, and syncs an AppListModel for a profile.
class AppListSyncableService : public syncer::SyncableService,
                               public BrowserContextKeyedService,
                               public content::NotificationObserver {
 public:
  struct SyncItem {
    SyncItem(const std::string& id,
             sync_pb::AppListSpecifics::AppListItemType type);
    ~SyncItem();
    const std::string item_id;
    sync_pb::AppListSpecifics::AppListItemType item_type;
    std::string item_name;
    std::string parent_id;
    syncer::StringOrdinal page_ordinal;
    syncer::StringOrdinal item_ordinal;

    std::string ToString() const;
  };

  // Populates the model when |extension_system| is ready.
  AppListSyncableService(Profile* profile,
                         extensions::ExtensionSystem* extension_system);

  virtual ~AppListSyncableService();

  // Adds |item| to |sync_items_| and |model_|. Does nothing if a sync item
  // already exists.
  void AddItem(AppListItem* item);

  // Updates existing entry in |sync_items_| from |item|.
  void UpdateItem(AppListItem* item);

  // Removes sync item matching |id|.
  void RemoveItem(const std::string& id);

  // Returns the existing sync item matching |id| or NULL.
  const SyncItem* GetSyncItem(const std::string& id) const;

  Profile* profile() { return profile_; }
  AppListModel* model() { return model_.get(); }
  size_t GetNumSyncItemsForTest() { return sync_items_.size(); }

  // syncer::SyncableService
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

 private:
  typedef std::map<std::string, SyncItem*> SyncItemMap;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Builds the model once ExtensionService is ready.
  void BuildModel();

  // Returns true if sync has restarted, otherwise runs |flare_|.
  bool SyncStarted();

  // Creates or updates a SyncItem from |specifics|. Returns true if a new item
  // was created.
  bool ProcessSyncItem(const sync_pb::AppListSpecifics& specifics);

  // Handles a newly created sync item (e.g. creates a new Appitem and adds it
  // to the model or uninstalls a deleted default item.
  void ProcessNewSyncItem(SyncItem* sync_item);

  // Handles updating an existing sync item (e.g. updates item positions).
  void ProcessExistingSyncItem(SyncItem* sync_item);

  // Sends ADD or CHANGED for sync item.
  void SendSyncChange(SyncItem* sync_item,
                      syncer::SyncChange::SyncChangeType sync_change_type);

  // Returns an existing SyncItem corresponding to |item_id| or NULL.
  SyncItem* FindSyncItem(const std::string& item_id);

  // Creates a new sync item for |item_id|.
  SyncItem* CreateSyncItem(
      const std::string& item_id,
      sync_pb::AppListSpecifics::AppListItemType item_type);

  // Deletes a SyncItem matching |specifics|.
  void DeleteSyncItem(const sync_pb::AppListSpecifics& specifics);

  Profile* profile_;
  extensions::ExtensionSystem* extension_system_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<AppListModel> model_;
  scoped_ptr<ExtensionAppModelBuilder> apps_builder_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> sync_error_handler_;
  SyncItemMap sync_items_;
  syncer::SyncableService::StartSyncFlare flare_;

  DISALLOW_COPY_AND_ASSIGN(AppListSyncableService);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
