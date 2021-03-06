// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_MASKED_WINDOW_TARGETER_H_
#define UI_WM_PUBLIC_MASKED_WINDOW_TARGETER_H_

#include "ui/aura/window_targeter.h"

namespace gfx {
class Path;
}

namespace wm {

class MaskedWindowTargeter : public aura::WindowTargeter {
 public:
  explicit MaskedWindowTargeter(aura::Window* masked_window);
  virtual ~MaskedWindowTargeter();

 protected:
  // Sets the hit-test mask for |window| in |mask| (in |window|'s local
  // coordinate system).
  virtual void GetHitTestMask(aura::Window* window, gfx::Path* mask) const = 0;

  // aura::WindowTargeter:
  virtual bool EventLocationInsideBounds(
      aura::Window* window,
      const ui::LocatedEvent& event) const OVERRIDE;

  aura::Window* masked_window_;

  DISALLOW_COPY_AND_ASSIGN(MaskedWindowTargeter);
};

}  // namespace wm

#endif  // UI_WM_PUBLIC_MASKED_WINDOW_TARGETER_H_
