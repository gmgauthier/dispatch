/* SPDX-License-Identifier: Unlicense */

#include "check.hpp"
#include "mail_store.hpp"

#include <filesystem>
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

  return suite_test::done("trash");
}
