/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace dispatch {

struct MailMessage {
  uint32_t uid = 0;
  std::string path;
  std::string from;
  std::string subject;
  std::string date;
  std::int64_t date_unix = 0;
  std::string html;
  std::string text;
  std::string reply_addr;
  std::string msgid;
  std::string in_reply_to;
  std::vector<std::string> references;
  bool unread = true;
};

std::string mail_root();
void ensure_maildirs();
std::string inbox_cur_dir();
uint32_t inbox_uidvalidity();
void inbox_set_uidvalidity(uint32_t uidvalidity);
void inbox_wipe();
std::set<uint32_t> inbox_uids();
std::set<uint32_t> inbox_skip_uids();
std::vector<std::pair<uint32_t, bool>> inbox_uid_seen();
bool inbox_write(uint32_t uid, const char* rfc822, size_t len, bool seen);
bool folder_write(int folder_index, const char* rfc822, size_t len, bool seen);
bool folder_remove(const std::string& path);
bool folder_move(std::string& path, int dest_folder);
bool folder_set_seen(std::string& path, bool seen);
const char* mail_folder_name(int folder_index);
std::vector<MailMessage> load_mail_folder(int folder_index);

enum {
  kFolderInbox = 0,
  kFolderSent = 1,
  kFolderDrafts = 2,
  kFolderOutbox = 3,
  kFolderTrash = 4,
  kFolderCount = 5
};

}  // namespace dispatch
