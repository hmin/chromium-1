// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_modality_controller.h"

#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ash {

namespace wm {

namespace {

bool TransientChildIsWindowModal(aura::Window* window) {
  return window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_WINDOW;
}

aura::Window* GetWindowModalTransientChild(aura::Window* window) {
  aura::Window::Windows::const_iterator it;
  for (it = window->transient_children().begin();
       it != window->transient_children().end();
       ++it) {
    if (TransientChildIsWindowModal(*it) && (*it)->IsVisible()) {
      if (!(*it)->transient_children().empty())
        return GetWindowModalTransientChild(*it);
      return *it;
    }
  }
  return NULL;
}

}  // namespace

aura::Window* GetWindowModalTransient(aura::Window* window) {
  if (!window)
    return NULL;

  // We always want to check the for the transient child of the activatable
  // window.
  window = wm::GetActivatableWindow(window);
  if (!window)
    return NULL;

  return GetWindowModalTransientChild(window);
}

}  // namespace wm

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// WindowModalityController, public:

WindowModalityController::WindowModalityController() {
}

WindowModalityController::~WindowModalityController() {
}

////////////////////////////////////////////////////////////////////////////////
// WindowModalityController, aura::EventFilter implementation:

bool WindowModalityController::PreHandleKeyEvent(aura::Window* target,
                                                 aura::KeyEvent* event) {
  return !!wm::GetWindowModalTransient(target);
}

bool WindowModalityController::PreHandleMouseEvent(aura::Window* target,
                                                   aura::MouseEvent* event) {
  aura::Window* modal_transient_child = wm::GetWindowModalTransient(target);
  if (modal_transient_child && event->type() == ui::ET_MOUSE_PRESSED)
    wm::ActivateWindow(modal_transient_child);
  return !!modal_transient_child;
}

ui::TouchStatus WindowModalityController::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  // TODO: make touch work with modals.
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus WindowModalityController::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  // TODO: make gestures work with modals.
  return ui::GESTURE_STATUS_UNKNOWN;
}

}  // namespace internal
}  // namespace ash
