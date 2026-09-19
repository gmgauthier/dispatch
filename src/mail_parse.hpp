/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "mail_store.hpp"

#include <string>

namespace dispatch {

void parse_rfc822(const std::string& raw, MailMessage& out);

}  // namespace dispatch
