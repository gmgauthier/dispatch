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
  return name.find('S', colon) != std::string::npos;
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
  std::set<uint32_t> uids;
  for (const char* sub : {"cur", "new"}) {
    const std::string dir = Glib::build_filename(folder_dir("Inbox"), sub);
    if (!Glib::file_test(dir, Glib::FILE_TEST_IS_DIR))
      continue;
    Glib::Dir gd(dir);
    for (const std::string& name : gd) {
      const uint32_t uid = uid_from_name(name);
      if (uid)
        uids.insert(uid);
    }
  }
  return uids;
}

bool inbox_write(uint32_t uid, const char* rfc822, size_t len, bool seen)
{
  if (!rfc822 || len == 0 || uid == 0)
    return false;
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

std::vector<MailMessage> load_mail_folder(int folder_index)
{
  std::vector<MailMessage> out;
  if (folder_index != 0)
    return out;
  scan_dir(Glib::build_filename(folder_dir("Inbox"), "cur"), out);
  scan_dir(Glib::build_filename(folder_dir("Inbox"), "new"), out);
  std::sort(out.begin(), out.end(),
            [](const MailMessage& a, const MailMessage& b) { return a.uid > b.uid; });
  return out;
}

}  // namespace dispatch
