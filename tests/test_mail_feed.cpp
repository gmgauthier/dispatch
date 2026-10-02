/* SPDX-License-Identifier: Unlicense */

#include "feed.hpp"
#include "mail_parse.hpp"
#include "opml.hpp"
#include "check.hpp"

#include <fstream>
#include <string>
#include <unistd.h>

int main()
{
  {
    const auto addrs = dispatch::split_addresses("Ann <a@b.test>; Bob <b@b.test>, plain@c.test");
    CHECK(addrs.size() == 3);
    CHECK(addrs[0] == "a@b.test");
    CHECK(addrs[1] == "b@b.test");
    CHECK(addrs[2] == "plain@c.test");
    CHECK(dispatch::split_addresses("").empty());

    const auto doe = dispatch::split_addresses(
        "Doe, Jane <jane@example.com>, \"Roe, Bob\" <bob@example.com>; plain@c.test");
    CHECK(doe.size() == 3);
    CHECK(doe[0] == "jane@example.com");
    CHECK(doe[1] == "bob@example.com");
    CHECK(doe[2] == "plain@c.test");
    const auto doubled = dispatch::split_addresses("a@b.test,,c@d.test");
    CHECK(doubled.size() == 2);
    CHECK(doubled[0] == "a@b.test");
    CHECK(doubled[1] == "c@d.test");
    const auto angled = dispatch::split_addresses("Weird <doe,jane@example.com>");
    CHECK(angled.size() == 1);
    CHECK(angled[0] == "doe,jane@example.com");

    const std::string letter = dispatch::build_rfc822("me@dispatch.test", doe, {}, "Hello",
                                                      "Plain body", "");
    std::string from;
    std::vector<std::string> rcpt;
    dispatch::parse_rfc822_envelope(letter, from, rcpt);
    CHECK(from == "me@dispatch.test");
    CHECK(rcpt.size() == 3);
    CHECK(rcpt[0] == "jane@example.com");
    CHECK(rcpt[1] == "bob@example.com");
    CHECK(rcpt[2] == "plain@c.test");
  }

  CHECK(dispatch::with_re_prefix("Hello") == "Re: Hello");
  CHECK(dispatch::with_re_prefix("re: Hello") == "re: Hello");
  CHECK(dispatch::with_re_prefix("RE:Hello") == "RE:Hello");
  CHECK(dispatch::with_re_prefix("") == "Re: ");
  CHECK(dispatch::with_fwd_prefix("fwd: x") == "fwd: x");
  CHECK(dispatch::with_fwd_prefix("Note") == "Fwd: Note");

  {
    const std::string quoted = dispatch::quote_plain("Ann", "Mon", "Line one\nLine two");
    CHECK(quoted.find("On Mon, Ann wrote:") != std::string::npos);
    CHECK(quoted.find("> Line one\n") != std::string::npos);
    CHECK(quoted.find("> Line two\n") != std::string::npos);
  }

  {
    const std::string raw = dispatch::build_rfc822("me@dispatch.test", {"you@dispatch.test"}, {},
                                                   "Hello", "Plain body", "");
    CHECK(raw.find("Subject: Hello") != std::string::npos);
    CHECK(raw.find("Plain body") != std::string::npos);
    CHECK(raw.find("me@dispatch.test") != std::string::npos);
    CHECK(raw.find("you@dispatch.test") != std::string::npos);
    std::string from;
    std::vector<std::string> rcpt;
    dispatch::parse_rfc822_envelope(raw, from, rcpt);
    CHECK(from == "me@dispatch.test");
    CHECK(rcpt.size() == 1);
    CHECK(rcpt[0] == "you@dispatch.test");
  }

  CHECK(dispatch::html_to_text("<p>Hi <b>there</b></p>") == "Hi there");
  CHECK(dispatch::html_to_text("A &amp; B") == "A & B");
  CHECK(dispatch::short_date("2026-09-25T02:01:51Z") == "2026-09-25");
  {
    const std::string shown = dispatch::short_date("Fri, 25 Sep 2026 02:01:51 GMT");
    CHECK(shown.compare(0, 11, "25 Sep 2026") == 0);
  }

  {
    const auto empty = dispatch::parse_feed("", "Fallback");
    CHECK(!empty.error.empty());
    const char* xml =
        "<?xml version=\"1.0\"?><rss version=\"2.0\"><channel><title>Desk</title>"
        "<item><title>One</title><link>http://example.test/1</link>"
        "<pubDate>Fri, 25 Sep 2026 02:01:51 GMT</pubDate>"
        "<description><![CDATA[<p>Hello</p>]]></description></item>"
        "</channel></rss>";
    const auto feed = dispatch::parse_feed(xml, "Fallback");
    CHECK(feed.error.empty());
    CHECK(feed.title == "Desk");
    CHECK(feed.items.size() == 1);
    CHECK(feed.items[0].subject == "One");
    CHECK(feed.items[0].link == "http://example.test/1");
    CHECK(feed.items[0].html.find("Hello") != std::string::npos);
  }

  {
    const std::string path =
        "/tmp/dispatch-opml-" + std::to_string(static_cast<long long>(getpid())) + ".opml";
    std::string error;
    const std::vector<dispatch::OpmlOutline> outlines = {
        {"http://example.test/a.xml", "Alpha"},
        {"http://example.test/b.xml", ""},
    };
    CHECK(dispatch::write_opml_file(path, outlines, error));
    const auto back = dispatch::parse_opml_file(path, error);
    CHECK(error.empty());
    CHECK(back.size() == 2);
    CHECK(back[0].url == "http://example.test/a.xml");
    CHECK(back[0].title == "Alpha");
    CHECK(back[1].url == "http://example.test/b.xml");
    CHECK(back[1].title == "http://example.test/b.xml");
    CHECK(dispatch::parse_opml_file("/etc/hostname", error).empty());
    CHECK(!error.empty());
    std::remove(path.c_str());
  }

  return suite_test::done("mail-feed");
}
