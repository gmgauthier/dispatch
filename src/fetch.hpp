/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gio/gio.h>

#include <string>

namespace dispatch {

/* Blocking GET. Call from a worker, not the UI thread. */
std::string http_get(const std::string& url, std::string& error, GCancellable* cancel = nullptr);

}  // namespace dispatch
