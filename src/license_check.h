// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Checks license status. If valid, returns 1 immediately.
/// If not activated, shows a native GUI activation prompt.
/// Returns 1 if activated/unlocked, 0 if user cancelled or closed.
int license_check_prompt(void);

#ifdef __cplusplus
}
#endif

