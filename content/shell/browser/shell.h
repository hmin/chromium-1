// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_SHELL_BROWSER_SHELL_H_
#define CONTENT_SHELL_BROWSER_SHELL_H_


#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ipc/ipc_channel.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkToolItem GtkToolItem;
#elif defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#elif defined(USE_AURA)
#if defined(OS_CHROMEOS)
namespace wm {
class WMTestHelper;
}
#endif  // defined(OS_CHROMEOS)
namespace views {
class Widget;
class ViewsDelegate;
}
#endif  // defined(USE_AURA)

class GURL;
namespace content {

#if defined(USE_AURA)
class ShellAuraPlatformData;
#endif

class BrowserContext;
class ShellDevToolsFrontend;
class ShellJavaScriptDialogManager;
class SiteInstance;
class WebContents;

// This represents one window of the Content Shell, i.e. all the UI including
// buttons and url bar, as well as the web content area.
class Shell : public WebContentsDelegate,
              public WebContentsObserver {
 public:
  static const int kDefaultTestWindowWidthDip;
  static const int kDefaultTestWindowHeightDip;

  virtual ~Shell();

  void LoadURL(const GURL& url);
  void LoadURLForFrame(const GURL& url, const std::string& frame_name);
  void GoBackOrForward(int offset);
  void Reload();
  void Stop();
  void UpdateNavigationControls();
  void Close();
  void ShowDevTools();
  void CloseDevTools();
#if defined(TOOLKIT_GTK) || defined(OS_MACOSX)
  // Resizes the main window to the given dimensions.
  void SizeTo(const gfx::Size& content_size);
#endif

  // Do one time initialization at application startup.
  static void Initialize();

  static Shell* CreateNewWindow(BrowserContext* browser_context,
                                const GURL& url,
                                SiteInstance* site_instance,
                                int routing_id,
                                const gfx::Size& initial_size);

  // Returns the Shell object corresponding to the given RenderViewHost.
  static Shell* FromRenderViewHost(RenderViewHost* rvh);

  // Returns the currently open windows.
  static std::vector<Shell*>& windows() { return windows_; }

  // Closes all windows and returns. This runs a message loop.
  static void CloseAllWindows();

  // Used for content_browsertests. Called once.
  static void SetShellCreatedCallback(
      base::Callback<void(Shell*)> shell_created_callback);

  WebContents* web_contents() const { return web_contents_.get(); }
  gfx::NativeWindow window() { return window_; }

#if defined(OS_MACOSX)
  // Public to be called by an ObjC bridge object.
  void ActionPerformed(int control);
  void URLEntered(std::string url_string);
#elif defined(OS_ANDROID)
  // Registers the Android Java to native methods.
  static bool Register(JNIEnv* env);
#endif

  // WebContentsDelegate
  virtual WebContents* OpenURLFromTab(WebContents* source,
                                      const OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(WebContents* source,
                              WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void LoadingStateChanged(WebContents* source) OVERRIDE;
#if defined(OS_ANDROID)
  virtual void LoadProgressChanged(WebContents* source,
                                   double progress) OVERRIDE;
#endif
  virtual void ToggleFullscreenModeForTab(WebContents* web_contents,
                                          bool enter_fullscreen) OVERRIDE;
  virtual bool IsFullscreenForTabOrPending(
      const WebContents* web_contents) const OVERRIDE;
  virtual void RequestToLockMouse(WebContents* web_contents,
                                  bool user_gesture,
                                  bool last_unlocked_by_target) OVERRIDE;
  virtual void CloseContents(WebContents* source) OVERRIDE;
  virtual bool CanOverscrollContent() const OVERRIDE;
  virtual void DidNavigateMainFramePostCommit(
      WebContents* web_contents) OVERRIDE;
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager() OVERRIDE;
#if defined(OS_MACOSX)
  virtual void HandleKeyboardEvent(
      WebContents* source,
      const NativeWebKeyboardEvent& event) OVERRIDE;
#endif
  virtual bool AddMessageToConsole(WebContents* source,
                                   int32 level,
                                   const base::string16& message,
                                   int32 line_no,
                                   const base::string16& source_id) OVERRIDE;
  virtual void RendererUnresponsive(WebContents* source) OVERRIDE;
  virtual void ActivateContents(WebContents* contents) OVERRIDE;
  virtual void DeactivateContents(WebContents* contents) OVERRIDE;
  virtual void WorkerCrashed(WebContents* source) OVERRIDE;

 private:
  enum UIControl {
    BACK_BUTTON,
    FORWARD_BUTTON,
    STOP_BUTTON
  };

  class DevToolsWebContentsObserver;

  explicit Shell(WebContents* web_contents);

  // Helper to create a new Shell given a newly created WebContents.
  static Shell* CreateShell(WebContents* web_contents,
                            const gfx::Size& initial_size);

  // Helper for one time initialization of application
  static void PlatformInitialize(const gfx::Size& default_window_size);
  // Helper for one time deinitialization of platform specific state.
  static void PlatformExit();

  // Adjust the size when Blink sends 0 for width and/or height.
  // This happens when Blink requests a default-sized window.
  static gfx::Size AdjustWindowSize(const gfx::Size& initial_size);

  // All the methods that begin with Platform need to be implemented by the
  // platform specific Shell implementation.
  // Called from the destructor to let each platform do any necessary cleanup.
  void PlatformCleanUp();
  // Creates the main window GUI.
  void PlatformCreateWindow(int width, int height);
  // Links the WebContents into the newly created window.
  void PlatformSetContents();
  // Resize the content area and GUI.
  void PlatformResizeSubViews();
  // Enable/disable a button.
  void PlatformEnableUIControl(UIControl control, bool is_enabled);
  // Updates the url in the url bar.
  void PlatformSetAddressBarURL(const GURL& url);
  // Sets whether the spinner is spinning.
  void PlatformSetIsLoading(bool loading);
  // Set the title of shell window
  void PlatformSetTitle(const base::string16& title);
#if defined(OS_ANDROID)
  void PlatformToggleFullscreenModeForTab(WebContents* web_contents,
                                          bool enter_fullscreen);
  bool PlatformIsFullscreenForTabOrPending(
      const WebContents* web_contents) const;
#endif

  gfx::NativeView GetContentView();

  // WebContentsObserver
  virtual void TitleWasSet(NavigationEntry* entry, bool explicit_set) OVERRIDE;

  void OnDevToolsWebContentsDestroyed();

#if defined(TOOLKIT_GTK)
  CHROMEGTK_CALLBACK_0(Shell, void, OnBackButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnForwardButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnReloadButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnStopButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnURLEntryActivate);
  CHROMEGTK_CALLBACK_0(Shell, gboolean, OnWindowDestroyed);

  CHROMEG_CALLBACK_3(Shell, gboolean, OnCloseWindowKeyPressed, GtkAccelGroup*,
                     GObject*, guint, GdkModifierType);
  CHROMEG_CALLBACK_3(Shell, gboolean, OnNewWindowKeyPressed, GtkAccelGroup*,
                     GObject*, guint, GdkModifierType);
  CHROMEG_CALLBACK_3(Shell, gboolean, OnHighlightURLView, GtkAccelGroup*,
                     GObject*, guint, GdkModifierType);
  CHROMEG_CALLBACK_3(Shell, gboolean, OnReloadKeyPressed, GtkAccelGroup*,
                     GObject*, guint, GdkModifierType);
#endif

  scoped_ptr<ShellJavaScriptDialogManager> dialog_manager_;

  scoped_ptr<WebContents> web_contents_;

  scoped_ptr<DevToolsWebContentsObserver> devtools_observer_;
  ShellDevToolsFrontend* devtools_frontend_;

  bool is_fullscreen_;

  gfx::NativeWindow window_;
  gfx::NativeEditView url_edit_view_;

  gfx::Size content_size_;

#if defined(TOOLKIT_GTK)
  GtkWidget* vbox_;

  GtkToolItem* back_button_;
  GtkToolItem* forward_button_;
  GtkToolItem* reload_button_;
  GtkToolItem* stop_button_;

  GtkWidget* spinner_;
  GtkToolItem* spinner_item_;

  int ui_elements_height_; // height of menubar, toolbar, etc.
#elif defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
#elif defined(USE_AURA)
#if defined(OS_CHROMEOS)
  static wm::WMTestHelper* wm_test_helper_;
#endif
#if defined(TOOLKIT_VIEWS)
  static views::ViewsDelegate* views_delegate_;

  views::Widget* window_widget_;
#else // defined(TOOLKIT_VIEWS)
  static ShellAuraPlatformData* platform_;
#endif // defined(TOOLKIT_VIEWS)
#endif

  bool headless_;

  // A container of all the open windows. We use a vector so we can keep track
  // of ordering.
  static std::vector<Shell*> windows_;

  static base::Callback<void(Shell*)> shell_created_callback_;

  // True if the destructur of Shell should post a quit closure on the current
  // message loop if the destructed Shell object was the last one.
  static bool quit_message_loop_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_H_
