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
    char tmpl[] = "/tmp/dispatch-folders-XXXXXX";
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

int index_of_imap(const std::string& imap)
{
  return dispatch::find_mail_folder(imap, "");
}

}  // namespace

int main()
{
  TempDir dir;
  CHECK(!dir.path().empty());
  CHECK(setenv("XDG_DATA_HOME", dir.path().c_str(), 1) == 0);

  dispatch::load_mail_folders();
  dispatch::merge_imap_folders({"INBOX", "ZZ", "AA"});

  const int inbox = index_of_imap("INBOX");
  const int aa = index_of_imap("AA");
  const int zz = index_of_imap("ZZ");
  CHECK(inbox == dispatch::kFolderInbox);
  CHECK(aa >= dispatch::kFolderCount);
  CHECK(zz > aa);
  CHECK(dispatch::mail_folder(zz).imap == "ZZ");
  const std::string zz_dir = dispatch::mail_folder(zz).dir;
  const std::string inbox_dir = dispatch::mail_folder(inbox).dir;
  const int old_zz = zz;

  dispatch::merge_imap_folders({"INBOX", "ZZ", "AA", "MM"});

  CHECK(dispatch::mail_folder(dispatch::kFolderInbox).imap == "INBOX");
  CHECK(dispatch::mail_folder(old_zz).imap != "ZZ");
  const int found = dispatch::find_mail_folder("ZZ", zz_dir);
  CHECK(found >= dispatch::kFolderCount);
  CHECK(found != old_zz);
  CHECK(dispatch::mail_folder(found).imap == "ZZ");
  CHECK(dispatch::mail_folder(found).display == "ZZ");
  CHECK(index_of_imap("AA") == aa);
  CHECK(index_of_imap("MM") == old_zz);
  CHECK(dispatch::find_mail_folder("INBOX", inbox_dir) == dispatch::kFolderInbox);
  CHECK(dispatch::find_mail_folder("", zz_dir) == found);
  CHECK(dispatch::find_mail_folder("", inbox_dir) == dispatch::kFolderInbox);
  CHECK(dispatch::find_mail_folder("missing", "no-such-dir") == -1);
  CHECK(dispatch::find_mail_folder("", "") == -1);

  {
    const auto live = dispatch::mail_folder_snapshot();
    const int zz_now = dispatch::find_mail_folder("ZZ", "");
    CHECK(zz_now >= 0);
    const auto planned =
        dispatch::plan_imap_folders(live, {"INBOX", "ZZ", "AA", "MM", "BB"});
    CHECK(dispatch::mail_folder(zz_now).imap == "ZZ");
    CHECK(dispatch::mail_folder_snapshot().size() == live.size());
    int plan_zz = -1;
    int plan_bb = -1;
    for (int i = 0; i < static_cast<int>(planned.size()); ++i) {
      if (planned[static_cast<size_t>(i)].imap == "ZZ")
        plan_zz = i;
      if (planned[static_cast<size_t>(i)].imap == "BB")
        plan_bb = i;
    }
    CHECK(plan_bb >= dispatch::kFolderCount);
    CHECK(plan_zz > plan_bb);
    CHECK(plan_zz != zz_now);
    dispatch::merge_imap_folders({"INBOX", "ZZ", "AA", "MM", "BB"});
    const auto after = dispatch::mail_folder_snapshot();
    CHECK(after.size() == planned.size());
    for (size_t i = 0; i < after.size(); ++i)
      CHECK(after[i].imap == planned[static_cast<size_t>(i)].imap);
    CHECK(dispatch::mail_folder(plan_zz).imap == "ZZ");
  }

  return suite_test::done("folders");
}
