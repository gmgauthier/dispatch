/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <set>
#include <string>
#include <vector>

namespace dispatch {

struct SavedFeed {
  std::string url;
  std::string title;
};

struct Settings {
  std::vector<SavedFeed> feeds;
  std::set<std::string> read;

  void load();
  void save() const;
  bool is_read(const std::string& key) const;
  void mark_read(const std::string& key);
  void mark_unread(const std::string& key);
};

std::string item_key(const std::string& link, const std::string& subject,
                     const std::string& date);

}  // namespace dispatch
