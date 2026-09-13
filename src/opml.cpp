/* SPDX-License-Identifier: Unlicense */

#include "opml.hpp"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cctype>

namespace dispatch {
namespace {

bool ieq(const xmlChar* a, const char* b)
{
  return a && b && xmlStrcasecmp(a, BAD_CAST b) == 0;
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

void trim(std::string& s)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
}

void walk_outlines(xmlNodePtr n, std::vector<OpmlOutline>& out)
{
  for (xmlNodePtr c = n; c; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && ieq(c->name, "outline")) {
      std::string url = attr(c, "xmlUrl");
      if (url.empty())
        url = attr(c, "xmlurl");
      if (url.empty())
        url = attr(c, "url");
      trim(url);
      if (!url.empty()) {
        std::string title = attr(c, "title");
        if (title.empty())
          title = attr(c, "text");
        trim(title);
        out.push_back({url, title});
      }
    }
    if (c->children)
      walk_outlines(c->children, out);
  }
}

}  // namespace

std::vector<OpmlOutline> parse_opml_file(const std::string& path, std::string& error)
{
  error.clear();
  std::vector<OpmlOutline> out;
  xmlDocPtr doc = xmlReadFile(path.c_str(), nullptr,
                              XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING |
                                  XML_PARSE_RECOVER);
  if (!doc) {
    error = "Not OPML";
    return out;
  }
  xmlNodePtr root = xmlDocGetRootElement(doc);
  if (!root || !ieq(root->name, "opml")) {
    xmlFreeDoc(doc);
    error = "Not OPML";
    return out;
  }
  walk_outlines(root->children, out);
  xmlFreeDoc(doc);
  if (out.empty())
    error = "No feed URLs in OPML";
  return out;
}

bool write_opml_file(const std::string& path, const std::vector<OpmlOutline>& outlines,
                     std::string& error)
{
  error.clear();
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr opml = xmlNewNode(nullptr, BAD_CAST "opml");
  xmlNewProp(opml, BAD_CAST "version", BAD_CAST "2.0");
  xmlDocSetRootElement(doc, opml);

  xmlNodePtr head = xmlNewChild(opml, nullptr, BAD_CAST "head", nullptr);
  xmlNewChild(head, nullptr, BAD_CAST "title", BAD_CAST "Dispatch subscriptions");
  xmlNodePtr body = xmlNewChild(opml, nullptr, BAD_CAST "body", nullptr);

  for (const auto& o : outlines) {
    xmlNodePtr outline = xmlNewChild(body, nullptr, BAD_CAST "outline", nullptr);
    const char* title = o.title.empty() ? o.url.c_str() : o.title.c_str();
    xmlNewProp(outline, BAD_CAST "text", BAD_CAST title);
    xmlNewProp(outline, BAD_CAST "title", BAD_CAST title);
    xmlNewProp(outline, BAD_CAST "type", BAD_CAST "rss");
    xmlNewProp(outline, BAD_CAST "xmlUrl", BAD_CAST o.url.c_str());
  }

  const int rc = xmlSaveFormatFileEnc(path.c_str(), doc, "UTF-8", 1);
  xmlFreeDoc(doc);
  if (rc < 0) {
    error = "Could not write file";
    return false;
  }
  return true;
}

}  // namespace dispatch
