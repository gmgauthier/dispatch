/* SPDX-License-Identifier: Unlicense */

#include "check.hpp"
#include "mail_store.hpp"

#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

namespace fs = std::filesystem;

class TempDir {
 public:
  TempDir()
  {
    char tmpl[] = "/tmp/dispatch-seen-XXXXXX";
    if (char* made = mkdtemp(tmpl))
      path_ = made;
  }

  ~TempDir()
  {
    if (!path_.empty())
      fs::remove_all(path_);
  }

  const std::string& path() const
  {
    return path_;
  }

 private:
  std::string path_;
};

bool listed(const std::vector<uint32_t>& uids, uint32_t uid)
{
  for (uint32_t item : uids) {
    if (item == uid)
      return true;
  }
  return false;
}

bool uid_seen(const std::string& dir, uint32_t uid)
{
  for (const auto& item : dispatch::folder_uid_seen(dir)) {
    if (item.first == uid)
      return item.second;
  }
  return false;
}

const dispatch::MailMessage* find_uid(const std::vector<dispatch::MailMessage>& items,
                                       uint32_t uid)
{
  for (const auto& item : items) {
    if (item.uid == uid)
      return &item;
  }
  return nullptr;
}

const char kSeven[] =
    "From: Ada <ada@example.com>\r\n"
    "To: Greg <greg@example.com>\r\n"
    "Subject: Seven\r\n"
    "Date: Fri, 02 Oct 2026 08:00:00 +0100\r\n"
    "Message-ID: <seen-7@example.com>\r\n"
    "\r\n"
    "seven\r\n";

const char kEight[] =
    "From: Ada <ada@example.com>\r\n"
    "To: Greg <greg@example.com>\r\n"
    "Subject: Eight\r\n"
    "Date: Fri, 02 Oct 2026 08:01:00 +0100\r\n"
    "Message-ID: <seen-8@example.com>\r\n"
    "\r\n"
    "eight\r\n";

const char kNine[] =
    "From: Ada <ada@example.com>\r\n"
    "To: Greg <greg@example.com>\r\n"
    "Subject: Nine\r\n"
    "Date: Fri, 02 Oct 2026 08:02:00 +0100\r\n"
    "Message-ID: <seen-9@example.com>\r\n"
    "\r\n"
    "nine\r\n";

}  // namespace

int main()
{
  TempDir dir;
  CHECK(!dir.path().empty());
  CHECK(setenv("XDG_DATA_HOME", dir.path().c_str(), 1) == 0);

  dispatch::load_mail_folders();
  dispatch::ensure_maildirs();

  CHECK(dispatch::folder_write_uid("Inbox", 7, kSeven, sizeof(kSeven) - 1, false));
  CHECK(dispatch::folder_write_uid("Inbox", 8, kEight, sizeof(kEight) - 1, true));
  CHECK(dispatch::folder_write_uid("Inbox", 9, kNine, sizeof(kNine) - 1, false));

  const std::set<uint32_t> on_server{7, 8, 9};
  dispatch::SeenStore push = dispatch::seen_flags_to_store("Inbox", on_server);
  CHECK(push.add_seen.empty());
  CHECK(push.remove_seen.empty());

  CHECK(dispatch::folder_take_server_seen("Inbox", 7, true));
  CHECK(uid_seen("Inbox", 7));
  const auto after_read = dispatch::load_mail_folder(dispatch::kFolderInbox);
  const auto* seven = find_uid(after_read, 7);
  CHECK(seven != nullptr);
  CHECK(seven && !seven->unread);

  CHECK(dispatch::folder_take_server_seen("Inbox", 8, false));
  CHECK(!uid_seen("Inbox", 8));
  const auto after_unread = dispatch::load_mail_folder(dispatch::kFolderInbox);
  const auto* eight = find_uid(after_unread, 8);
  CHECK(eight != nullptr);
  CHECK(eight && eight->unread);

  push = dispatch::seen_flags_to_store("Inbox", on_server);
  CHECK(push.add_seen.empty());
  CHECK(push.remove_seen.empty());

  const auto before_mark = dispatch::load_mail_folder(dispatch::kFolderInbox);
  const auto* nine = find_uid(before_mark, 9);
  CHECK(nine != nullptr);
  std::string nine_path = nine ? nine->path : std::string();
  CHECK(dispatch::folder_set_seen(nine_path, true));
  CHECK(uid_seen("Inbox", 9));

  push = dispatch::seen_flags_to_store("Inbox", on_server);
  CHECK(listed(push.add_seen, 9));
  CHECK(push.add_seen.size() == 1);
  CHECK(push.remove_seen.empty());

  CHECK(!dispatch::folder_take_server_seen("Inbox", 9, false));
  CHECK(uid_seen("Inbox", 9));

  const std::set<uint32_t> without_nine{7, 8};
  push = dispatch::seen_flags_to_store("Inbox", without_nine);
  CHECK(push.add_seen.empty());
  CHECK(push.remove_seen.empty());

  dispatch::folder_clear_stored_seen("Inbox", {9});
  push = dispatch::seen_flags_to_store("Inbox", on_server);
  CHECK(push.add_seen.empty());
  CHECK(dispatch::folder_take_server_seen("Inbox", 9, false));
  CHECK(!uid_seen("Inbox", 9));

  const auto marked = dispatch::load_mail_folder(dispatch::kFolderInbox);
  const auto* eight_again = find_uid(marked, 8);
  CHECK(eight_again != nullptr);
  std::string eight_path = eight_again ? eight_again->path : std::string();
  CHECK(dispatch::folder_set_seen(eight_path, true));
  push = dispatch::seen_flags_to_store("Inbox", on_server);
  CHECK(listed(push.add_seen, 8));
  CHECK(!listed(push.add_seen, 7));
  CHECK(!listed(push.remove_seen, 8));

  dispatch::folder_wipe("Inbox");
  push = dispatch::seen_flags_to_store("Inbox", on_server);
  CHECK(push.add_seen.empty());
  CHECK(push.remove_seen.empty());
  CHECK(dispatch::folder_uid_seen("Inbox").empty());
  CHECK(!dispatch::folder_take_server_seen("Inbox", 42, true));
  CHECK(!dispatch::folder_take_server_seen("Inbox", 0, true));
  CHECK(dispatch::seen_flags_to_store("", on_server).add_seen.empty());

  return suite_test::done("seen");
}
