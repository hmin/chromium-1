// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_

#include <string>

namespace extensions {
class Extension;
class ExtensionSystem;
}

class ExtensionService;

namespace extension_util {

// Whether this extension can run in an incognito window.
bool IsIncognitoEnabled(const std::string& extension_id,
                        const ExtensionService* service);

// Will reload the extension since this permission is applied at loading time
// only.
void SetIsIncognitoEnabled(const std::string& extension_id,
                           ExtensionService* service,
                           bool enabled);

// Returns true if the given extension can see events and data from another
// sub-profile (incognito to original profile, or vice versa).
bool CanCrossIncognito(const extensions::Extension* extension,
                       const ExtensionService* service);

// Returns true if the given extension can be loaded in incognito.
bool CanLoadInIncognito(const extensions::Extension* extension,
                        const ExtensionService* service);

// Whether this extension can inject scripts into pages with file URLs.
bool AllowFileAccess(const extensions::Extension* extension,
                     const ExtensionService* service);

// Will reload the extension since this permission is applied at loading time
// only.
void SetAllowFileAccess(const extensions::Extension* extension,
                        ExtensionService* service,
                        bool allow);

// Whether an app can be launched or not. Apps may require enabling first,
// but they will still be launchable.
bool IsAppLaunchable(const std::string& extension_id,
                     const ExtensionService* service);

// Whether an app can be launched without being enabled first.
bool IsAppLaunchableWithoutEnabling(const std::string& extension_id,
                                    const ExtensionService* service);

// Whether an extension is idle and whether it is safe to perform actions
// such as updating.
bool IsExtensionIdle(const std::string& extension_id,
                     extensions::ExtensionSystem* extension_system);

// Whether an extension is installed permanently and not ephemerally.
bool IsExtensionInstalledPermanently(const std::string& extension_id,
                                     const ExtensionService* service);

// Returns true if the extension is an ephemeral app that is not currently
// running. Note that this function will always return false if |extension| is
// not an ephemeral app.
bool IsIdleEphemeralApp(const extensions::Extension* extension,
                        extensions::ExtensionSystem* extension_system);

}  // namespace extension_util

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_
