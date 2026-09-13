/* SPDX-License-Identifier: Unlicense */

#include "feed.hpp"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cctype>

namespace dispatch {
namespace {

bool ieq(const xmlChar* a, const char* b)
{
  return a && b && xmlStrcasecmp(a, BAD_CAST b) == 0;
}

xmlNodePtr first_elem(xmlNodePtr n, const char* local)
{
  for (xmlNodePtr c = n ? n->children : nullptr; c; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && ieq(c->name, local))
      return c;
  }
  return nullptr;
}

std::string node_text(xmlNodePtr n)
{
  if (!n)
    return {};
  xmlChar* t = xmlNodeGetContent(n);
  std::string s = t ? reinterpret_cast<char*>(t) : "";
  xmlFree(t);
  return s;
}

std::string node_inner_xml(xmlNodePtr n)
{
  if (!n)
    return {};
  bool has_elem = false;
  for (xmlNodePtr c = n->children; c; c = c->next) {
    if (c->type == XML_ELEMENT_NODE) {
      has_elem = true;
      break;
    }
  }
  if (!has_elem)
    return node_text(n);
  xmlBufferPtr buf = xmlBufferCreate();
  if (!buf)
    return node_text(n);
  for (xmlNodePtr c = n->children; c; c = c->next)
    xmlNodeDump(buf, n->doc, c, 0, 0);
  const xmlChar* content = xmlBufferContent(buf);
  std::string s = content ? reinterpret_cast<const char*>(content) : "";
  xmlBufferFree(buf);
  return s;
}

std::string child_text(xmlNodePtr n, const char* local)
{
  return node_text(first_elem(n, local));
}

std::string attr(xmlNodePtr n, const char* name)
{
  if (!n)
    return {};
  xmlChar* v = xmlGetProp(n, BAD_CAST name);
  std::string s = v ? reinterpret_cast<char*>(v) : "";
  xmlFree(v);
  return s;
}

std::string atom_link(xmlNodePtr entry)
{
  std::string alt;
  std::string any;
  for (xmlNodePtr c = entry ? entry->children : nullptr; c; c = c->next) {
    if (c->type != XML_ELEMENT_NODE || !ieq(c->name, "link"))
      continue;
    const std::string href = attr(c, "href");
    if (href.empty())
      continue;
    const std::string rel = attr(c, "rel");
    if (rel.empty() || rel == "alternate") {
      if (alt.empty())
        alt = href;
    } else if (any.empty()) {
      any = href;
    }
  }
  return alt.empty() ? any : alt;
}

std::string item_body(xmlNodePtr item)
{
  std::string encoded, description, content, summary;
  for (xmlNodePtr c = item ? item->children : nullptr; c; c = c->next) {
    if (c->type != XML_ELEMENT_NODE)
      continue;
    if (ieq(c->name, "encoded"))
      encoded = node_inner_xml(c);
    else if (ieq(c->name, "description"))
      description = node_inner_xml(c);
    else if (ieq(c->name, "content"))
      content = node_inner_xml(c);
    else if (ieq(c->name, "summary"))
      summary = node_inner_xml(c);
  }
  encoded = html_to_text(encoded);
  content = html_to_text(content);
  description = html_to_text(description);
  summary = html_to_text(summary);

  std::string best;
  auto consider = [&](const std::string& t) {
    if (t.size() > best.size())
      best = t;
  };
  consider(summary);
  consider(description);
  consider(content);
  consider(encoded);
  return best;
}

std::string item_date(xmlNodePtr item)
{
  const char* keys[] = {"pubDate", "updated", "published", "date", nullptr};
  for (int i = 0; keys[i]; ++i) {
    const std::string d = child_text(item, keys[i]);
    if (!d.empty())
      return short_date(d);
  }
  return {};
}

void add_item(ParsedFeed& out, xmlNodePtr item)
{
  Headline h;
  h.subject = html_to_text(child_text(item, "title"));
  h.date = item_date(item);
  h.body = item_body(item);
  h.link = child_text(item, "link");
  if (h.link.empty())
    h.link = atom_link(item);
  if (h.subject.empty() && h.body.empty())
    return;
  if (h.subject.empty())
    h.subject = "(no title)";
  out.items.push_back(std::move(h));
}

void parse_rss(xmlNodePtr root, ParsedFeed& out)
{
  xmlNodePtr channel = first_elem(root, "channel");
  if (!channel)
    channel = root;
  out.title = html_to_text(child_text(channel, "title"));
  for (xmlNodePtr c = channel->children; c; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && ieq(c->name, "item"))
      add_item(out, c);
  }
}

void parse_rdf(xmlNodePtr root, ParsedFeed& out)
{
  out.title = html_to_text(child_text(first_elem(root, "channel"), "title"));
  if (out.title.empty())
    out.title = html_to_text(child_text(root, "title"));
  for (xmlNodePtr c = root->children; c; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && ieq(c->name, "item"))
      add_item(out, c);
  }
}

void parse_atom(xmlNodePtr root, ParsedFeed& out)
{
  out.title = html_to_text(child_text(root, "title"));
  for (xmlNodePtr c = root->children; c; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && ieq(c->name, "entry"))
      add_item(out, c);
  }
}

std::string ascii_lower(std::string s)
{
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

}  // namespace

std::string html_to_text(const std::string& html)
{
  std::string out;
  out.reserve(html.size());
  bool in_tag = false;
  bool in_ent = false;
  bool skip = false;
  std::string ent;
  std::string tag;
  auto emit = [&](char ch) {
    if (skip)
      return;
    if (ch == '\r')
      return;
    if (ch == '\n' || ch == '\t')
      ch = ' ';
    if (ch == ' ' && !out.empty() && out.back() == ' ')
      return;
    out.push_back(ch);
  };
  auto emit_nl = [&]() {
    if (skip)
      return;
    while (!out.empty() && out.back() == ' ')
      out.pop_back();
    if (out.empty() || out.back() == '\n')
      return;
    out.push_back('\n');
  };
  auto decode_ent = [&](const std::string& e) -> std::string {
    if (e == "amp")
      return "&";
    if (e == "lt")
      return "<";
    if (e == "gt")
      return ">";
    if (e == "quot" || e == "ldquo" || e == "rdquo")
      return "\"";
    if (e == "apos" || e == "lsquo" || e == "rsquo")
      return "'";
    if (e == "nbsp")
      return " ";
    if (e == "mdash" || e == "ndash")
      return "-";
    if (e == "hellip")
      return "...";
    if (!e.empty() && e[0] == '#') {
      int n = 0;
      if (e.size() > 1 && (e[1] == 'x' || e[1] == 'X')) {
        for (size_t i = 2; i < e.size(); ++i) {
          const char c = e[i];
          n *= 16;
          if (c >= '0' && c <= '9')
            n += c - '0';
          else if (c >= 'a' && c <= 'f')
            n += 10 + c - 'a';
          else if (c >= 'A' && c <= 'F')
            n += 10 + c - 'A';
        }
      } else {
        for (size_t i = 1; i < e.size(); ++i)
          if (e[i] >= '0' && e[i] <= '9')
            n = n * 10 + (e[i] - '0');
      }
      if (n <= 0)
        return {};
      if (n < 128)
        return std::string(1, static_cast<char>(n));
      /* UTF-8 BMP */
      if (n < 0x800) {
        char buf[2] = {static_cast<char>(0xC0 | (n >> 6)),
                       static_cast<char>(0x80 | (n & 0x3F))};
        return std::string(buf, 2);
      }
      char buf[3] = {static_cast<char>(0xE0 | (n >> 12)),
                     static_cast<char>(0x80 | ((n >> 6) & 0x3F)),
                     static_cast<char>(0x80 | (n & 0x3F))};
      return std::string(buf, 3);
    }
    return {};
  };

  for (size_t i = 0; i < html.size(); ++i) {
    const char ch = html[i];
    if (in_ent) {
      if (ch == ';') {
        const std::string d = decode_ent(ent);
        for (char c : d)
          emit(c);
        if (d.empty()) {
          emit('&');
          for (char c : ent)
            emit(c);
          emit(';');
        }
        in_ent = false;
        ent.clear();
      } else if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '#') {
        if (ent.size() < 12)
          ent.push_back(ch);
      } else {
        emit('&');
        for (char c : ent)
          emit(c);
        in_ent = false;
        ent.clear();
        --i;
      }
      continue;
    }
    if (in_tag) {
      if (ch == '>') {
        in_tag = false;
        std::string t = ascii_lower(tag);
        if (!t.empty() && t[0] == '/')
          t = t.substr(1);
        const bool closing = !tag.empty() && tag[0] == '/';
        if (t == "script" || t == "style" || t == "noscript" || t == "svg")
          skip = !closing;
        else if (t == "br" || t == "br/" || t == "p" || t == "div" || t == "li" || t == "tr" ||
                 t == "h1" || t == "h2" || t == "h3" || t == "h4" || t == "blockquote" ||
                 t == "pre" || t == "section" || t == "article")
          emit_nl();
        tag.clear();
      } else if (tag.size() < 24 && ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
        tag.push_back(ch);
      }
      continue;
    }
    if (ch == '<') {
      in_tag = true;
      tag.clear();
      continue;
    }
    if (ch == '&') {
      in_ent = true;
      ent.clear();
      continue;
    }
    emit(ch);
  }
  while (!out.empty() && (out.back() == ' ' || out.back() == '\n'))
    out.pop_back();
  return out;
}

std::string short_date(const std::string& raw)
{
  std::string s = raw;
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  if (s.size() >= 10 && std::isdigit(static_cast<unsigned char>(s[0])) && s[4] == '-' &&
      s[7] == '-')
    return s.substr(0, 10);
  const auto comma = s.find(',');
  std::string rest = comma == std::string::npos ? s : s.substr(comma + 1);
  while (!rest.empty() && rest.front() == ' ')
    rest.erase(rest.begin());
  int tokens = 0;
  size_t i = 0;
  while (i < rest.size() && tokens < 3) {
    if (rest[i] == ' ') {
      ++tokens;
      while (i < rest.size() && rest[i] == ' ')
        ++i;
    } else {
      ++i;
    }
  }
  if (tokens >= 2)
    return rest.substr(0, i);
  if (rest.size() > 16)
    return rest.substr(0, 16);
  return rest;
}

ParsedFeed parse_feed(const std::string& xml, const std::string& fallback_title)
{
  ParsedFeed out;
  if (xml.empty()) {
    out.error = "Empty document";
    return out;
  }
  xmlDocPtr doc = xmlReadMemory(xml.data(), static_cast<int>(xml.size()), fallback_title.c_str(),
                                nullptr, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING |
                                             XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (!doc) {
    out.error = "Not XML (RSS or Atom)";
    return out;
  }
  xmlNodePtr root = xmlDocGetRootElement(doc);
  if (!root) {
    xmlFreeDoc(doc);
    out.error = "Empty XML";
    return out;
  }
  if (ieq(root->name, "rss"))
    parse_rss(root, out);
  else if (ieq(root->name, "RDF") || ieq(root->name, "rdf"))
    parse_rdf(root, out);
  else if (ieq(root->name, "feed"))
    parse_atom(root, out);
  else
    out.error = "Not an RSS or Atom feed";

  xmlFreeDoc(doc);
  if (out.error.empty() && out.items.empty() && out.title.empty())
    out.error = "No items in feed";
  if (out.title.empty())
    out.title = fallback_title;
  return out;
}

}  // namespace dispatch
