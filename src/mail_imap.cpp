/* SPDX-License-Identifier: Unlicense */

#include "mail_imap.hpp"
#include "mail_store.hpp"

#include <libetpan/libetpan.h>
#include <libetpan/uidplus.h>

#include <set>
#include <vector>

namespace dispatch {
namespace {

bool imap_ok(int r)
{
  return r == MAILIMAP_NO_ERROR || r == MAILIMAP_NO_ERROR_AUTHENTICATED ||
         r == MAILIMAP_NO_ERROR_NON_AUTHENTICATED;
}

const char* imap_err(int r)
{
  switch (r) {
    case MAILIMAP_ERROR_CONNECTION_REFUSED:
      return "connection refused";
    case MAILIMAP_ERROR_STREAM:
      return "connection lost";
    case MAILIMAP_ERROR_SSL:
      return "TLS failed";
    case MAILIMAP_ERROR_STARTTLS:
      return "STARTTLS failed";
    case MAILIMAP_ERROR_LOGIN:
      return "login failed";
    case MAILIMAP_ERROR_PROTOCOL:
      return "protocol error";
    case MAILIMAP_ERROR_PARSE:
      return "parse error";
    default:
      return "IMAP error";
  }
}

bool flag_seen(struct mailimap_msg_att_dynamic* dyn)
{
  if (!dyn || !dyn->att_list)
    return false;
  for (clistiter* cur = clist_begin(dyn->att_list); cur; cur = clist_next(cur)) {
    auto* ff = static_cast<mailimap_flag_fetch*>(clist_content(cur));
    if (!ff || ff->fl_type != MAILIMAP_FLAG_FETCH_OTHER || !ff->fl_flag)
      continue;
    if (ff->fl_flag->fl_type == MAILIMAP_FLAG_SEEN)
      return true;
  }
  return false;
}

bool mailbox_noselect(mailimap_mailbox_list* mb)
{
  if (!mb || !mb->mb_flag)
    return false;
  auto* fl = mb->mb_flag;
  if (fl->mbf_type == MAILIMAP_MBX_LIST_FLAGS_SFLAG &&
      fl->mbf_sflag == MAILIMAP_MBX_LIST_SFLAG_NOSELECT)
    return true;
  return false;
}

mailimap_set* uid_set(const std::vector<uint32_t>& uids)
{
  mailimap_set* set = mailimap_set_new_empty();
  if (!set)
    return nullptr;
  for (uint32_t uid : uids)
    mailimap_set_add_single(set, uid);
  return set;
}

bool store_flags(mailimap* imap, const std::vector<uint32_t>& uids, bool add_seen)
{
  if (!imap || uids.empty())
    return true;
  mailimap_set* set = uid_set(uids);
  if (!set)
    return false;
  mailimap_flag_list* flags = mailimap_flag_list_new_empty();
  mailimap_flag_list_add(flags, mailimap_flag_new_seen());
  mailimap_store_att_flags* store = add_seen ? mailimap_store_att_flags_new_add_flags(flags)
                                             : mailimap_store_att_flags_new_remove_flags(flags);
  const int r = mailimap_uid_store(imap, set, store);
  mailimap_store_att_flags_free(store);
  mailimap_set_free(set);
  return imap_ok(r);
}

void push_local_flags(mailimap* imap, const std::string& dir, const std::set<uint32_t>& on_server)
{
  std::vector<uint32_t> seen;
  std::vector<uint32_t> unseen;
  for (const auto& item : folder_uid_seen(dir)) {
    if (!on_server.count(item.first))
      continue;
    if (item.second)
      seen.push_back(item.first);
    else
      unseen.push_back(item.first);
  }
  store_flags(imap, seen, true);
  store_flags(imap, unseen, false);
}

std::string trash_imap_name()
{
  const int n = mail_folder_count();
  for (int i = 0; i < n; ++i) {
    const auto& f = mail_folder(i);
    if (f.role == kFolderTrash)
      return f.imap;
  }
  return {};
}

bool expunge_uids(mailimap* imap, const std::vector<uint32_t>& uids)
{
  if (!imap || uids.empty())
    return true;
  mailimap_set* set = uid_set(uids);
  if (!set)
    return false;
  mailimap_flag_list* flags = mailimap_flag_list_new_empty();
  mailimap_flag_list_add(flags, mailimap_flag_new_deleted());
  mailimap_store_att_flags* store = mailimap_store_att_flags_new_add_flags(flags);
  int r = mailimap_uid_store(imap, set, store);
  mailimap_store_att_flags_free(store);
  if (imap_ok(r))
    r = mailimap_uid_expunge(imap, set);
  if (!imap_ok(r))
    r = mailimap_expunge(imap);
  mailimap_set_free(set);
  return imap_ok(r);
}

bool move_uids_to_trash(mailimap* imap, const std::vector<uint32_t>& uids,
                        const std::string& trash_imap)
{
  if (!imap || uids.empty())
    return true;
  mailimap_set* set = uid_set(uids);
  if (!set)
    return false;
  int r = MAILIMAP_ERROR_BAD_STATE;
  if (!trash_imap.empty())
    r = mailimap_uid_move(imap, set, trash_imap.c_str());
  if (!imap_ok(r) && !trash_imap.empty()) {
    r = mailimap_uid_copy(imap, set, trash_imap.c_str());
    if (imap_ok(r)) {
      mailimap_set_free(set);
      return expunge_uids(imap, uids);
    }
  }
  mailimap_set_free(set);
  if (imap_ok(r))
    return true;
  return expunge_uids(imap, uids);
}

void take_att(mailimap_msg_att* att, uint32_t& uid, const char*& body, size_t& len, bool& seen)
{
  uid = 0;
  body = nullptr;
  len = 0;
  seen = false;
  if (!att || !att->att_list)
    return;
  for (clistiter* cur = clist_begin(att->att_list); cur; cur = clist_next(cur)) {
    auto* item = static_cast<mailimap_msg_att_item*>(clist_content(cur));
    if (!item)
      continue;
    if (item->att_type == MAILIMAP_MSG_ATT_ITEM_DYNAMIC)
      seen = flag_seen(item->att_data.att_dyn);
    if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC || !item->att_data.att_static)
      continue;
    auto* st = item->att_data.att_static;
    if (st->att_type == MAILIMAP_MSG_ATT_UID)
      uid = st->att_data.att_uid;
    else if (st->att_type == MAILIMAP_MSG_ATT_BODY_SECTION && st->att_data.att_body_section) {
      body = st->att_data.att_body_section->sec_body_part;
      len = st->att_data.att_body_section->sec_length;
    } else if (st->att_type == MAILIMAP_MSG_ATT_RFC822) {
      body = st->att_data.att_rfc822.att_content;
      len = st->att_data.att_rfc822.att_length;
    }
  }
}

int sync_one(mailimap* imap, const std::string& imap_name, const std::string& dir, int role,
             MailSyncResult& out)
{
  int r = mailimap_select(imap, imap_name.c_str());
  if (!imap_ok(r) || !imap->imap_selection_info)
    return r;
  const uint32_t uidvalidity = imap->imap_selection_info->sel_uidvalidity;
  const uint32_t local_uv = folder_uidvalidity(dir);
  if (uidvalidity != 0 && local_uv == 0) {
    folder_set_uidvalidity(dir, uidvalidity);
  } else if (uidvalidity != 0 && local_uv != 0 && uidvalidity != local_uv) {
    folder_wipe(dir);
    folder_set_uidvalidity(dir, uidvalidity);
  }

  mailimap_set* all = mailimap_set_new_interval(1, 0);
  mailimap_fetch_type* flag_type = mailimap_fetch_type_new_fetch_att_list_empty();
  mailimap_fetch_type_new_fetch_att_list_add(flag_type, mailimap_fetch_att_new_uid());
  mailimap_fetch_type_new_fetch_att_list_add(flag_type, mailimap_fetch_att_new_flags());
  clist* flag_result = nullptr;
  r = mailimap_uid_fetch(imap, all, flag_type, &flag_result);
  mailimap_fetch_type_free(flag_type);
  mailimap_set_free(all);
  if (!imap_ok(r)) {
    if (flag_result)
      mailimap_fetch_list_free(flag_result);
    return r;
  }
  std::set<uint32_t> on_server;
  if (flag_result) {
    for (clistiter* cur = clist_begin(flag_result); cur; cur = clist_next(cur)) {
      auto* att = static_cast<mailimap_msg_att*>(clist_content(cur));
      uint32_t uid = 0;
      const char* body = nullptr;
      size_t len = 0;
      bool seen = false;
      take_att(att, uid, body, len, seen);
      if (uid)
        on_server.insert(uid);
    }
    mailimap_fetch_list_free(flag_result);
  }

  std::vector<uint32_t> gone;
  for (uint32_t uid : folder_deleted_uids(dir)) {
    if (on_server.count(uid))
      gone.push_back(uid);
  }
  if (!gone.empty()) {
    bool ok = false;
    if (role == kFolderTrash)
      ok = expunge_uids(imap, gone);
    else
      ok = move_uids_to_trash(imap, gone, trash_imap_name());
    if (ok) {
      std::set<uint32_t> done(gone.begin(), gone.end());
      folder_drop_deleted(dir, done);
      for (uint32_t u : gone)
        on_server.erase(u);
    }
  }

  push_local_flags(imap, dir, on_server);

  const std::set<uint32_t> have = folder_skip_uids(dir);
  std::vector<uint32_t> want;
  for (uint32_t uid : on_server) {
    ++out.total;
    if (!have.count(uid))
      want.push_back(uid);
  }
  if (want.empty()) {
    ++out.folders;
    return MAILIMAP_NO_ERROR;
  }

  mailimap_set* set = uid_set(want);
  mailimap_fetch_type* ftype = mailimap_fetch_type_new_fetch_att_list_empty();
  mailimap_fetch_type_new_fetch_att_list_add(ftype, mailimap_fetch_att_new_uid());
  mailimap_fetch_type_new_fetch_att_list_add(ftype, mailimap_fetch_att_new_flags());
  mailimap_fetch_type_new_fetch_att_list_add(ftype,
                                             mailimap_fetch_att_new_body_peek_section(nullptr));
  clist* fetch_result = nullptr;
  r = mailimap_uid_fetch(imap, set, ftype, &fetch_result);
  mailimap_fetch_type_free(ftype);
  mailimap_set_free(set);
  if (!imap_ok(r)) {
    if (fetch_result)
      mailimap_fetch_list_free(fetch_result);
    return r;
  }
  if (fetch_result) {
    for (clistiter* cur = clist_begin(fetch_result); cur; cur = clist_next(cur)) {
      auto* att = static_cast<mailimap_msg_att*>(clist_content(cur));
      uint32_t uid = 0;
      const char* body = nullptr;
      size_t len = 0;
      bool seen = false;
      take_att(att, uid, body, len, seen);
      if (uid == 0 || !body || len == 0 || len > 8u * 1024u * 1024u)
        continue;
      if (folder_write_uid(dir, uid, body, len, seen))
        ++out.downloaded;
    }
    mailimap_fetch_list_free(fetch_result);
  }
  ++out.folders;
  return MAILIMAP_NO_ERROR;
}

}  // namespace

MailSyncResult sync_mailboxes(const ImapAccount& account)
{
  MailSyncResult out;
  if (account.host.empty() || account.user.empty()) {
    out.error = "Set Options → Account… first.";
    return out;
  }
  ensure_maildirs();

  mailimap* imap = mailimap_new(0, nullptr);
  if (!imap) {
    out.error = "IMAP session failed";
    return out;
  }
  mailimap_set_timeout(imap, 30);

  int r = 0;
  if (account.starttls) {
    r = mailimap_socket_connect(imap, account.host.c_str(), account.port);
    if (imap_ok(r))
      r = mailimap_socket_starttls(imap);
  } else {
    r = mailimap_ssl_connect(imap, account.host.c_str(), account.port);
  }
  if (!imap_ok(r)) {
    out.error = imap_err(r);
    mailimap_free(imap);
    return out;
  }
  r = mailimap_login(imap, account.user.c_str(), account.password.c_str());
  if (!imap_ok(r)) {
    out.error = "login failed";
    mailimap_logout(imap);
    mailimap_free(imap);
    return out;
  }

  clist* listed = nullptr;
  r = mailimap_list(imap, "", "*", &listed);
  std::vector<std::string> names;
  if (imap_ok(r) && listed) {
    for (clistiter* cur = clist_begin(listed); cur; cur = clist_next(cur)) {
      auto* mb = static_cast<mailimap_mailbox_list*>(clist_content(cur));
      if (!mb || !mb->mb_name || mailbox_noselect(mb))
        continue;
      names.emplace_back(mb->mb_name);
    }
    mailimap_list_result_free(listed);
  }
  if (!names.empty())
    merge_imap_folders(names);
  ensure_maildirs();

  const int n = mail_folder_count();
  for (int i = 0; i < n; ++i) {
    const auto& f = mail_folder(i);
    if (f.imap.empty())
      continue;
    r = sync_one(imap, f.imap, f.dir, f.role, out);
    if (!imap_ok(r) && out.folders == 0)
      out.error = std::string("cannot select ") + f.display;
  }

  mailimap_logout(imap);
  mailimap_free(imap);
  if (out.folders == 0 && out.error.empty() && !names.empty())
    out.error = "no selectable IMAP folders";
  return out;
}

}  // namespace dispatch
