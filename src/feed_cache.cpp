/* SPDX-License-Identifier: Unlicense */

#include "feed_cache.hpp"

#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <unistd.h>

namespace dispatch {
namespace {

std::string cache_dir()
{
  const std::string dir = Glib::build_filename(Glib::get_user_data_dir(), "dispatch", "feeds");
  g_mkdir_with_parents(dir.c_str(), 0700);
  return dir;
}

std::string cache_path(const std::string& url)
{
  gchar* sum = g_compute_checksum_for_string(G_CHECKSUM_SHA256, url.c_str(), url.size());
  std::string name = sum ? sum : "feed";
  g_free(sum);
  name += ".xml";
  return Glib::build_filename(cache_dir(), name);
}

std::string node_text(xmlNodePtr n)
{
  if (!n)
    return {};
  xmlChar* v = xmlNodeGetContent(n);
  std::string s = v ? reinterpret_cast<char*>(v) : "";
  xmlFree(v);
  return s;
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

}  // namespace

bool load_feed_cache(const std::string& url, ParsedFeed& out)
{
  out = {};
  const std::string path = cache_path(url);
  if (!Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR))
    return false;
  xmlDocPtr doc = xmlReadFile(path.c_str(), nullptr, XML_PARSE_NONET | XML_PARSE_NOBLANKS);
  if (!doc)
    return false;
  xmlNodePtr root = xmlDocGetRootElement(doc);
  if (!root || !root->name || xmlStrcasecmp(root->name, BAD_CAST "feedcache") != 0) {
    xmlFreeDoc(doc);
    return false;
  }
  out.title = attr(root, "title");
  for (xmlNodePtr n = root->children; n; n = n->next) {
    if (n->type != XML_ELEMENT_NODE || !n->name || xmlStrcasecmp(n->name, BAD_CAST "item") != 0)
      continue;
    Headline h;
    h.date = attr(n, "date");
    h.link = attr(n, "link");
    for (xmlNodePtr c = n->children; c; c = c->next) {
      if (c->type != XML_ELEMENT_NODE || !c->name)
        continue;
      if (xmlStrcasecmp(c->name, BAD_CAST "subject") == 0)
        h.subject = node_text(c);
      else if (xmlStrcasecmp(c->name, BAD_CAST "html") == 0)
        h.html = node_text(c);
      else if (xmlStrcasecmp(c->name, BAD_CAST "enclosure") == 0) {
        Enclosure e;
        e.url = attr(c, "url");
        e.type = attr(c, "type");
        e.title = attr(c, "title");
        if (!e.url.empty())
          h.enclosures.push_back(std::move(e));
      }
    }
    out.items.push_back(std::move(h));
  }
  xmlFreeDoc(doc);
  return !out.items.empty() || !out.title.empty();
}

bool save_feed_cache(const std::string& url, const std::string& title,
                     const std::vector<Headline>& items)
{
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root = xmlNewNode(nullptr, BAD_CAST "feedcache");
  xmlNewProp(root, BAD_CAST "title", BAD_CAST title.c_str());
  xmlNewProp(root, BAD_CAST "url", BAD_CAST url.c_str());
  xmlDocSetRootElement(doc, root);
  for (const auto& h : items) {
    xmlNodePtr item = xmlNewChild(root, nullptr, BAD_CAST "item", nullptr);
    xmlNewProp(item, BAD_CAST "date", BAD_CAST h.date.c_str());
    xmlNewProp(item, BAD_CAST "link", BAD_CAST h.link.c_str());
    xmlNewTextChild(item, nullptr, BAD_CAST "subject", BAD_CAST h.subject.c_str());
    xmlNewTextChild(item, nullptr, BAD_CAST "html", BAD_CAST h.html.c_str());
    for (const auto& e : h.enclosures) {
      xmlNodePtr enc = xmlNewChild(item, nullptr, BAD_CAST "enclosure", nullptr);
      xmlNewProp(enc, BAD_CAST "url", BAD_CAST e.url.c_str());
      xmlNewProp(enc, BAD_CAST "type", BAD_CAST e.type.c_str());
      xmlNewProp(enc, BAD_CAST "title", BAD_CAST e.title.c_str());
    }
  }
  const std::string path = cache_path(url);
  const std::string tmp = path + ".tmp";
  const int rc = xmlSaveFormatFileEnc(tmp.c_str(), doc, "UTF-8", 1);
  xmlFreeDoc(doc);
  if (rc < 0) {
    ::unlink(tmp.c_str());
    return false;
  }
  if (std::rename(tmp.c_str(), path.c_str()) != 0) {
    ::unlink(tmp.c_str());
    return false;
  }
  return true;
}

void delete_feed_cache(const std::string& url)
{
  ::unlink(cache_path(url).c_str());
}

}  // namespace dispatch
