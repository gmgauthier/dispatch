/* SPDX-License-Identifier: Unlicense */

#include "settings.hpp"

#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>

#include <algorithm>
#include <cctype>

namespace dispatch {
namespace {

std::string config_dir()
{
  return Glib::build_filename(Glib::get_user_config_dir(), "dispatch");
}

std::string config_path()
{
  return Glib::build_filename(config_dir(), "dispatch.ini");
}

std::string get_str(Glib::KeyFile& kf, const Glib::ustring& group, const char* key)
{
  try {
    if (kf.has_key(group, key))
      return kf.get_string(group, key);
  } catch (const Glib::Error&) {
  }
  return {};
}

int get_int(Glib::KeyFile& kf, const Glib::ustring& group, const char* key, int def)
{
  try {
    if (kf.has_key(group, key))
      return kf.get_integer(group, key);
  } catch (const Glib::Error&) {
  }
  return def;
}

bool get_bool(Glib::KeyFile& kf, const Glib::ustring& group, const char* key, bool def)
{
  try {
    if (kf.has_key(group, key))
      return kf.get_boolean(group, key);
  } catch (const Glib::Error&) {
  }
  return def;
}

int feed_index(const std::string& group)
{
  if (group.compare(0, 5, "feed.") != 0)
    return -1;
  int n = 0;
  for (size_t i = 5; i < group.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(group[i])))
      return -1;
    n = n * 10 + (group[i] - '0');
  }
  return n;
}

}  // namespace

std::string item_key(const std::string& link, const std::string& subject, const std::string& date)
{
  if (!link.empty())
    return link;
  return subject + "\t" + date;
}

void Settings::load()
{
  feeds.clear();
  read.clear();
  const std::string path = config_path();
  if (!Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR))
    return;
  Glib::KeyFile kf;
  try {
    kf.load_from_file(path);
  } catch (const Glib::Error&) {
    return;
  }

  struct Row {
    int n;
    SavedFeed f;
  };
  std::vector<Row> rows;
  for (const Glib::ustring& group : kf.get_groups()) {
    const int n = feed_index(group.raw());
    if (n < 0)
      continue;
    SavedFeed f;
    f.url = get_str(kf, group, "url");
    f.title = get_str(kf, group, "title");
    if (f.url.empty())
      continue;
    rows.push_back({n, std::move(f)});
  }
  std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.n < b.n; });
  for (auto& r : rows)
    feeds.push_back(std::move(r.f));

  try {
    if (kf.has_group("read") && kf.has_key("read", "keys")) {
      for (const Glib::ustring& k : kf.get_string_list("read", "keys")) {
        if (!k.empty())
          read.insert(k.raw());
      }
    }
  } catch (const Glib::Error&) {
  }

  if (kf.has_group("appearance")) {
    const std::string fam = get_str(kf, "appearance", "family");
    if (!fam.empty())
      font_family = fam;
    font_size = get_int(kf, "appearance", "size", font_size);
    if (font_size < 8)
      font_size = 8;
    if (font_size > 32)
      font_size = 32;
    font_weight = get_int(kf, "appearance", "weight", font_weight);
    palette = get_int(kf, "appearance", "palette", palette);
    if (palette < 0 || palette > 2)
      palette = 1;
  }

  if (kf.has_group("window")) {
    window_w = get_int(kf, "window", "width", window_w);
    window_h = get_int(kf, "window", "height", window_h);
    feeds_sash = get_int(kf, "window", "feeds_sash", feeds_sash);
    headlines_sash = get_int(kf, "window", "headlines_sash", headlines_sash);
    preview = get_bool(kf, "window", "preview", preview);
    last_url = get_str(kf, "window", "last_url");
    refresh_minutes = get_int(kf, "window", "refresh_minutes", refresh_minutes);
    mail_mode = get_bool(kf, "window", "mail_mode", true);
    if (refresh_minutes < 0)
      refresh_minutes = 0;
    if (refresh_minutes > 1440)
      refresh_minutes = 1440;
  }
}

void Settings::save() const
{
  g_mkdir_with_parents(config_dir().c_str(), 0700);
  Glib::KeyFile kf;
  for (size_t i = 0; i < feeds.size(); ++i) {
    const std::string group = "feed." + std::to_string(i);
    kf.set_string(group, "url", feeds[i].url);
    kf.set_string(group, "title", feeds[i].title);
  }
  std::vector<Glib::ustring> keys;
  keys.reserve(read.size());
  for (const auto& k : read)
    keys.emplace_back(k);
  if (!keys.empty())
    kf.set_string_list("read", "keys", keys);
  kf.set_string("appearance", "family", font_family);
  kf.set_integer("appearance", "size", font_size);
  kf.set_integer("appearance", "weight", font_weight);
  kf.set_integer("appearance", "palette", palette);
  kf.set_integer("window", "width", window_w);
  kf.set_integer("window", "height", window_h);
  kf.set_integer("window", "feeds_sash", feeds_sash);
  kf.set_integer("window", "headlines_sash", headlines_sash);
  kf.set_boolean("window", "preview", preview);
  kf.set_string("window", "last_url", last_url);
  kf.set_integer("window", "refresh_minutes", refresh_minutes);
  kf.set_boolean("window", "mail_mode", mail_mode);
  try {
    kf.save_to_file(config_path());
  } catch (const Glib::Error&) {
  }
}

bool Settings::is_read(const std::string& key) const
{
  return !key.empty() && read.count(key) != 0;
}

void Settings::mark_read(const std::string& key)
{
  if (!key.empty())
    read.insert(key);
}

void Settings::mark_unread(const std::string& key)
{
  read.erase(key);
}

}  // namespace dispatch
