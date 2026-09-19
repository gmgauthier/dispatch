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
  std::string to;
  std::string cc;
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

enum MailFolderRole {
  kFolderInbox = 0,
  kFolderSent = 1,
  kFolderDrafts = 2,
  kFolderOutbox = 3,
  kFolderTrash = 4,
  kFolderCount = 5,
  kFolderExtra = -1
};

struct MailFolderInfo {
  std::string display;
  std::string imap;
  std::string dir;
  int role = kFolderExtra;
};

std::string mail_root();
void ensure_maildirs();
void load_mail_folders();
void save_mail_folders();
void merge_imap_folders(const std::vector<std::string>& imap_names);
int mail_folder_count();
const MailFolderInfo& mail_folder(int index);
std::string inbox_cur_dir();
uint32_t folder_uidvalidity(const std::string& dir);
void folder_set_uidvalidity(const std::string& dir, uint32_t uidvalidity);
void folder_wipe(const std::string& dir);
std::set<uint32_t> folder_skip_uids(const std::string& dir);
std::set<uint32_t> folder_deleted_uids(const std::string& dir);
void folder_drop_deleted(const std::string& dir, const std::set<uint32_t>& done);
std::vector<std::pair<uint32_t, bool>> folder_uid_seen(const std::string& dir);
bool folder_has_msgid(const std::string& dir, const std::string& msgid);
bool folder_write_uid(const std::string& dir, uint32_t uid, const char* rfc822, size_t len,
                      bool seen);
bool folder_write(int folder_index, const char* rfc822, size_t len, bool seen);
bool folder_remove(const std::string& path);
bool folder_move(std::string& path, int dest_folder);
bool folder_set_seen(std::string& path, bool seen);
const char* mail_folder_name(int folder_index);
std::vector<MailMessage> load_mail_folder(int folder_index);

}  // namespace dispatch
