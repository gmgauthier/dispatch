/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>
#include <vector>

namespace dispatch {

struct Enclosure {
  std::string url;
  std::string type;
  std::string title;
};

struct Headline {
  std::string subject;
  std::string date;
  std::string html;
  std::string link;
  std::vector<Enclosure> enclosures;
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
