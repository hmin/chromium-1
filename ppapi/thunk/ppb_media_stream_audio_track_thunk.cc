// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_media_stream_audio_track.idl modified Mon Jan 13 11:50:41 2014.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_media_stream_audio_track.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_media_stream_audio_track_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsMediaStreamAudioTrack(PP_Resource resource) {
  VLOG(4) << "PPB_MediaStreamAudioTrack::IsMediaStreamAudioTrack()";
  EnterResource<PPB_MediaStreamAudioTrack_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Configure(PP_Resource audio_track,
                  uint32_t samples_per_frame,
                  uint32_t max_buffered_frames) {
  VLOG(4) << "PPB_MediaStreamAudioTrack::Configure()";
  EnterResource<PPB_MediaStreamAudioTrack_API> enter(audio_track, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->Configure(samples_per_frame, max_buffered_frames);
}

struct PP_Var GetId(PP_Resource audio_track) {
  VLOG(4) << "PPB_MediaStreamAudioTrack::GetId()";
  EnterResource<PPB_MediaStreamAudioTrack_API> enter(audio_track, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetId();
}

PP_Bool HasEnded(PP_Resource audio_track) {
  VLOG(4) << "PPB_MediaStreamAudioTrack::HasEnded()";
  EnterResource<PPB_MediaStreamAudioTrack_API> enter(audio_track, true);
  if (enter.failed())
    return PP_TRUE;
  return enter.object()->HasEnded();
}

int32_t GetFrame(PP_Resource audio_track,
                 PP_Resource* frame,
                 struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_MediaStreamAudioTrack::GetFrame()";
  EnterResource<PPB_MediaStreamAudioTrack_API> enter(audio_track,
                                                     callback,
                                                     true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetFrame(frame, enter.callback()));
}

int32_t RecycleFrame(PP_Resource audio_track, PP_Resource frame) {
  VLOG(4) << "PPB_MediaStreamAudioTrack::RecycleFrame()";
  EnterResource<PPB_MediaStreamAudioTrack_API> enter(audio_track, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->RecycleFrame(frame);
}

void Close(PP_Resource audio_track) {
  VLOG(4) << "PPB_MediaStreamAudioTrack::Close()";
  EnterResource<PPB_MediaStreamAudioTrack_API> enter(audio_track, true);
  if (enter.failed())
    return;
  enter.object()->Close();
}

const PPB_MediaStreamAudioTrack_0_1 g_ppb_mediastreamaudiotrack_thunk_0_1 = {
  &IsMediaStreamAudioTrack,
  &Configure,
  &GetId,
  &HasEnded,
  &GetFrame,
  &RecycleFrame,
  &Close
};

}  // namespace

const PPB_MediaStreamAudioTrack_0_1* GetPPB_MediaStreamAudioTrack_0_1_Thunk() {
  return &g_ppb_mediastreamaudiotrack_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
