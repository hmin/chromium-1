// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/media_stream_audio_track.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_media_stream_audio_track.h"
#include "ppapi/cpp/audio_frame.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_MediaStreamAudioTrack_0_1>() {
  return PPB_MEDIASTREAMAUDIOTRACK_INTERFACE_0_1;
}

}  // namespace

MediaStreamAudioTrack::MediaStreamAudioTrack() {
}

MediaStreamAudioTrack::MediaStreamAudioTrack(
    const MediaStreamAudioTrack& other) : Resource(other) {
}

MediaStreamAudioTrack::MediaStreamAudioTrack(const Resource& resource)
    : Resource(resource) {
  PP_DCHECK(IsMediaStreamAudioTrack(resource));
}

MediaStreamAudioTrack::MediaStreamAudioTrack(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

MediaStreamAudioTrack::~MediaStreamAudioTrack() {
}

int32_t MediaStreamAudioTrack::Configure(uint32_t samples_per_frame,
                                         uint32_t frame_buffer_size) {
  if (has_interface<PPB_MediaStreamAudioTrack_0_1>()) {
    return get_interface<PPB_MediaStreamAudioTrack_0_1>()->Configure(
        pp_resource(), samples_per_frame, frame_buffer_size);
  }
  return PP_ERROR_NOINTERFACE;
}

std::string MediaStreamAudioTrack::GetId() const {
  if (has_interface<PPB_MediaStreamAudioTrack_0_1>()) {
    pp::Var id(PASS_REF, get_interface<PPB_MediaStreamAudioTrack_0_1>()->GetId(
        pp_resource()));
    if (id.is_string())
      return id.AsString();
  }
  return std::string();
}

bool MediaStreamAudioTrack::HasEnded() const {
  if (has_interface<PPB_MediaStreamAudioTrack_0_1>()) {
    return PP_ToBool(get_interface<PPB_MediaStreamAudioTrack_0_1>()->HasEnded(
        pp_resource()));
  }
  return true;
}

int32_t MediaStreamAudioTrack::GetFrame(
    const CompletionCallbackWithOutput<AudioFrame>& cc) {
  if (has_interface<PPB_MediaStreamAudioTrack_0_1>()) {
    return get_interface<PPB_MediaStreamAudioTrack_0_1>()->GetFrame(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t MediaStreamAudioTrack::RecycleFrame(const AudioFrame& frame) {
  if (has_interface<PPB_MediaStreamAudioTrack_0_1>()) {
    return get_interface<PPB_MediaStreamAudioTrack_0_1>()->RecycleFrame(
        pp_resource(), frame.pp_resource());
  }
  return PP_ERROR_NOINTERFACE;
}

void MediaStreamAudioTrack::Close() {
  if (has_interface<PPB_MediaStreamAudioTrack_0_1>())
    get_interface<PPB_MediaStreamAudioTrack_0_1>()->Close(pp_resource());
}

// static
bool MediaStreamAudioTrack::IsMediaStreamAudioTrack(const Resource& resource) {
  if (has_interface<PPB_MediaStreamAudioTrack_0_1>()) {
    return PP_ToBool(get_interface<PPB_MediaStreamAudioTrack_0_1>()->
        IsMediaStreamAudioTrack(resource.pp_resource()));
  }
  return false;
}

}  // namespace pp
