/* SPDX-License-Identifier: Unlicense */

#include "check.hpp"
#include "mail_store.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

class TempDir {
 public:
  TempDir()
  {
    char tmpl[] = "/tmp/dispatch-trash-XXXXXX";
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

const dispatch::MailFolderInfo* find_imap(const std::string& imap)
{
  const int n = dispatch::mail_folder_count();
  for (int i = 0; i < n; ++i) {
    const auto& f = dispatch::mail_folder(i);
    if (f.imap == imap)
      return &f;
  }
  return nullptr;
}

}  // namespace

int main()
{
  TempDir dir;
  CHECK(!dir.path().empty());
  CHECK(setenv("XDG_DATA_HOME", dir.path().c_str(), 1) == 0);

  dispatch::load_mail_folders();
  dispatch::merge_imap_folders({"INBOX", "INBOX.Sent", "[Gmail]/Drafts", "INBOX.Trash", "Projects",
                                "INBOX.Deleted", "INBOX.Trashcan"});

  CHECK(dispatch::mail_folder(dispatch::kFolderInbox).imap == "INBOX");
  CHECK(dispatch::mail_folder(dispatch::kFolderSent).imap == "INBOX.Sent");
  CHECK(dispatch::mail_folder(dispatch::kFolderDrafts).imap == "[Gmail]/Drafts");
  CHECK(dispatch::mail_folder(dispatch::kFolderTrash).imap == "INBOX.Trash");

  const auto* projects = find_imap("Projects");
  const auto* trashcan = find_imap("INBOX.Trashcan");
  const auto* deleted = find_imap("INBOX.Deleted");
  CHECK(projects != nullptr);
  CHECK(projects && projects->role == dispatch::kFolderExtra);
  CHECK(trashcan != nullptr);
  CHECK(trashcan && trashcan->role == dispatch::kFolderExtra);
  CHECK(deleted == nullptr);

  CHECK(!dispatch::expunge_after_trash_copy("", false, false));
  CHECK(!dispatch::expunge_after_trash_copy("", false, true));
  CHECK(!dispatch::expunge_after_trash_copy("INBOX.Trash", false, false));
  CHECK(!dispatch::expunge_after_trash_copy("INBOX.Trash", true, false));
  CHECK(dispatch::expunge_after_trash_copy("INBOX.Trash", false, true));
  CHECK(!dispatch::fallback_mailbox_expunge());

  const char archived[] =
      "From: Ada <ada@example.com>\r\n"
      "Subject: Move\r\n"
      "Message-ID: <move@example.com>\r\n"
      "\r\n"
      "Hello\r\n";
  CHECK(dispatch::folder_write_uid("Inbox", 42, archived, sizeof(archived) - 1, false));
  std::string path;
  for (const auto& m : dispatch::load_mail_folder(dispatch::kFolderInbox)) {
    if (m.uid == 42)
      path = m.path;
  }
  CHECK(!path.empty());
  const int archive = dispatch::ensure_archive_folder();
  CHECK(archive >= 0);
  CHECK(dispatch::folder_move(path, archive));
  CHECK(dispatch::folder_deleted_uids("Inbox").count(42) == 0);
  CHECK(dispatch::folder_skip_uids("Inbox").count(42) == 1);
  CHECK(fs::is_regular_file(path));

  CHECK(dispatch::folder_move(path, dispatch::kFolderTrash));
  CHECK(dispatch::folder_deleted_uids("Archive").count(42) == 1);
  CHECK(dispatch::folder_deleted_uids("Trash").count(42) == 0);

  CHECK(dispatch::folder_move(path, dispatch::kFolderInbox));
  CHECK(dispatch::folder_deleted_uids("Trash").count(42) == 0);
  CHECK(dispatch::folder_skip_uids("Trash").count(42) == 1);
  CHECK(dispatch::folder_deleted_uids("Inbox").count(42) == 0);
  CHECK(fs::is_regular_file(path));

  const char stuck_body[] =
      "From: Ada <ada@example.com>\r\n"
      "Subject: Stuck\r\n"
      "Message-ID: <stuck@example.com>\r\n"
      "\r\n"
      "Still here\r\n";
  CHECK(dispatch::folder_write_uid("Inbox", 7, stuck_body, sizeof(stuck_body) - 1, false));
  std::string stuck;
  for (const auto& m : dispatch::load_mail_folder(dispatch::kFolderInbox)) {
    if (m.uid == 7)
      stuck = m.path;
  }
  CHECK(!stuck.empty());
  const auto slash = stuck.rfind('/');
  CHECK(slash != std::string::npos);
  const std::string blocked =
      dir.path() + "/dispatch/mail/Archive/cur/" + stuck.substr(slash + 1);
  CHECK(fs::create_directory(blocked));
  CHECK(!dispatch::folder_move(stuck, archive));
  CHECK(fs::is_regular_file(stuck));
  CHECK(dispatch::folder_deleted_uids("Inbox").count(7) == 0);
  const std::string relocated = dir.path() + "/dispatch/mail/Inbox/.relocated";
  if (fs::is_regular_file(relocated)) {
    std::ifstream in(relocated);
    std::string line;
    while (std::getline(in, line))
      CHECK(line != "7");
  }

  return suite_test::done("trash");
}
