/* SPDX-License-Identifier: Unlicense */

#include "mail_parse.hpp"
#include "config.hpp"

#include <gmime/gmime.h>

#include <cctype>
#include <mutex>
#include <vector>

namespace dispatch {
namespace {

std::once_flag gmime_once;

void gmime_start()
{
  g_mime_init();
}

std::string html_escape(const std::string& raw)
{
  std::string out;
  out.reserve(raw.size() + 8);
  for (unsigned char c : raw) {
    if (c == '&')
      out += "&amp;";
    else if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else
      out += static_cast<char>(c);
  }
  return out;
}

void collect_text(GMimeObject* obj, std::string& html, std::string& plain)
{
  if (!obj)
    return;
  if (GMIME_IS_MULTIPART(obj)) {
    auto* mp = GMIME_MULTIPART(obj);
    const int n = g_mime_multipart_get_count(mp);
    for (int i = 0; i < n; ++i)
      collect_text(g_mime_multipart_get_part(mp, i), html, plain);
    return;
  }
  if (GMIME_IS_MESSAGE_PART(obj)) {
    GMimeMessage* sub = g_mime_message_part_get_message(GMIME_MESSAGE_PART(obj));
    if (sub)
      collect_text(g_mime_message_get_mime_part(sub), html, plain);
    return;
  }
  if (!GMIME_IS_TEXT_PART(obj))
    return;
  GMimeContentType* ct = g_mime_object_get_content_type(obj);
  gchar* text = g_mime_text_part_get_text(GMIME_TEXT_PART(obj));
  if (!text)
    return;
  const bool is_html = ct && g_mime_content_type_is_type(ct, "text", "html");
  if (is_html) {
    if (html.empty())
      html = text;
  } else if (ct && g_mime_content_type_is_type(ct, "text", "plain")) {
    if (plain.empty())
      plain = text;
  }
  g_free(text);
}

}  // namespace

void parse_rfc822(const std::string& raw, MailMessage& out)
{
  std::call_once(gmime_once, gmime_start);
  if (raw.empty())
    return;
  GMimeStream* stream =
      g_mime_stream_mem_new_with_buffer(raw.data(), static_cast<gssize>(raw.size()));
  GMimeParser* parser = g_mime_parser_new_with_stream(stream);
  g_object_unref(stream);
  GMimeMessage* msg = g_mime_parser_construct_message(parser, nullptr);
  g_object_unref(parser);
  if (!msg)
    return;

  InternetAddressList* from = g_mime_message_get_from(msg);
  if (from) {
    gchar* s = internet_address_list_to_string(from, nullptr, FALSE);
    if (s) {
      out.from = s;
      g_free(s);
    }
  }
  const char* subj = g_mime_message_get_subject(msg);
  if (subj)
    out.subject = subj;
  GDateTime* dt = g_mime_message_get_date(msg);
  if (dt) {
    gchar* s = g_date_time_format(dt, "%Y-%m-%d %H:%M");
    if (s) {
      out.date = s;
      g_free(s);
    }
  }

  std::string html;
  std::string plain;
  collect_text(g_mime_message_get_mime_part(msg), html, plain);
  if (!html.empty())
    out.html = std::move(html);
  else if (!plain.empty())
    out.html = "<pre>" + html_escape(plain) + "</pre>";
  else
    out.html = "<p>(no text body)</p>";

  g_object_unref(msg);
}

void collect_mailboxes(InternetAddressList* list, std::vector<std::string>& out)
{
  if (!list)
    return;
  const int n = internet_address_list_length(list);
  for (int i = 0; i < n; ++i) {
    InternetAddress* addr = internet_address_list_get_address(list, i);
    if (!addr)
      continue;
    if (INTERNET_ADDRESS_IS_MAILBOX(addr)) {
      const char* mb = internet_address_mailbox_get_addr(INTERNET_ADDRESS_MAILBOX(addr));
      if (mb && *mb)
        out.emplace_back(mb);
    } else if (INTERNET_ADDRESS_IS_GROUP(addr)) {
      collect_mailboxes(internet_address_group_get_members(INTERNET_ADDRESS_GROUP(addr)), out);
    }
  }
}

std::string trim_copy(const std::string& s)
{
  size_t a = 0;
  while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a])))
    ++a;
  size_t b = s.size();
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
    --b;
  return s.substr(a, b - a);
}

std::string extract_addr(const std::string& token)
{
  const auto lt = token.find('<');
  const auto gt = token.rfind('>');
  if (lt != std::string::npos && gt != std::string::npos && gt > lt)
    return trim_copy(token.substr(lt + 1, gt - lt - 1));
  return trim_copy(token);
}

std::vector<std::string> split_addresses(const std::string& raw)
{
  std::vector<std::string> out;
  std::string cur;
  for (char c : raw) {
    if (c == ',' || c == ';') {
      const std::string addr = extract_addr(cur);
      if (!addr.empty())
        out.push_back(addr);
      cur.clear();
    } else {
      cur += c;
    }
  }
  const std::string addr = extract_addr(cur);
  if (!addr.empty())
    out.push_back(addr);
  return out;
}

void parse_rfc822_envelope(const std::string& raw, std::string& from,
                           std::vector<std::string>& rcpt)
{
  std::call_once(gmime_once, gmime_start);
  from.clear();
  rcpt.clear();
  if (raw.empty())
    return;
  GMimeStream* stream =
      g_mime_stream_mem_new_with_buffer(raw.data(), static_cast<gssize>(raw.size()));
  GMimeParser* parser = g_mime_parser_new_with_stream(stream);
  g_object_unref(stream);
  GMimeMessage* msg = g_mime_parser_construct_message(parser, nullptr);
  g_object_unref(parser);
  if (!msg)
    return;
  std::vector<std::string> froms;
  collect_mailboxes(g_mime_message_get_from(msg), froms);
  if (!froms.empty())
    from = froms.front();
  collect_mailboxes(g_mime_message_get_all_recipients(msg), rcpt);
  g_object_unref(msg);
}

std::string build_rfc822(const std::string& from, const std::vector<std::string>& to,
                         const std::vector<std::string>& cc, const std::string& subject,
                         const std::string& body)
{
  std::call_once(gmime_once, gmime_start);
  GMimeMessage* msg = g_mime_message_new(TRUE);
  if (!from.empty())
    g_mime_message_add_mailbox(msg, GMIME_ADDRESS_TYPE_FROM, nullptr, from.c_str());
  for (const auto& a : to) {
    if (!a.empty())
      g_mime_message_add_mailbox(msg, GMIME_ADDRESS_TYPE_TO, nullptr, a.c_str());
  }
  for (const auto& a : cc) {
    if (!a.empty())
      g_mime_message_add_mailbox(msg, GMIME_ADDRESS_TYPE_CC, nullptr, a.c_str());
  }
  g_mime_message_set_subject(msg, subject.c_str(), "UTF-8");
  GDateTime* now = g_date_time_new_now_local();
  g_mime_message_set_date(msg, now);
  g_date_time_unref(now);
  gchar* mid = g_mime_utils_generate_message_id("dispatch.local");
  if (mid) {
    g_mime_message_set_message_id(msg, mid);
    g_free(mid);
  }
  g_mime_object_set_header(GMIME_OBJECT(msg), "User-Agent", "Dispatch/" VERSION, nullptr);

  GMimeTextPart* part = g_mime_text_part_new_with_subtype("plain");
  g_mime_text_part_set_charset(part, "UTF-8");
  g_mime_text_part_set_text(part, body.c_str());
  g_mime_message_set_mime_part(msg, GMIME_OBJECT(part));
  g_object_unref(part);

  GMimeStream* stream = g_mime_stream_mem_new();
  g_mime_object_write_to_stream(GMIME_OBJECT(msg), nullptr, stream);
  g_mime_stream_flush(stream);
  GByteArray* arr = g_mime_stream_mem_get_byte_array(GMIME_STREAM_MEM(stream));
  std::string out;
  if (arr && arr->data && arr->len)
    out.assign(reinterpret_cast<char*>(arr->data), arr->len);
  g_object_unref(stream);
  g_object_unref(msg);
  return out;
}

}  // namespace dispatch
