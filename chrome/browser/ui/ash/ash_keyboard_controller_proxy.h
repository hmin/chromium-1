// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_KEYBOARD_CONTROLLER_PROXY_H_
#define CHROME_BROWSER_UI_ASH_ASH_KEYBOARD_CONTROLLER_PROXY_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/keyboard/keyboard_controller_proxy.h"

class ExtensionFunctionDispatcher;

namespace content {
class WebContents;
}
namespace extensions {
class WindowController;
}
namespace gfx {
class Rect;
}
namespace ui {
class InputMethod;
}

// Subclass of KeyboardControllerProxy. It is used by KeyboardController to get
// access to the virtual keyboard window and setup Chrome extension functions.
class AshKeyboardControllerProxy
    : public keyboard::KeyboardControllerProxy,
      public content::WebContentsObserver,
      public ExtensionFunctionDispatcher::Delegate,
      public ui::LayerAnimationObserver {
 public:
  AshKeyboardControllerProxy();
  virtual ~AshKeyboardControllerProxy();

 private:
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  // keyboard::KeyboardControllerProxy overrides
  virtual content::BrowserContext* GetBrowserContext() OVERRIDE;
  virtual ui::InputMethod* GetInputMethod() OVERRIDE;
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual void SetupWebContents(content::WebContents* contents) OVERRIDE;
  virtual void ShowKeyboardContainer(aura::Window* container) OVERRIDE;
  virtual void HideKeyboardContainer(aura::Window* container) OVERRIDE;

  // The overridden implementation dispatches
  // chrome.virtualKeyboardPrivate.onTextInputBoxFocused event to extension to
  // provide the input type information. Naturally, when the virtual keyboard
  // extension is used as an IME then chrome.input.ime.onFocus provides the
  // information, but not when the virtual keyboard is used in conjunction with
  // another IME. virtualKeyboardPrivate.onTextInputBoxFocused is the remedy in
  // that case.
  virtual void SetUpdateInputType(ui::TextInputType type) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate overrides
  virtual extensions::WindowController* GetExtensionWindowController() const
      OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  // content::WebContentsObserver overrides
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // ui::LayerAnimationObserver overrides
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE {}

  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;
  // The keyboard container window for animation.
  aura::Window* animation_window_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardControllerProxy);
};

#endif  // CHROME_BROWSER_UI_ASH_ASH_KEYBOARD_CONTROLLER_PROXY_H_
