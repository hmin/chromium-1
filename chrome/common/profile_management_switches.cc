// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profile_management_switches.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/chrome_switches.h"

namespace {

const char kNewProfileManagementFieldTrialName[] = "NewProfileManagement";

bool CheckProfileManagementFlag(std::string command_switch, bool active_state) {
  std::string trial_type =
      base::FieldTrialList::FindFullName(kNewProfileManagementFieldTrialName);
  if (!trial_type.empty()) {
    if (trial_type == "Enabled")
      return active_state;
    if (trial_type == "Disabled")
      return !active_state;
  }
  return CommandLine::ForCurrentProcess()->HasSwitch(command_switch);
}

}  // namespace

namespace switches {

bool IsEnableWebBasedSignin() {
  return CheckProfileManagementFlag(switches::kEnableWebBasedSignin, false);
}

bool IsGoogleProfileInfo() {
  return CheckProfileManagementFlag(switches::kGoogleProfileInfo, true);
}

bool IsNewProfileManagement() {
  return CheckProfileManagementFlag(switches::kNewProfileManagement, true);
}

}  // namespace switches
