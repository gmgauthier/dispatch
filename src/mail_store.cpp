/* SPDX-License-Identifier: Unlicense */

#include "mail_store.hpp"
#include "mail_parse.hpp"

#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <utility>
#include <vector>

namespace dispatch {
namespace {

const char* kFolders[] = {"Inbox", "Sent", "Drafts", "Outbox", "Trash"};

std::string folder_dir(const char* name)
{
  return Glib::build_filename(mail_root(), name);
}

void ensure_maildir(const std::string& dir)
{
  g_mkdir_with_parents(Glib::build_filename(dir, "tmp").c_str(), 0700);
  g_mkdir_with_parents(Glib::build_filename(dir, "new").c_str(), 0700);
  g_mkdir_with_parents(Glib::build_filename(dir, "cur").c_str(), 0700);
}

std::string uidvalidity_path()
{
  return Glib::build_filename(folder_dir("Inbox"), ".uidvalidity");
}

uint32_t uid_from_name(const std::string& name)
{
  if (name.empty() || name[0] == '.')
    return 0;
  char* end = nullptr;
  const unsigned long v = std::strtoul(name.c_str(), &end, 10);
  if (!end || end == name.c_str() || *end != '.')
    return 0;
  if (v == 0 || v > 0xffffffffUL)
    return 0;
  return static_cast<uint32_t>(v);
}

bool name_seen(const std::string& name)
{
  const auto colon = name.rfind(":2,");
  if (colon == std::string::npos)
    return false;
  const std::string flags = name.substr(colon + 3);
  return flags.find('S') != std::string::npos;
}

std::string deleted_path()
{
  return Glib::build_filename(folder_dir("Inbox"), ".deleted");
}

std::set<uint32_t> read_uid_set(const std::string& path)
{
  std::set<uint32_t> uids;
  if (!Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR))
    return uids;
  try {
    const std::string s = Glib::file_get_contents(path);
    std::istringstream in(s);
    std::string line;
    while (std::getline(in, line)) {
      const uint32_t uid = static_cast<uint32_t>(std::strtoul(line.c_str(), nullptr, 10));
      if (uid)
        uids.insert(uid);
    }
  } catch (const Glib::Error&) {
  }
  return uids;
}

void write_uid_set(const std::string& path, const std::set<uint32_t>& uids)
{
  std::ostringstream out;
  for (uint32_t uid : uids)
    out << uid << '\n';
  try {
    Glib::file_set_contents(path, out.str());
  } catch (const Glib::Error&) {
  }
}

void remember_deleted(uint32_t uid)
{
  if (uid == 0)
    return;
  auto uids = read_uid_set(deleted_path());
  uids.insert(uid);
  write_uid_set(deleted_path(), uids);
}

void forget_deleted(uint32_t uid)
{
  if (uid == 0)
    return;
  auto uids = read_uid_set(deleted_path());
  if (!uids.erase(uid))
    return;
  write_uid_set(deleted_path(), uids);
}

void remove_inbox_uid_copies(uint32_t uid, const std::string& keep_path)
{
  if (uid == 0)
    return;
  for (const char* sub : {"cur", "new"}) {
    const std::string dir = Glib::build_filename(folder_dir("Inbox"), sub);
    if (!Glib::file_test(dir, Glib::FILE_TEST_IS_DIR))
      continue;
    Glib::Dir gd(dir);
    std::vector<std::string> names;
    for (const std::string& name : gd)
      names.push_back(name);
    for (const auto& name : names) {
      if (uid_from_name(name) != uid)
        continue;
      const std::string p = Glib::build_filename(dir, name);
      if (p != keep_path)
        ::unlink(p.c_str());
    }
  }
}

std::set<uint32_t> scan_uids(const std::string& dir)
{
  std::set<uint32_t> uids;
  if (!Glib::file_test(dir, Glib::FILE_TEST_IS_DIR))
    return uids;
  Glib::Dir gd(dir);
  for (const std::string& name : gd) {
    const uint32_t uid = uid_from_name(name);
    if (uid)
      uids.insert(uid);
  }
  return uids;
}

void scan_dir(const std::string& dir, std::vector<MailMessage>& out)
{
  if (!Glib::file_test(dir, Glib::FILE_TEST_IS_DIR))
    return;
  Glib::Dir gd(dir);
  for (const std::string& name : gd) {
    const uint32_t uid = uid_from_name(name);
    if (uid == 0)
      continue;
    MailMessage m;
    m.uid = uid;
    m.path = Glib::build_filename(dir, name);
    m.unread = !name_seen(name);
    std::string raw;
    try {
      raw = Glib::file_get_contents(m.path);
    } catch (const Glib::Error&) {
      continue;
    }
    parse_rfc822(raw, m);
    out.push_back(std::move(m));
  }
}

}  // namespace

std::string mail_root()
{
  return Glib::build_filename(Glib::get_user_data_dir(), "dispatch", "mail");
}

void ensure_maildirs()
{
  g_mkdir_with_parents(mail_root().c_str(), 0700);
  for (const char* name : kFolders)
    ensure_maildir(folder_dir(name));
}

std::string inbox_cur_dir()
{
  return Glib::build_filename(folder_dir("Inbox"), "cur");
}

uint32_t inbox_uidvalidity()
{
  const std::string path = uidvalidity_path();
  if (!Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR))
    return 0;
  try {
    const std::string s = Glib::file_get_contents(path);
    return static_cast<uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
  } catch (const Glib::Error&) {
    return 0;
  }
}

void inbox_set_uidvalidity(uint32_t uidvalidity)
{
  std::ofstream f(uidvalidity_path(), std::ios::trunc);
  if (f)
    f << uidvalidity << '\n';
}

void inbox_wipe()
{
  ::unlink(deleted_path().c_str());
  for (const char* sub : {"cur", "new", "tmp"}) {
    const std::string dir = Glib::build_filename(folder_dir("Inbox"), sub);
    if (!Glib::file_test(dir, Glib::FILE_TEST_IS_DIR))
      continue;
    Glib::Dir gd(dir);
    std::vector<std::string> names;
    for (const std::string& name : gd)
      names.push_back(name);
    for (const auto& name : names) {
      if (name == "." || name == "..")
        continue;
      ::unlink(Glib::build_filename(dir, name).c_str());
    }
  }
}

std::set<uint32_t> inbox_uids()
{
  std::set<uint32_t> uids = scan_uids(Glib::build_filename(folder_dir("Inbox"), "cur"));
  const auto more = scan_uids(Glib::build_filename(folder_dir("Inbox"), "new"));
  uids.insert(more.begin(), more.end());
  return uids;
}

std::set<uint32_t> inbox_skip_uids()
{
  std::set<uint32_t> uids = inbox_uids();
  const auto deleted = read_uid_set(deleted_path());
  uids.insert(deleted.begin(), deleted.end());
  for (const char* folder : {"Trash", "Sent", "Drafts", "Outbox"}) {
    const auto extra = scan_uids(Glib::build_filename(folder_dir(folder), "cur"));
    uids.insert(extra.begin(), extra.end());
    const auto extra_new = scan_uids(Glib::build_filename(folder_dir(folder), "new"));
    uids.insert(extra_new.begin(), extra_new.end());
  }
  return uids;
}

std::vector<std::pair<uint32_t, bool>> inbox_uid_seen()
{
  std::vector<std::pair<uint32_t, bool>> out;
  for (const char* sub : {"cur", "new"}) {
    const std::string dir = Glib::build_filename(folder_dir("Inbox"), sub);
    if (!Glib::file_test(dir, Glib::FILE_TEST_IS_DIR))
      continue;
    Glib::Dir gd(dir);
    for (const std::string& name : gd) {
      const uint32_t uid = uid_from_name(name);
      if (uid)
        out.emplace_back(uid, name_seen(name));
    }
  }
  return out;
}

bool inbox_write(uint32_t uid, const char* rfc822, size_t len, bool seen)
{
  if (!rfc822 || len == 0 || uid == 0)
    return false;
  if (inbox_skip_uids().count(uid))
    return true;
  ensure_maildirs();
  std::ostringstream name;
  name << uid << ".M" << static_cast<long>(::time(nullptr)) << "P" << ::getpid() << ".dispatch";
  const std::string tmp = Glib::build_filename(folder_dir("Inbox"), "tmp", name.str());
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f)
      return false;
    f.write(rfc822, static_cast<std::streamsize>(len));
    if (!f)
      return false;
  }
  std::string dest_name = name.str() + (seen ? ":2,S" : ":2,");
  const std::string dest = Glib::build_filename(inbox_cur_dir(), dest_name);
  if (::rename(tmp.c_str(), dest.c_str()) != 0) {
    ::unlink(tmp.c_str());
    return false;
  }
  return true;
}

const char* mail_folder_name(int folder_index)
{
  if (folder_index < 0 || folder_index >= kFolderCount)
    return "";
  return kFolders[folder_index];
}

bool folder_write(int folder_index, const char* rfc822, size_t len, bool seen)
{
  if (!rfc822 || len == 0 || folder_index < 0 || folder_index >= kFolderCount)
    return false;
  ensure_maildirs();
  const char* folder = kFolders[folder_index];
  std::ostringstream name;
  name << static_cast<long>(::time(nullptr)) << ".M" << ::getpid() << "Q" << folder_index
       << ".dispatch";
  const std::string tmp = Glib::build_filename(folder_dir(folder), "tmp", name.str());
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f)
      return false;
    f.write(rfc822, static_cast<std::streamsize>(len));
    if (!f)
      return false;
  }
  const std::string dest =
      Glib::build_filename(folder_dir(folder), "cur", name.str() + (seen ? ":2,S" : ":2,"));
  if (::rename(tmp.c_str(), dest.c_str()) != 0) {
    ::unlink(tmp.c_str());
    return false;
  }
  return true;
}

bool folder_remove(const std::string& path)
{
  if (path.empty())
    return false;
  return ::unlink(path.c_str()) == 0;
}

bool folder_set_seen(std::string& path, bool seen)
{
  if (path.empty())
    return false;
  const std::string dir = Glib::path_get_dirname(path);
  std::string name = Glib::path_get_basename(path);
  const auto colon = name.rfind(":2,");
  if (colon == std::string::npos)
    name += seen ? ":2,S" : ":2,";
  else {
    std::string flags;
    for (char c : name.substr(colon + 3)) {
      if (c != 'S')
        flags += c;
    }
    if (seen)
      flags += 'S';
    name = name.substr(0, colon + 3) + flags;
  }
  std::string dest_dir = dir;
  if (Glib::path_get_basename(dir) == "new")
    dest_dir = Glib::build_filename(Glib::path_get_dirname(dir), "cur");
  const std::string dest = Glib::build_filename(dest_dir, name);
  if (dest == path)
    return true;
  if (Glib::file_test(dest, Glib::FILE_TEST_IS_REGULAR)) {
    ::unlink(path.c_str());
    path = dest;
    return true;
  }
  if (::rename(path.c_str(), dest.c_str()) != 0)
    return false;
  path = dest;
  return true;
}

bool folder_move(std::string& path, int dest_folder)
{
  if (path.empty() || dest_folder < 0 || dest_folder >= kFolderCount)
    return false;
  ensure_maildirs();
  const std::string name_only = Glib::path_get_basename(path);
  const uint32_t uid = uid_from_name(name_only);
  const std::string parent =
      Glib::path_get_basename(Glib::path_get_dirname(Glib::path_get_dirname(path)));
  if (parent == "Inbox" && dest_folder != kFolderInbox && uid)
    remember_deleted(uid);
  if (dest_folder == kFolderInbox && uid) {
    forget_deleted(uid);
    remove_inbox_uid_copies(uid, path);
  }
  std::string name = name_only;
  if (name.rfind(":2,") == std::string::npos)
    name += ":2,";
  std::string dest = Glib::build_filename(folder_dir(kFolders[dest_folder]), "cur", name);
  if (dest == path)
    return true;
  if (Glib::file_test(dest, Glib::FILE_TEST_IS_REGULAR))
    ::unlink(dest.c_str());
  if (::rename(path.c_str(), dest.c_str()) != 0)
    return false;
  path = dest;
  return true;
}

std::vector<MailMessage> load_mail_folder(int folder_index)
{
  std::vector<MailMessage> out;
  if (folder_index < 0 || folder_index >= kFolderCount)
    return out;
  const char* folder = kFolders[folder_index];
  scan_dir(Glib::build_filename(folder_dir(folder), "cur"), out);
  scan_dir(Glib::build_filename(folder_dir(folder), "new"), out);
  std::sort(out.begin(), out.end(),
            [](const MailMessage& a, const MailMessage& b) { return a.uid > b.uid; });
  return out;
}

}  // namespace dispatch
