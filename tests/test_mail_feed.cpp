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

  {
    int inflight = 2;
    int below = 0;
    int same = 2;
    int above = 4;
    int append = -1;
    dispatch::retarget_fetch_index(inflight, 2);
    dispatch::retarget_fetch_index(below, 2);
    dispatch::retarget_fetch_index(same, 2);
    dispatch::retarget_fetch_index(above, 2);
    dispatch::retarget_fetch_index(append, 2);
    CHECK(inflight == dispatch::kFetchDiscard);
    CHECK(below == 0);
    CHECK(same == dispatch::kFetchDiscard);
    CHECK(above == 3);
    CHECK(append == -1);
    int untouched = 3;
    dispatch::retarget_fetch_index(untouched, -1);
    CHECK(untouched == 3);
  }
  CHECK(dispatch::html_to_text("<p>Hi <b>there</b></p>") == "Hi there");
  CHECK(dispatch::html_to_text("A &amp; B") == "A & B");
  {
    const std::string grin = "\xF0\x9F\x98\x80";
    CHECK(dispatch::html_to_text("Hello &#128512;") == "Hello " + grin);
    CHECK(dispatch::html_to_text("Hello &#x1F600;") == "Hello " + grin);
    CHECK(dispatch::html_to_text("&#8364;") == "\xE2\x82\xAC");
    CHECK(dispatch::html_to_text("&#xD800;") == "&#xD800;");
  }
  {
    const char* html_only =
        "From: Ada <ada@example.com>\r\n"
        "To: Greg <greg@example.com>\r\n"
        "Subject: Hi\r\n"
        "Date: Fri, 02 Oct 2026 08:00:00 +0100\r\n"
        "Message-ID: <html-only@example.com>\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "\r\n"
        "<p>Tom &amp; Jerry</p>\r\n";
    dispatch::MailMessage msg;
    dispatch::parse_rfc822(html_only, msg);
    CHECK(msg.text.find("Tom & Jerry") != std::string::npos);
    CHECK(msg.text.find("&amp;") == std::string::npos);
    const std::string quoted = dispatch::quote_plain(msg.from, msg.date, msg.text);
    CHECK(quoted.find("> Tom & Jerry") != std::string::npos);
    CHECK(quoted.find("&amp;") == std::string::npos);

    const char* alternative =
        "From: Ada <ada@example.com>\r\n"
        "To: Greg <greg@example.com>\r\n"
        "Subject: Hi\r\n"
        "Date: Fri, 02 Oct 2026 08:01:00 +0100\r\n"
        "Message-ID: <html-alt@example.com>\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/alternative; boundary=bnd\r\n"
        "\r\n"
        "--bnd\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "\r\n"
        "Tom & Jerry plain\r\n"
        "--bnd\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "\r\n"
        "<p>Tom &amp; Jerry html</p>\r\n"
        "--bnd--\r\n";
    dispatch::MailMessage alt;
    dispatch::parse_rfc822(alternative, alt);
    CHECK(alt.text.find("Tom & Jerry plain") != std::string::npos);
    CHECK(alt.text.find("html") == std::string::npos);
  }
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

    const std::string grin = "\xF0\x9F\x98\x80";
    const char* cdata =
        "<?xml version=\"1.0\"?><rss version=\"2.0\"><channel><title>Desk</title>"
        "<item><title><![CDATA[Hello &#128512;]]></title>"
        "<link>http://example.test/2</link></item></channel></rss>";
    const auto cdata_feed = dispatch::parse_feed(cdata, "Fallback");
    CHECK(cdata_feed.error.empty());
    CHECK(cdata_feed.items.size() == 1);
    CHECK(cdata_feed.items[0].subject == "Hello " + grin);

    const char* doubled =
        "<?xml version=\"1.0\"?><rss version=\"2.0\"><channel><title>Desk</title>"
        "<item><title>Hello &amp;#x1F600;</title>"
        "<link>http://example.test/3</link></item></channel></rss>";
    const auto doubled_feed = dispatch::parse_feed(doubled, "Fallback");
    CHECK(doubled_feed.error.empty());
    CHECK(doubled_feed.items.size() == 1);
    CHECK(doubled_feed.items[0].subject == "Hello " + grin);
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
