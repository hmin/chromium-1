// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message.h"
#include "ppapi/proxy/nacl_message_scanner.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {
namespace proxy {

namespace {
const PP_Resource kInvalidResource = 0;
const PP_Resource kFileSystem = 1;
const PP_Resource kFileIO = 2;
const int64_t kQuotaReservationAmount = 100;
}

class NaClMessageScannerTest : public PluginProxyTest {
 public:
  NaClMessageScannerTest() {}

  uint32 FindPendingSyncMessage(
      const NaClMessageScanner& scanner,
      const IPC::Message& msg) {
    int msg_id = IPC::SyncMessage::GetMessageId(msg);
    std::map<int, uint32>::const_iterator it =
        scanner.pending_sync_msgs_.find(msg_id);
    // O can signal that no message was found.
    return (it != scanner.pending_sync_msgs_.end()) ? it->second : 0;
  }

  NaClMessageScanner::FileSystem* FindFileSystem(
      const NaClMessageScanner& scanner,
      PP_Resource file_system) {
    NaClMessageScanner::FileSystemMap::const_iterator it =
        scanner.file_systems_.find(file_system);
    return (it != scanner.file_systems_.end()) ? it->second : NULL;
  }

  NaClMessageScanner::FileIO* FindFileIO(
      const NaClMessageScanner& scanner,
      PP_Resource file_io) {
    NaClMessageScanner::FileIOMap::const_iterator it =
        scanner.files_.find(file_io);
    return (it != scanner.files_.end()) ? it->second : NULL;
  }

  void OpenQuotaFile(NaClMessageScanner* scanner,
                     PP_Resource file_io,
                     PP_Resource file_system) {
    std::vector<SerializedHandle> unused_handles;
    ResourceMessageReplyParams fio_reply_params(file_io, 0);
    scoped_ptr<IPC::Message> new_msg_ptr;
    scanner->ScanMessage(
      PpapiPluginMsg_ResourceReply(
          fio_reply_params,
          PpapiPluginMsg_FileIO_OpenReply(file_system, 0)),
      &unused_handles,
      &new_msg_ptr);
    EXPECT_FALSE(new_msg_ptr);
  }
};

TEST_F(NaClMessageScannerTest, SyncMessageAndReply) {
  NaClMessageScanner test;
  ppapi::proxy::SerializedHandle handle(
      ppapi::proxy::SerializedHandle::SHARED_MEMORY);
  IPC::Message msg =
      PpapiHostMsg_PPBGraphics3D_GetTransferBuffer(
          ppapi::API_ID_PPB_GRAPHICS_3D,
          HostResource(),
          0,  // id
          &handle);
  scoped_ptr<IPC::Message> new_msg_ptr;
  EXPECT_NE(msg.type(), FindPendingSyncMessage(test, msg));
  test.ScanUntrustedMessage(msg, &new_msg_ptr);
  EXPECT_FALSE(new_msg_ptr);
  EXPECT_EQ(msg.type(), FindPendingSyncMessage(test, msg));

  // TODO(bbudge) Figure out how to put together a sync reply message.
}

TEST_F(NaClMessageScannerTest, FileOpenClose) {
  NaClMessageScanner test;
  std::vector<SerializedHandle> unused_handles;
  ResourceMessageCallParams fio_call_params(kFileIO, 0);
  ResourceMessageCallParams fs_call_params(kFileSystem, 0);
  ResourceMessageReplyParams fio_reply_params(kFileIO, 0);
  ResourceMessageReplyParams fs_reply_params(kFileSystem, 0);
  scoped_ptr<IPC::Message> new_msg_ptr;

  EXPECT_EQ(NULL, FindFileSystem(test, kFileSystem));
  EXPECT_EQ(NULL, FindFileIO(test, kFileIO));

  // Open a file, not in a quota file system.
  test.ScanMessage(
      PpapiPluginMsg_ResourceReply(
          fio_reply_params,
          PpapiPluginMsg_FileIO_OpenReply(kInvalidResource, 0)),
      &unused_handles,
      &new_msg_ptr);
  EXPECT_FALSE(new_msg_ptr);
  EXPECT_FALSE(FindFileSystem(test, kFileSystem));
  EXPECT_FALSE(FindFileIO(test, kFileIO));

  // Open a file in a quota file system; info objects for it and its file system
  // should be created.
  OpenQuotaFile(&test, kFileIO, kFileSystem);
  NaClMessageScanner::FileSystem* fs = FindFileSystem(test, kFileSystem);
  NaClMessageScanner::FileIO* fio = FindFileIO(test, kFileIO);
  EXPECT_TRUE(fs);
  EXPECT_EQ(0, fs->reserved_quota());
  EXPECT_TRUE(fio);
  EXPECT_EQ(0, fio->max_written_offset());

  const int64_t kNewFileSize = 10;
  fio->SetMaxWrittenOffset(kNewFileSize);

  // We should not be able to under-report max_written_offset when closing.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fio_call_params,
          PpapiHostMsg_FileIO_Close(0)),
      &new_msg_ptr);
  EXPECT_TRUE(new_msg_ptr);
  ResourceMessageCallParams call_params;
  IPC::Message nested_msg;
  int64_t max_written_offset = 0;
  EXPECT_TRUE(UnpackMessage<PpapiHostMsg_ResourceCall>(
                  *new_msg_ptr, &call_params, &nested_msg) &&
              UnpackMessage<PpapiHostMsg_FileIO_Close>(
                  nested_msg, &max_written_offset));
  new_msg_ptr.reset();
  EXPECT_EQ(kNewFileSize, max_written_offset);
  EXPECT_FALSE(FindFileIO(test, kFileIO));

  // Reopen the file.
  OpenQuotaFile(&test, kFileIO, kFileSystem);
  fio = FindFileIO(test, kFileIO);
  fio->SetMaxWrittenOffset(kNewFileSize);

  // Close with correct max_written_offset.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fio_call_params,
          PpapiHostMsg_FileIO_Close(kNewFileSize)),
      &new_msg_ptr);
  EXPECT_FALSE(new_msg_ptr);
  EXPECT_FALSE(FindFileIO(test, kFileIO));

  // Destroy file system.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fs_call_params,
          PpapiHostMsg_ResourceDestroyed(kFileSystem)),
      &new_msg_ptr);
  EXPECT_FALSE(FindFileSystem(test, kFileSystem));
}

TEST_F(NaClMessageScannerTest, QuotaAuditing) {
  NaClMessageScanner test;
  std::vector<SerializedHandle> unused_handles;
  ResourceMessageCallParams fio_call_params(kFileIO, 0);
  ResourceMessageCallParams fs_call_params(kFileSystem, 0);
  ResourceMessageReplyParams fio_reply_params(kFileIO, 0);
  ResourceMessageReplyParams fs_reply_params(kFileSystem, 0);
  scoped_ptr<IPC::Message> new_msg_ptr;

  OpenQuotaFile(&test, kFileIO, kFileSystem);
  NaClMessageScanner::FileSystem* fs = FindFileSystem(test, kFileSystem);
  NaClMessageScanner::FileIO* fio = FindFileIO(test, kFileIO);
  EXPECT_TRUE(fs);
  EXPECT_EQ(0, fs->reserved_quota());
  EXPECT_TRUE(fio);
  EXPECT_EQ(0, fio->max_written_offset());

  // Without reserving quota, we should not be able to grow the file.
  EXPECT_FALSE(fio->Grow(1));
  EXPECT_EQ(0, fs->reserved_quota());
  EXPECT_EQ(0, fio->max_written_offset());

  // Receive reserved quota, and updated file sizes.
  const int64_t kNewFileSize = 10;
  FileOffsetMap offset_map;
  offset_map.insert(std::make_pair(kFileIO, kNewFileSize));
  test.ScanMessage(
      PpapiPluginMsg_ResourceReply(
          fs_reply_params,
          PpapiPluginMsg_FileSystem_ReserveQuotaReply(
              kQuotaReservationAmount,
              offset_map)),
      &unused_handles,
      &new_msg_ptr);
  EXPECT_FALSE(new_msg_ptr);
  EXPECT_EQ(kQuotaReservationAmount, fs->reserved_quota());
  EXPECT_EQ(kNewFileSize, fio->max_written_offset());

  // We should be able to grow the file within quota.
  EXPECT_TRUE(fio->Grow(1));
  EXPECT_EQ(kQuotaReservationAmount - 1, fs->reserved_quota());
  EXPECT_EQ(kNewFileSize + 1, fio->max_written_offset());

  // We should not be able to grow the file over quota.
  EXPECT_FALSE(fio->Grow(kQuotaReservationAmount));
  EXPECT_EQ(kQuotaReservationAmount - 1, fs->reserved_quota());
  EXPECT_EQ(kNewFileSize + 1, fio->max_written_offset());

  // Plugin should not under-report max written offsets when reserving quota.
  offset_map[kFileIO] = 0;  // should be kNewFileSize + 1.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fio_call_params,
          PpapiHostMsg_FileSystem_ReserveQuota(
              kQuotaReservationAmount,
              offset_map)),
      &new_msg_ptr);
  EXPECT_TRUE(new_msg_ptr);
  ResourceMessageCallParams call_params;
  IPC::Message nested_msg;
  int64_t amount = 0;
  FileOffsetMap new_offset_map;
  EXPECT_TRUE(UnpackMessage<PpapiHostMsg_ResourceCall>(
                  *new_msg_ptr, &call_params, &nested_msg) &&
              UnpackMessage<PpapiHostMsg_FileSystem_ReserveQuota>(
                  nested_msg, &amount, &new_offset_map));
  new_msg_ptr.reset();
  EXPECT_EQ(kQuotaReservationAmount, amount);
  EXPECT_EQ(kNewFileSize + 1, new_offset_map[kFileIO]);
}

TEST_F(NaClMessageScannerTest, SetLength) {
  NaClMessageScanner test;
  std::vector<SerializedHandle> unused_handles;
  ResourceMessageCallParams fio_call_params(kFileIO, 0);
  ResourceMessageCallParams fs_call_params(kFileSystem, 0);
  ResourceMessageReplyParams fio_reply_params(kFileIO, 0);
  ResourceMessageReplyParams fs_reply_params(kFileSystem, 0);
  scoped_ptr<IPC::Message> new_msg_ptr;

  OpenQuotaFile(&test, kFileIO, kFileSystem);
  NaClMessageScanner::FileSystem* fs = FindFileSystem(test, kFileSystem);
  NaClMessageScanner::FileIO* fio = FindFileIO(test, kFileIO);

  // Receive reserved quota, and updated file sizes.
  const int64_t kNewFileSize = 10;
  FileOffsetMap offset_map;
  offset_map.insert(std::make_pair(kFileIO, 0));
  test.ScanMessage(
      PpapiPluginMsg_ResourceReply(
          fs_reply_params,
          PpapiPluginMsg_FileSystem_ReserveQuotaReply(
              kQuotaReservationAmount,
              offset_map)),
      &unused_handles,
      &new_msg_ptr);

  // We should be able to SetLength within quota.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fio_call_params,
          PpapiHostMsg_FileIO_SetLength(kNewFileSize)),
      &new_msg_ptr);
  EXPECT_FALSE(new_msg_ptr);
  EXPECT_EQ(kQuotaReservationAmount - kNewFileSize, fs->reserved_quota());
  EXPECT_EQ(kNewFileSize, fio->max_written_offset());

  // We shouldn't be able to SetLength beyond quota. The message should be
  // rewritten to fail with length == -1.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fio_call_params,
          PpapiHostMsg_FileIO_SetLength(kQuotaReservationAmount + 1)),
      &new_msg_ptr);
  EXPECT_TRUE(new_msg_ptr);
  ResourceMessageCallParams call_params;
  IPC::Message nested_msg;
  int64_t length = 0;
  EXPECT_TRUE(UnpackMessage<PpapiHostMsg_ResourceCall>(
                  *new_msg_ptr, &call_params, &nested_msg) &&
              UnpackMessage<PpapiHostMsg_FileIO_SetLength>(
                  nested_msg, &length));
  new_msg_ptr.reset();
  EXPECT_EQ(-1, length);
  EXPECT_EQ(kQuotaReservationAmount - kNewFileSize, fs->reserved_quota());
  EXPECT_EQ(kNewFileSize, fio->max_written_offset());
}

}  // namespace proxy
}  // namespace ppapi
