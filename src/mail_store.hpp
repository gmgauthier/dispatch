/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace dispatch {

struct MailMessage {
  uint32_t uid = 0;
  std::string path;
  std::string from;
  std::string subject;
  std::string date;
  std::string html;
  bool unread = true;
};

std::string mail_root();
void ensure_maildirs();
std::string inbox_cur_dir();
uint32_t inbox_uidvalidity();
void inbox_set_uidvalidity(uint32_t uidvalidity);
void inbox_wipe();
std::set<uint32_t> inbox_uids();
bool inbox_write(uint32_t uid, const char* rfc822, size_t len, bool seen);
std::vector<MailMessage> load_mail_folder(int folder_index);

}  // namespace dispatch
