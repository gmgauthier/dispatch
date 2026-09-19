/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dispatch {

struct SmtpAccount {
  std::string host;
  uint16_t port = 587;
  bool starttls = true;
  std::string user;
  std::string password;
};

struct SmtpResult {
  std::string error;
};

struct OutboxFlushResult {
  int sent = 0;
  int failed = 0;
  std::string error;
};

/* Blocking. Call from a worker, not the UI thread. */
SmtpResult smtp_send(const SmtpAccount& account, const std::string& from,
                     const std::vector<std::string>& rcpt, const std::string& rfc822);
OutboxFlushResult flush_outbox(const SmtpAccount& account);

}  // namespace dispatch
