/* SPDX-License-Identifier: Unlicense */

#include "body_view.hpp"
#include "check.hpp"

#include <string>

int main()
{
  const std::string ini = "file:///home/USER/.config/dispatch/dispatch.ini";
  CHECK(dispatch::resolve_link(ini, "").empty());
  CHECK(dispatch::resolve_link("FILE:///etc/passwd", "").empty());
  CHECK(dispatch::resolve_link("file://localhost/etc/passwd", "").empty());
  CHECK(dispatch::resolve_link("file:/etc/passwd", "").empty());
  CHECK(dispatch::resolve_link(ini, "https://example.com/feed/").empty());
  CHECK(dispatch::resolve_link("/home/USER/.config/dispatch/dispatch.ini", "").empty());
  CHECK(dispatch::resolve_link("dispatch.ini", "").empty());
  CHECK(dispatch::resolve_link("mailto:a@b.test", "").empty());
  CHECK(dispatch::resolve_link("javascript:alert(1)", "").empty());
  CHECK(dispatch::resolve_link("data:text/html,hi", "").empty());
  CHECK(dispatch::resolve_link("", "https://example.com/").empty());

  CHECK(dispatch::resolve_link("https://example.com/a", "") == "https://example.com/a");
  CHECK(dispatch::resolve_link("http://example.com/a", "") == "http://example.com/a");
  CHECK(dispatch::resolve_link("HTTP://example.com/a", "") == "HTTP://example.com/a");
  CHECK(dispatch::resolve_link("https://example.com//a//b", "") == "https://example.com/a/b");

  const std::string base = "https://example.com/feed/index.html";
  CHECK(dispatch::resolve_link("item.html", base) == "https://example.com/feed/item.html");
  CHECK(dispatch::resolve_link("/abs", base) == "https://example.com/abs");
  CHECK(dispatch::resolve_link("//cdn.example.com/a", base) == "https://cdn.example.com/a");
  CHECK(dispatch::resolve_link("//cdn.example.com/a", "") == "http://cdn.example.com/a");
  CHECK(dispatch::resolve_link("note.html", "http://example.com/dir") ==
        "http://example.com/note.html");

  return suite_test::done("links");
}
