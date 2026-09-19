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
                         const std::string& body);
std::vector<std::string> split_addresses(const std::string& raw);

}  // namespace dispatch
