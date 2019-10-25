// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_LAUNCH_CHROME_H_
#define CHROME_INSTALLER_SETUP_LAUNCH_CHROME_H_

#include <stdint.h>

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace installer {

// Launches Chrome without waiting for it to exit.
bool LaunchChromeBrowser(const base::FilePath& application_path);

// Launches Chrome with given command line, waits for Chrome indefinitely (until
// it terminates), and gets the process exit code if available. The function
// returns true as long as Chrome is successfully launched. The status of Chrome
// at the return of the function is given by exit_code.  NOTE: The 'options'
// CommandLine object should only contain parameters.  The program part will be
// ignored.
bool LaunchChromeAndWait(const base::FilePath& application_path,
                         const base::CommandLine& options,
                         int32_t* exit_code);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_LAUNCH_CHROME_H_
