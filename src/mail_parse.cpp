/* SPDX-License-Identifier: Unlicense */

#include "mail_parse.hpp"
#include "compose_format.hpp"
#include "config.hpp"

#include <gmime/gmime.h>

#include <cctype>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>

namespace dispatch {
namespace {

std::once_flag gmime_once;

void gmime_start()
{
  g_mime_init();
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

std::string lower_copy(const std::string& s)
{
  std::string o = s;
  for (char& c : o)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return o;
}

std::string html_to_text(const std::string& html)
{
  std::string out;
  out.reserve(html.size());
  bool in_tag = false;
  std::string tag;
  for (char c : html) {
    if (c == '<') {
      in_tag = true;
      tag.clear();
      continue;
    }
    if (c == '>') {
      in_tag = false;
      const std::string t = lower_copy(tag);
      if (t == "br" || t == "br/" || t == "/p" || t == "/div" || t == "/tr" || t == "/h1" ||
          t == "/h2" || t == "/h3" || t == "p")
        out += '\n';
      continue;
    }
    if (in_tag) {
      tag += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      continue;
    }
    out += c;
  }
  return out;
}

std::string trim_copy_local(const std::string& s)
{
  size_t a = 0;
  while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a])))
    ++a;
  size_t b = s.size();
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
    --b;
  return s.substr(a, b - a);
}

std::string canon_msgid(const std::string& raw)
{
  std::string s = trim_copy_local(raw);
  if (s.size() >= 2 && s.front() == '<' && s.back() == '>')
    s = s.substr(1, s.size() - 2);
  return trim_copy_local(s);
}

std::vector<std::string> parse_msgids(const char* raw)
{
  std::vector<std::string> out;
  if (!raw || !*raw)
    return out;
  const std::string s = raw;
  size_t i = 0;
  while (i < s.size()) {
    const auto lt = s.find('<', i);
    if (lt == std::string::npos)
      break;
    const auto gt = s.find('>', lt + 1);
    if (gt == std::string::npos)
      break;
    std::string id = canon_msgid(s.substr(lt, gt - lt + 1));
    if (!id.empty())
      out.push_back(std::move(id));
    i = gt + 1;
  }
  if (out.empty()) {
    std::string id = canon_msgid(s);
    if (!id.empty())
      out.push_back(std::move(id));
  }
  return out;
}

bool has_thread_prefix(const std::string& subject)
{
  std::string s = trim_copy_local(subject);
  std::string l = lower_copy(s);
  return l.compare(0, 3, "re:") == 0 || l.compare(0, 4, "fwd:") == 0 || l.compare(0, 3, "fw:") == 0;
}

std::string thread_subject_key(const std::string& subject)
{
  std::string s = trim_copy_local(subject);
  for (;;) {
    std::string l = lower_copy(s);
    size_t n = 0;
    if (l.compare(0, 3, "re:") == 0)
      n = 3;
    else if (l.compare(0, 4, "fwd:") == 0)
      n = 4;
    else if (l.compare(0, 3, "fw:") == 0)
      n = 3;
    if (n == 0)
      break;
    s = trim_copy_local(s.substr(n));
  }
  return lower_copy(s);
}

}  // namespace

void collect_mailboxes(InternetAddressList* list, std::vector<std::string>& out);

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
    out.date_unix = g_date_time_to_unix(dt);
    gchar* s = g_date_time_format(dt, "%Y-%m-%d %H:%M");
    if (s) {
      out.date = s;
      g_free(s);
    }
  }

  std::string html;
  std::string plain;
  collect_text(g_mime_message_get_mime_part(msg), html, plain);
  if (!plain.empty())
    out.text = plain;
  else if (!html.empty())
    out.text = html_to_text(html);
  if (!html.empty())
    out.html = std::move(html);
  else if (!plain.empty())
    out.html = plain_to_letter_html(plain);
  else
    out.html = "<p>(no text body)</p>";

  std::vector<std::string> reply;
  collect_mailboxes(g_mime_message_get_reply_to(msg), reply);
  if (reply.empty())
    collect_mailboxes(g_mime_message_get_from(msg), reply);
  if (!reply.empty())
    out.reply_addr = reply.front();

  const char* mid = g_mime_message_get_message_id(msg);
  auto ids = parse_msgids(mid);
  if (!ids.empty())
    out.msgid = ids.front();
  else
    out.msgid = canon_msgid(mid ? mid : "");
  const char* irt = g_mime_object_get_header(GMIME_OBJECT(msg), "In-Reply-To");
  ids = parse_msgids(irt);
  if (!ids.empty())
    out.in_reply_to = ids.front();
  const char* refs = g_mime_object_get_header(GMIME_OBJECT(msg), "References");
  out.references = parse_msgids(refs);

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
                         const std::string& plain, const std::string& html,
                         const std::string& in_reply_to, const std::string& references)
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
  auto angled = [](const std::string& id) -> std::string {
    if (id.empty())
      return {};
    if (id.front() == '<')
      return id;
    return "<" + id + ">";
  };
  if (!in_reply_to.empty())
    g_mime_object_set_header(GMIME_OBJECT(msg), "In-Reply-To", angled(in_reply_to).c_str(),
                             nullptr);
  if (!references.empty())
    g_mime_object_set_header(GMIME_OBJECT(msg), "References", references.c_str(), nullptr);

  GMimeTextPart* plain_part = g_mime_text_part_new_with_subtype("plain");
  g_mime_text_part_set_charset(plain_part, "UTF-8");
  g_mime_text_part_set_text(plain_part, plain.c_str());
  if (html.empty()) {
    g_mime_message_set_mime_part(msg, GMIME_OBJECT(plain_part));
    g_object_unref(plain_part);
  } else {
    GMimeTextPart* html_part = g_mime_text_part_new_with_subtype("html");
    g_mime_text_part_set_charset(html_part, "UTF-8");
    g_mime_text_part_set_text(html_part, html.c_str());
    GMimeMultipart* alt = g_mime_multipart_new_with_subtype("alternative");
    g_mime_multipart_add(alt, GMIME_OBJECT(plain_part));
    g_mime_multipart_add(alt, GMIME_OBJECT(html_part));
    g_object_unref(plain_part);
    g_object_unref(html_part);
    g_mime_message_set_mime_part(msg, GMIME_OBJECT(alt));
    g_object_unref(alt);
  }

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

std::string with_re_prefix(const std::string& subject)
{
  if (subject.size() >= 3) {
    const unsigned char a = static_cast<unsigned char>(subject[0]);
    const unsigned char b = static_cast<unsigned char>(subject[1]);
    const unsigned char c = static_cast<unsigned char>(subject[2]);
    if ((a == 'R' || a == 'r') && (b == 'E' || b == 'e') && c == ':')
      return subject;
  }
  if (subject.empty())
    return "Re: ";
  return "Re: " + subject;
}

std::string with_fwd_prefix(const std::string& subject)
{
  if (subject.size() >= 4) {
    const unsigned char a = static_cast<unsigned char>(subject[0]);
    const unsigned char b = static_cast<unsigned char>(subject[1]);
    const unsigned char c = static_cast<unsigned char>(subject[2]);
    const unsigned char d = static_cast<unsigned char>(subject[3]);
    if ((a == 'F' || a == 'f') && (b == 'W' || b == 'w') && (c == 'D' || c == 'd') && d == ':')
      return subject;
  }
  if (subject.empty())
    return "Fwd: ";
  return "Fwd: " + subject;
}

std::string quote_plain(const std::string& from, const std::string& date, const std::string& text)
{
  std::ostringstream out;
  out << "\n\nOn " << (date.empty() ? "an earlier date" : date);
  if (!from.empty())
    out << ", " << from;
  out << " wrote:\n";
  std::istringstream in(text);
  std::string line;
  bool any = false;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    out << "> " << line << '\n';
    any = true;
  }
  if (!any)
    out << "> \n";
  return out.str();
}

std::vector<int> thread_parents(const std::vector<MailMessage>& items)
{
  const int n = static_cast<int>(items.size());
  std::vector<int> parent(static_cast<size_t>(n), -1);
  std::map<std::string, int> by_id;
  for (int i = 0; i < n; ++i) {
    if (!items[static_cast<size_t>(i)].msgid.empty() &&
        by_id.find(items[static_cast<size_t>(i)].msgid) == by_id.end())
      by_id[items[static_cast<size_t>(i)].msgid] = i;
  }
  for (int i = 0; i < n; ++i) {
    const auto& m = items[static_cast<size_t>(i)];
    std::string pid = m.in_reply_to;
    if (pid.empty() && !m.references.empty())
      pid = m.references.back();
    if (pid.empty())
      continue;
    const auto it = by_id.find(pid);
    if (it != by_id.end() && it->second != i)
      parent[static_cast<size_t>(i)] = it->second;
  }
  for (int i = 0; i < n; ++i) {
    std::vector<char> seen(static_cast<size_t>(n), 0);
    seen[static_cast<size_t>(i)] = 1;
    int p = parent[static_cast<size_t>(i)];
    while (p >= 0) {
      if (seen[static_cast<size_t>(p)]) {
        parent[static_cast<size_t>(i)] = -1;
        break;
      }
      seen[static_cast<size_t>(p)] = 1;
      p = parent[static_cast<size_t>(p)];
    }
  }
  std::map<std::string, std::vector<int>> groups;
  for (int i = 0; i < n; ++i) {
    if (parent[static_cast<size_t>(i)] >= 0)
      continue;
    const std::string key = thread_subject_key(items[static_cast<size_t>(i)].subject);
    if (key.size() < 6)
      continue;
    groups[key].push_back(i);
  }
  for (auto& g : groups) {
    auto& idxs = g.second;
    if (idxs.size() < 2)
      continue;
    bool any_pfx = false;
    for (int i : idxs) {
      if (has_thread_prefix(items[static_cast<size_t>(i)].subject))
        any_pfx = true;
    }
    if (!any_pfx)
      continue;
    int root = idxs.front();
    for (int i : idxs) {
      const bool i_pfx = has_thread_prefix(items[static_cast<size_t>(i)].subject);
      const bool r_pfx = has_thread_prefix(items[static_cast<size_t>(root)].subject);
      if (r_pfx && !i_pfx)
        root = i;
      else if (i_pfx == r_pfx &&
               items[static_cast<size_t>(i)].uid < items[static_cast<size_t>(root)].uid)
        root = i;
    }
    for (int i : idxs) {
      if (i != root && parent[static_cast<size_t>(i)] < 0)
        parent[static_cast<size_t>(i)] = root;
    }
  }
  return parent;
}

}  // namespace dispatch
