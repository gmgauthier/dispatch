/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "feed.hpp"

#include <string>

namespace dispatch {

bool load_feed_cache(const std::string& url, ParsedFeed& out);
bool save_feed_cache(const std::string& url, const std::string& title,
                     const std::vector<Headline>& items);
void delete_feed_cache(const std::string& url);

}  // namespace dispatch
