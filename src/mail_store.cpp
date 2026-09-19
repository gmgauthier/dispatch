/* SPDX-License-Identifier: Unlicense */

#include "mail_store.hpp"
#include "mail_parse.hpp"

#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <utility>
#include <vector>

namespace dispatch {
namespace {

std::vector<MailFolderInfo> g_folders;

std::string folder_dir(const std::string& name)
{
  return Glib::build_filename(mail_root(), name);
}

void ensure_maildir(const std::string& dir)
{
  g_mkdir_with_parents(Glib::build_filename(dir, "tmp").c_str(), 0700);
  g_mkdir_with_parents(Glib::build_filename(dir, "new").c_str(), 0700);
  g_mkdir_with_parents(Glib::build_filename(dir, "cur").c_str(), 0700);
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

std::string deleted_path(const std::string& dir)
{
  return Glib::build_filename(folder_dir(dir), ".deleted");
}

std::string uidvalidity_path(const std::string& dir)
{
  return Glib::build_filename(folder_dir(dir), ".uidvalidity");
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

void remember_deleted(const std::string& dir, uint32_t uid)
{
  if (uid == 0 || dir.empty())
    return;
  auto uids = read_uid_set(deleted_path(dir));
  uids.insert(uid);
  write_uid_set(deleted_path(dir), uids);
}

void forget_deleted(const std::string& dir, uint32_t uid)
{
  if (uid == 0 || dir.empty())
    return;
  auto uids = read_uid_set(deleted_path(dir));
  if (!uids.erase(uid))
    return;
  write_uid_set(deleted_path(dir), uids);
}

std::string msgid_from_rfc822(const char* rfc822, size_t len)
{
  if (!rfc822 || len == 0)
    return {};
  const std::string raw(rfc822, rfc822 + std::min(len, static_cast<size_t>(8192)));
  auto pos = raw.find("\r\n\r\n");
  auto hdr = raw.substr(0, pos == std::string::npos ? raw.size() : pos);
  const std::string lower = [&]() {
    std::string s = hdr;
    for (char& c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }();
  auto i = lower.find("\nmessage-id:");
  if (i == std::string::npos && lower.compare(0, 11, "message-id:") == 0)
    i = 0;
  else if (i != std::string::npos)
    i += 1;
  if (i == std::string::npos)
    return {};
  auto start = hdr.find(':', i);
  if (start == std::string::npos)
    return {};
  ++start;
  auto end = hdr.find('\n', start);
  std::string id = hdr.substr(start, end == std::string::npos ? std::string::npos : end - start);
  while (!id.empty() && (id.back() == '\r' || id.back() == ' ' || id.back() == '\t'))
    id.pop_back();
  while (!id.empty() && (id.front() == ' ' || id.front() == '\t'))
    id.erase(id.begin());
  return id;
}

void remove_dir_uid_copies(const std::string& dir, uint32_t uid, const std::string& keep_path)
{
  if (uid == 0)
    return;
  for (const char* sub : {"cur", "new"}) {
    const std::string d = Glib::build_filename(folder_dir(dir), sub);
    if (!Glib::file_test(d, Glib::FILE_TEST_IS_DIR))
      continue;
    Glib::Dir gd(d);
    std::vector<std::string> names;
    for (const std::string& name : gd)
      names.push_back(name);
    for (const auto& name : names) {
      if (uid_from_name(name) != uid)
        continue;
      const std::string p = Glib::build_filename(d, name);
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

std::string lower_copy(std::string s)
{
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string last_component(const std::string& imap)
{
  auto pos = imap.find_last_of("/.");
  if (pos == std::string::npos || pos + 1 >= imap.size())
    return imap;
  return imap.substr(pos + 1);
}

std::string safe_dir(const std::string& imap)
{
  std::string s;
  for (unsigned char c : imap) {
    if (std::isalnum(c) || c == '-' || c == '_')
      s += static_cast<char>(c);
    else if (c == '/' || c == '.')
      s += '.';
    else
      s += '_';
  }
  while (!s.empty() && s.front() == '.')
    s.erase(s.begin());
  if (s.empty())
    s = "folder";
  return s;
}

int role_for_imap(const std::string& imap)
{
  const std::string l = lower_copy(imap);
  if (l == "inbox")
    return kFolderInbox;
  if (l == "sent" || l == "sent messages" || l == "sent items" || l == "sent mail")
    return kFolderSent;
  if (l == "drafts" || l == "draft")
    return kFolderDrafts;
  if (l == "trash" || l == "deleted" || l == "deleted messages" || l == "deleted items" ||
      l == "bin")
    return kFolderTrash;
  return kFolderExtra;
}

bool skip_imap_name(const std::string& imap)
{
  const std::string l = lower_copy(imap);
  return l == "all mail" || l == "allmail" || l.find("all mail") != std::string::npos;
}

std::string folders_ini()
{
  return Glib::build_filename(mail_root(), "folders.ini");
}

void set_defaults()
{
  g_folders = {
      {"Inbox", "INBOX", "Inbox", kFolderInbox}, {"Sent", "", "Sent", kFolderSent},
      {"Drafts", "", "Drafts", kFolderDrafts},   {"Outbox", "", "Outbox", kFolderOutbox},
      {"Trash", "", "Trash", kFolderTrash},
  };
}

}  // namespace

std::string mail_root()
{
  return Glib::build_filename(Glib::get_user_data_dir(), "dispatch", "mail");
}

void load_mail_folders()
{
  if (g_folders.size() < static_cast<size_t>(kFolderCount))
    set_defaults();
  Glib::KeyFile kf;
  try {
    kf.load_from_file(folders_ini());
  } catch (const Glib::Error&) {
    return;
  }
  int n = 0;
  try {
    n = kf.get_integer("folders", "count");
  } catch (const Glib::Error&) {
    return;
  }
  if (n < kFolderCount)
    return;
  std::vector<MailFolderInfo> loaded;
  for (int i = 0; i < n; ++i) {
    const std::string g = "folder" + std::to_string(i);
    MailFolderInfo f;
    try {
      f.display = kf.get_string(g, "display");
      f.imap = kf.get_string(g, "imap");
      f.dir = kf.get_string(g, "dir");
      f.role = kf.get_integer(g, "role");
    } catch (const Glib::Error&) {
      continue;
    }
    if (f.display.empty() || f.dir.empty())
      continue;
    loaded.push_back(std::move(f));
  }
  if (loaded.size() >= static_cast<size_t>(kFolderCount))
    g_folders = std::move(loaded);
}

void save_mail_folders()
{
  g_mkdir_with_parents(mail_root().c_str(), 0700);
  Glib::KeyFile kf;
  kf.set_integer("folders", "count", static_cast<int>(g_folders.size()));
  for (int i = 0; i < static_cast<int>(g_folders.size()); ++i) {
    const auto& f = g_folders[static_cast<size_t>(i)];
    const std::string g = "folder" + std::to_string(i);
    kf.set_string(g, "display", f.display);
    kf.set_string(g, "imap", f.imap);
    kf.set_string(g, "dir", f.dir);
    kf.set_integer(g, "role", f.role);
  }
  try {
    kf.save_to_file(folders_ini());
  } catch (const Glib::Error&) {
  }
}

void merge_imap_folders(const std::vector<std::string>& imap_names)
{
  if (g_folders.size() < static_cast<size_t>(kFolderCount))
    set_defaults();
  std::set<std::string> used_dir;
  for (const auto& f : g_folders)
    used_dir.insert(f.dir);
  for (const auto& imap : imap_names) {
    if (imap.empty() || skip_imap_name(imap))
      continue;
    const int role = role_for_imap(imap);
    if (role >= 0 && role < kFolderCount) {
      if (g_folders[static_cast<size_t>(role)].imap.empty() || role == kFolderInbox)
        g_folders[static_cast<size_t>(role)].imap = imap;
      continue;
    }
    bool have = false;
    for (const auto& f : g_folders) {
      if (f.imap == imap)
        have = true;
    }
    if (have)
      continue;
    MailFolderInfo extra;
    extra.display = last_component(imap);
    extra.imap = imap;
    extra.dir = safe_dir(imap);
    extra.role = kFolderExtra;
    int n = 2;
    std::string base = extra.dir;
    while (used_dir.count(extra.dir))
      extra.dir = base + "_" + std::to_string(n++);
    used_dir.insert(extra.dir);
    g_folders.push_back(std::move(extra));
  }
  if (g_folders.size() > static_cast<size_t>(kFolderCount)) {
    std::sort(
        g_folders.begin() + kFolderCount, g_folders.end(),
        [](const MailFolderInfo& a, const MailFolderInfo& b) { return a.display < b.display; });
  }
  save_mail_folders();
}

int mail_folder_count()
{
  if (g_folders.empty())
    set_defaults();
  return static_cast<int>(g_folders.size());
}

const MailFolderInfo& mail_folder(int index)
{
  if (g_folders.empty())
    set_defaults();
  if (index < 0 || index >= static_cast<int>(g_folders.size()))
    return g_folders.front();
  return g_folders[static_cast<size_t>(index)];
}

void ensure_maildirs()
{
  g_mkdir_with_parents(mail_root().c_str(), 0700);
  if (g_folders.empty())
    set_defaults();
  for (const auto& f : g_folders)
    ensure_maildir(folder_dir(f.dir));
}

std::string inbox_cur_dir()
{
  return Glib::build_filename(folder_dir("Inbox"), "cur");
}

uint32_t folder_uidvalidity(const std::string& dir)
{
  const std::string path = uidvalidity_path(dir);
  if (!Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR))
    return 0;
  try {
    const std::string s = Glib::file_get_contents(path);
    return static_cast<uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
  } catch (const Glib::Error&) {
    return 0;
  }
}

void folder_set_uidvalidity(const std::string& dir, uint32_t uidvalidity)
{
  std::ofstream f(uidvalidity_path(dir), std::ios::trunc);
  if (f)
    f << uidvalidity << '\n';
}

void folder_wipe(const std::string& dir)
{
  ::unlink(deleted_path(dir).c_str());
  for (const char* sub : {"cur", "new", "tmp"}) {
    const std::string d = Glib::build_filename(folder_dir(dir), sub);
    if (!Glib::file_test(d, Glib::FILE_TEST_IS_DIR))
      continue;
    Glib::Dir gd(d);
    std::vector<std::string> names;
    for (const std::string& name : gd)
      names.push_back(name);
    for (const auto& name : names) {
      if (name == "." || name == "..")
        continue;
      ::unlink(Glib::build_filename(d, name).c_str());
    }
  }
}

std::set<uint32_t> folder_skip_uids(const std::string& dir)
{
  std::set<uint32_t> uids = scan_uids(Glib::build_filename(folder_dir(dir), "cur"));
  const auto more = scan_uids(Glib::build_filename(folder_dir(dir), "new"));
  uids.insert(more.begin(), more.end());
  const auto deleted = read_uid_set(deleted_path(dir));
  uids.insert(deleted.begin(), deleted.end());
  return uids;
}

std::set<uint32_t> folder_deleted_uids(const std::string& dir)
{
  return read_uid_set(deleted_path(dir));
}

void folder_drop_deleted(const std::string& dir, const std::set<uint32_t>& done)
{
  if (done.empty())
    return;
  auto uids = read_uid_set(deleted_path(dir));
  for (uint32_t u : done)
    uids.erase(u);
  write_uid_set(deleted_path(dir), uids);
}

bool folder_has_msgid(const std::string& dir, const std::string& msgid)
{
  if (msgid.empty())
    return false;
  for (const char* sub : {"cur", "new"}) {
    const std::string d = Glib::build_filename(folder_dir(dir), sub);
    if (!Glib::file_test(d, Glib::FILE_TEST_IS_DIR))
      continue;
    Glib::Dir gd(d);
    for (const std::string& name : gd) {
      if (name.empty() || name[0] == '.')
        continue;
      try {
        const std::string raw = Glib::file_get_contents(Glib::build_filename(d, name));
        if (msgid_from_rfc822(raw.data(), raw.size()) == msgid)
          return true;
      } catch (const Glib::Error&) {
      }
    }
  }
  return false;
}

std::vector<std::pair<uint32_t, bool>> folder_uid_seen(const std::string& dir)
{
  std::vector<std::pair<uint32_t, bool>> out;
  for (const char* sub : {"cur", "new"}) {
    const std::string d = Glib::build_filename(folder_dir(dir), sub);
    if (!Glib::file_test(d, Glib::FILE_TEST_IS_DIR))
      continue;
    Glib::Dir gd(d);
    for (const std::string& name : gd) {
      const uint32_t uid = uid_from_name(name);
      if (uid)
        out.emplace_back(uid, name_seen(name));
    }
  }
  return out;
}

bool folder_write_uid(const std::string& dir, uint32_t uid, const char* rfc822, size_t len,
                      bool seen)
{
  if (!rfc822 || len == 0 || uid == 0 || dir.empty())
    return false;
  if (folder_skip_uids(dir).count(uid))
    return true;
  const std::string mid = msgid_from_rfc822(rfc822, len);
  if (!mid.empty() && folder_has_msgid(dir, mid))
    return true;
  ensure_maildir(folder_dir(dir));
  std::ostringstream name;
  name << uid << ".M" << static_cast<long>(::time(nullptr)) << "P" << ::getpid() << ".dispatch";
  const std::string tmp = Glib::build_filename(folder_dir(dir), "tmp", name.str());
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f)
      return false;
    f.write(rfc822, static_cast<std::streamsize>(len));
    if (!f)
      return false;
  }
  const std::string dest =
      Glib::build_filename(folder_dir(dir), "cur", name.str() + (seen ? ":2,S" : ":2,"));
  if (::rename(tmp.c_str(), dest.c_str()) != 0) {
    ::unlink(tmp.c_str());
    return false;
  }
  return true;
}

const char* mail_folder_name(int folder_index)
{
  if (g_folders.empty())
    set_defaults();
  if (folder_index < 0 || folder_index >= static_cast<int>(g_folders.size()))
    return "";
  return g_folders[static_cast<size_t>(folder_index)].display.c_str();
}

bool folder_write(int folder_index, const char* rfc822, size_t len, bool seen)
{
  if (g_folders.empty())
    set_defaults();
  if (!rfc822 || len == 0 || folder_index < 0 || folder_index >= static_cast<int>(g_folders.size()))
    return false;
  const std::string& folder = g_folders[static_cast<size_t>(folder_index)].dir;
  ensure_maildir(folder_dir(folder));
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
  const std::string name = Glib::path_get_basename(path);
  const uint32_t uid = uid_from_name(name);
  const std::string parent =
      Glib::path_get_basename(Glib::path_get_dirname(Glib::path_get_dirname(path)));
  if (uid && !parent.empty())
    remember_deleted(parent, uid);
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
  if (g_folders.empty())
    set_defaults();
  if (path.empty() || dest_folder < 0 || dest_folder >= static_cast<int>(g_folders.size()))
    return false;
  ensure_maildirs();
  const std::string name_only = Glib::path_get_basename(path);
  const uint32_t uid = uid_from_name(name_only);
  const std::string parent =
      Glib::path_get_basename(Glib::path_get_dirname(Glib::path_get_dirname(path)));
  const std::string dest_dir = g_folders[static_cast<size_t>(dest_folder)].dir;
  if (uid && parent != dest_dir)
    remember_deleted(parent, uid);
  if (dest_folder == kFolderInbox && uid) {
    forget_deleted("Inbox", uid);
    remove_dir_uid_copies("Inbox", uid, path);
  }
  std::string name = name_only;
  if (name.rfind(":2,") == std::string::npos)
    name += ":2,";
  std::string dest = Glib::build_filename(folder_dir(dest_dir), "cur", name);
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
  if (g_folders.empty())
    set_defaults();
  if (folder_index < 0 || folder_index >= static_cast<int>(g_folders.size()))
    return out;
  const std::string& folder = g_folders[static_cast<size_t>(folder_index)].dir;
  scan_dir(Glib::build_filename(folder_dir(folder), "cur"), out);
  scan_dir(Glib::build_filename(folder_dir(folder), "new"), out);
  std::sort(out.begin(), out.end(),
            [](const MailMessage& a, const MailMessage& b) { return a.uid > b.uid; });
  return out;
}

}  // namespace dispatch
