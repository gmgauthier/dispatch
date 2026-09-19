/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dispatch {

struct ImapAccount {
  std::string host;
  uint16_t port = 143;
  bool starttls = true;
  std::string user;
  std::string password;
};

struct MailSyncResult {
  int downloaded = 0;
  int total = 0;
  int folders = 0;
  std::string error;
};

/* Blocking. Call from a worker, not the UI thread. */
MailSyncResult sync_mailboxes(const ImapAccount& account);

}  // namespace dispatch
