/* SPDX-License-Identifier: Unlicense */

#include "mail_parse.hpp"

#include <gmime/gmime.h>

#include <mutex>

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

}  // namespace dispatch
