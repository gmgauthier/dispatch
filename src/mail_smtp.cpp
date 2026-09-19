/* SPDX-License-Identifier: Unlicense */

#include "mail_smtp.hpp"
#include "mail_parse.hpp"
#include "mail_store.hpp"

#include <libetpan/libetpan.h>

#include <glibmm/fileutils.h>

namespace dispatch {
namespace {

bool smtp_ok(int r)
{
  return r == MAILSMTP_NO_ERROR;
}

SmtpResult send_one(const SmtpAccount& account, const std::string& from,
                    const std::vector<std::string>& rcpt, const std::string& rfc822)
{
  SmtpResult out;
  if (account.host.empty() || account.user.empty()) {
    out.error = "Set Options → Account… first.";
    return out;
  }
  if (from.empty() || rcpt.empty() || rfc822.empty()) {
    out.error = "Missing From, To, or message.";
    return out;
  }

  mailsmtp* smtp = mailsmtp_new(0, nullptr);
  if (!smtp) {
    out.error = "SMTP session failed";
    return out;
  }
  mailsmtp_set_timeout(smtp, 30);

  int r = 0;
  if (account.starttls) {
    r = mailsmtp_socket_connect(smtp, account.host.c_str(), account.port);
    if (smtp_ok(r))
      r = mailsmtp_init(smtp);
    if (smtp_ok(r))
      r = mailsmtp_socket_starttls(smtp);
    if (smtp_ok(r))
      r = mailsmtp_init(smtp);
  } else {
    r = mailsmtp_ssl_connect(smtp, account.host.c_str(), account.port);
    if (smtp_ok(r))
      r = mailsmtp_init(smtp);
  }
  if (!smtp_ok(r)) {
    out.error = mailsmtp_strerror(r) ? mailsmtp_strerror(r) : "SMTP connect failed";
    mailsmtp_free(smtp);
    return out;
  }

  r = mailesmtp_auth_sasl(smtp, "PLAIN", account.host.c_str(), nullptr, nullptr,
                          account.user.c_str(), account.user.c_str(), account.password.c_str(),
                          nullptr);
  if (!smtp_ok(r))
    r = mailsmtp_auth(smtp, account.user.c_str(), account.password.c_str());
  if (!smtp_ok(r)) {
    out.error = "SMTP login failed";
    mailsmtp_quit(smtp);
    mailsmtp_free(smtp);
    return out;
  }

  r = mailsmtp_mail(smtp, from.c_str());
  if (!smtp_ok(r)) {
    out.error = "SMTP MAIL FROM failed";
    mailsmtp_quit(smtp);
    mailsmtp_free(smtp);
    return out;
  }
  for (const auto& to : rcpt) {
    r = mailsmtp_rcpt(smtp, to.c_str());
    if (!smtp_ok(r)) {
      out.error = "SMTP RCPT TO failed";
      mailsmtp_quit(smtp);
      mailsmtp_free(smtp);
      return out;
    }
  }
  r = mailsmtp_data(smtp);
  if (smtp_ok(r))
    r = mailsmtp_data_message(smtp, rfc822.data(), rfc822.size());
  if (!smtp_ok(r)) {
    out.error = "SMTP DATA failed";
    mailsmtp_quit(smtp);
    mailsmtp_free(smtp);
    return out;
  }

  mailsmtp_quit(smtp);
  mailsmtp_free(smtp);
  return out;
}

}  // namespace

SmtpResult smtp_send(const SmtpAccount& account, const std::string& from,
                     const std::vector<std::string>& rcpt, const std::string& rfc822)
{
  return send_one(account, from, rcpt, rfc822);
}

OutboxFlushResult flush_outbox(const SmtpAccount& account)
{
  OutboxFlushResult out;
  auto items = load_mail_folder(kFolderOutbox);
  for (const auto& m : items) {
    std::string raw;
    try {
      raw = Glib::file_get_contents(m.path);
    } catch (const Glib::Error&) {
      ++out.failed;
      continue;
    }
    std::string from;
    std::vector<std::string> rcpt;
    parse_rfc822_envelope(raw, from, rcpt);
    if (from.empty())
      from = account.user;
    const SmtpResult r = send_one(account, from, rcpt, raw);
    if (!r.error.empty()) {
      ++out.failed;
      if (out.error.empty())
        out.error = r.error;
      continue;
    }
    folder_write(kFolderSent, raw.data(), raw.size(), true);
    folder_remove(m.path);
    ++out.sent;
  }
  return out;
}

}  // namespace dispatch
