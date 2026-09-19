/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "mail_store.hpp"

#include <string>
#include <vector>

namespace dispatch {

void parse_rfc822(const std::string& raw, MailMessage& out);
void parse_rfc822_envelope(const std::string& raw, std::string& from,
                           std::vector<std::string>& rcpt);
std::string build_rfc822(const std::string& from, const std::vector<std::string>& to,
                         const std::vector<std::string>& cc, const std::string& subject,
                         const std::string& body, const std::string& in_reply_to = {},
                         const std::string& references = {});
std::vector<std::string> split_addresses(const std::string& raw);
std::string quote_plain(const std::string& from, const std::string& date, const std::string& text);
std::string with_re_prefix(const std::string& subject);
std::string with_fwd_prefix(const std::string& subject);
std::vector<int> thread_parents(const std::vector<MailMessage>& items);

}  // namespace dispatch
