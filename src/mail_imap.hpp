/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <cstdint>
#include <string>

namespace dispatch {

struct ImapAccount {
  std::string host;
  uint16_t port = 143;
  bool starttls = true;
  std::string user;
  std::string password;
};

struct InboxSyncResult {
  int downloaded = 0;
  int total = 0;
  std::string error;
};

/* Blocking. Call from a worker, not the UI thread. */
InboxSyncResult sync_inbox(const ImapAccount& account);

}  // namespace dispatch
