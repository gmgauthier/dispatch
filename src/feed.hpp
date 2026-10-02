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

/* A fetch whose subscription was removed. on_fetch_done must not store it. */
constexpr int kFetchDiscard = -2;

/* feeds.erase at gone. Drop a replace index that pointed at gone, and shift
   a later index down. An index below gone, or a negative append slot, stays. */
void retarget_fetch_index(int& replace_index, int gone);

std::string html_to_text(const std::string& html);
std::string short_date(const std::string& raw);
ParsedFeed parse_feed(const std::string& xml, const std::string& fallback_title);

}  // namespace dispatch
