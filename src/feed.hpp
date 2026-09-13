/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>
#include <vector>

namespace dispatch {

struct Headline {
  std::string subject;
  std::string date;
  std::string body;
  std::string link;
};

struct ParsedFeed {
  std::string title;
  std::vector<Headline> items;
  std::string error;
};

std::string html_to_text(const std::string& html);
std::string short_date(const std::string& raw);
ParsedFeed parse_feed(const std::string& xml, const std::string& fallback_title);

}  // namespace dispatch
