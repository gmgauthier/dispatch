/* SPDX-License-Identifier: Unlicense */

#include "ephemeris_contacts.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <cctype>

namespace dispatch {
namespace {

void trim(std::string& s)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
}

std::string attr(xmlNodePtr n, const char* name)
{
  if (!n)
    return {};
  xmlChar* v = xmlGetProp(n, BAD_CAST name);
  std::string s = v ? reinterpret_cast<char*>(v) : "";
  xmlFree(v);
  trim(s);
  return s;
}

std::string ephemeris_binder_path()
{
  const std::string ini =
      Glib::build_filename(Glib::get_user_config_dir(), "ephemeris", "ephemeris.ini");
  Glib::KeyFile kf;
  try {
    kf.load_from_file(ini);
  } catch (const Glib::Error&) {
    return {};
  }
  try {
    if (kf.has_key("session", "path"))
      return kf.get_string("session", "path");
  } catch (const Glib::Error&) {
  }
  return {};
}

}  // namespace

std::vector<EphemerisContact> load_ephemeris_emails()
{
  std::vector<EphemerisContact> out;
  const std::string path = ephemeris_binder_path();
  if (path.empty() || !Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR))
    return out;
  xmlDoc* doc = xmlReadFile(path.c_str(), nullptr, XML_PARSE_NONET | XML_PARSE_NOBLANKS);
  if (!doc)
    return out;
  xmlNode* root = xmlDocGetRootElement(doc);
  if (!root || !root->name || xmlStrcasecmp(root->name, BAD_CAST "ephemeris") != 0) {
    xmlFreeDoc(doc);
    return out;
  }
  for (xmlNode* n = root->children; n; n = n->next) {
    if (n->type != XML_ELEMENT_NODE || !n->name || xmlStrcasecmp(n->name, BAD_CAST "contact") != 0)
      continue;
    EphemerisContact c;
    c.email = attr(n, "email");
    if (c.email.empty())
      continue;
    const std::string first = attr(n, "first");
    const std::string last = attr(n, "last");
    if (!first.empty() && !last.empty())
      c.name = first + " " + last;
    else if (!last.empty())
      c.name = last;
    else
      c.name = first;
    out.push_back(std::move(c));
  }
  xmlFreeDoc(doc);
  std::sort(out.begin(), out.end(), [](const EphemerisContact& a, const EphemerisContact& b) {
    if (a.name != b.name)
      return a.name < b.name;
    return a.email < b.email;
  });
  return out;
}

}  // namespace dispatch
