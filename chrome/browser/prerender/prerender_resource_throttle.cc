// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_resource_throttle.h"

#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"

namespace prerender {

namespace {
static const char kFollowOnlyWhenPrerenderShown[] =
    "follow-only-when-prerender-shown";

PrerenderContents* g_prerender_contents_for_testing;
}

void PrerenderResourceThrottle::OverridePrerenderContentsForTesting(
    PrerenderContents* contents) {
  g_prerender_contents_for_testing = contents;
}

PrerenderResourceThrottle::PrerenderResourceThrottle(
    net::URLRequest* request,
    PrerenderTracker* tracker)
    : request_(request),
      tracker_(tracker) {
}

void PrerenderResourceThrottle::WillStartRequest(bool* defer) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  int child_id = info->GetChildID();
  int route_id = info->GetRouteID();

  // If the prerender was used since the throttle was added, leave it
  // alone.
  if (!tracker_->IsPrerenderingOnIOThread(child_id, route_id))
    return;

  *defer = true;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&PrerenderResourceThrottle::WillStartRequestOnUI,
                 AsWeakPtr(), request_->method(), info->GetChildID(),
                 info->GetRenderFrameID(), request_->url()));
}

void PrerenderResourceThrottle::WillRedirectRequest(const GURL& new_url,
                                                    bool* defer) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  int child_id = info->GetChildID();
  int route_id = info->GetRouteID();

  // If the prerender was used since the throttle was added, leave it
  // alone.
  if (!tracker_->IsPrerenderingOnIOThread(child_id, route_id))
    return;

  *defer = true;
  std::string header;
  request_->GetResponseHeaderByName(kFollowOnlyWhenPrerenderShown, &header);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&PrerenderResourceThrottle::WillRedirectRequestOnUI,
                 AsWeakPtr(), header, info->GetResourceType(), info->IsAsync(),
                 info->GetChildID(), info->GetRenderFrameID(), new_url));
}

const char* PrerenderResourceThrottle::GetNameForLogging() const {
  return "PrerenderResourceThrottle";
}

void PrerenderResourceThrottle::Resume() {
  controller()->Resume();
}

void PrerenderResourceThrottle::Cancel() {
  controller()->Cancel();
}

void PrerenderResourceThrottle::WillStartRequestOnUI(
    const base::WeakPtr<PrerenderResourceThrottle>& throttle,
    const std::string& method,
    int render_process_id,
    int render_frame_id,
    const GURL& url) {
  bool cancel = false;
  PrerenderContents* prerender_contents =
      PrerenderContentsFromRenderFrame(render_process_id, render_frame_id);
  if (prerender_contents) {
    // Abort any prerenders that spawn requests that use unsupported HTTP
    // methods or schemes.
    if (!PrerenderManager::IsValidHttpMethod(method)) {
      prerender_contents->Destroy(FINAL_STATUS_INVALID_HTTP_METHOD);
      cancel = true;
    } else if (!PrerenderManager::DoesSubresourceURLHaveValidScheme(url)) {
      prerender_contents->Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
      ReportUnsupportedPrerenderScheme(url);
      cancel = true;
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(cancel ? &PrerenderResourceThrottle::Cancel :
                 &PrerenderResourceThrottle::Resume, throttle));
}

void PrerenderResourceThrottle::WillRedirectRequestOnUI(
    const base::WeakPtr<PrerenderResourceThrottle>& throttle,
    const std::string& follow_only_when_prerender_shown_header,
    ResourceType::Type resource_type,
    bool async,
    int render_process_id,
    int render_frame_id,
    const GURL& new_url) {
  bool cancel = false;
  PrerenderContents* prerender_contents =
      PrerenderContentsFromRenderFrame(render_process_id, render_frame_id);
  if (prerender_contents) {
    // Abort any prerenders with requests which redirect to invalid schemes.
    if (!PrerenderManager::DoesURLHaveValidScheme(new_url)) {
      prerender_contents->Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
      ReportUnsupportedPrerenderScheme(new_url);
      cancel = true;
    } else if (follow_only_when_prerender_shown_header == "1" &&
               resource_type != ResourceType::MAIN_FRAME) {
      // Only defer redirects with the Follow-Only-When-Prerender-Shown
      // header. Do not defer redirects on main frame loads.
      if (!async) {
        // Cancel on deferred synchronous requests. Those will
        // indefinitely hang up a renderer process.
        prerender_contents->Destroy(FINAL_STATUS_BAD_DEFERRED_REDIRECT);
        cancel = true;
      } else {
        // Defer the redirect until the prerender is used or
        // canceled. It is possible for the UI thread to used the
        // prerender at the same time. But then |tracker_| will resume
        // the request soon in
        // PrerenderTracker::RemovePrerenderOnIOThread.
        content::BrowserThread::PostTask(
            content::BrowserThread::IO,
            FROM_HERE,
            base::Bind(&PrerenderResourceThrottle::AddResourceThrottle,
                       throttle));
        return;
      }
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(cancel ? &PrerenderResourceThrottle::Cancel :
                 &PrerenderResourceThrottle::Resume, throttle));
}

PrerenderContents* PrerenderResourceThrottle::PrerenderContentsFromRenderFrame(
    int render_process_id, int render_frame_id) {
  if (g_prerender_contents_for_testing)
    return g_prerender_contents_for_testing;
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      render_process_id, render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  return PrerenderContents::FromWebContents(web_contents);
}

void PrerenderResourceThrottle::AddResourceThrottle() {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  int child_id = info->GetChildID();
  int route_id = info->GetRouteID();
  tracker_->AddResourceThrottleOnIOThread(child_id, route_id, AsWeakPtr());
}

}  // namespace prerender
