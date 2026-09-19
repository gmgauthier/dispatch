/* SPDX-License-Identifier: Unlicense */

#include "mail_imap.hpp"
#include "mail_store.hpp"

#include <libetpan/libetpan.h>

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

}  // namespace

InboxSyncResult sync_inbox(const ImapAccount& account)
{
  InboxSyncResult out;
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

  r = mailimap_select(imap, "INBOX");
  if (!imap_ok(r) || !imap->imap_selection_info) {
    out.error = "cannot select INBOX";
    mailimap_logout(imap);
    mailimap_free(imap);
    return out;
  }

  const uint32_t uidvalidity = imap->imap_selection_info->sel_uidvalidity;
  if (uidvalidity != 0 && uidvalidity != inbox_uidvalidity()) {
    inbox_wipe();
    inbox_set_uidvalidity(uidvalidity);
  } else if (inbox_uidvalidity() == 0 && uidvalidity != 0) {
    inbox_set_uidvalidity(uidvalidity);
  }

  const std::set<uint32_t> have = inbox_uids();

  mailimap_set* set = mailimap_set_new_interval(1, 0);
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
    out.error = imap_err(r);
    if (fetch_result)
      mailimap_fetch_list_free(fetch_result);
    mailimap_logout(imap);
    mailimap_free(imap);
    return out;
  }

  int downloaded = 0;
  int total = 0;
  if (fetch_result) {
    for (clistiter* cur = clist_begin(fetch_result); cur; cur = clist_next(cur)) {
      auto* att = static_cast<mailimap_msg_att*>(clist_content(cur));
      uint32_t uid = 0;
      const char* body = nullptr;
      size_t len = 0;
      bool seen = false;
      take_att(att, uid, body, len, seen);
      if (uid == 0)
        continue;
      ++total;
      if (have.count(uid))
        continue;
      if (!body || len == 0)
        continue;
      if (len > 8u * 1024u * 1024u)
        continue;
      if (inbox_write(uid, body, len, seen))
        ++downloaded;
    }
    mailimap_fetch_list_free(fetch_result);
  }

  mailimap_logout(imap);
  mailimap_free(imap);
  out.downloaded = downloaded;
  out.total = total > 0 ? total : static_cast<int>(have.size());
  if (out.total == 0)
    out.total = static_cast<int>(inbox_uids().size());
  return out;
}

}  // namespace dispatch
