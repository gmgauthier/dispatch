/* SPDX-License-Identifier: Unlicense */

#include "compose_format.hpp"
#include "check.hpp"

#include <gtkmm.h>
#include <string>

namespace {

void place(const Glib::RefPtr<Gtk::TextBuffer>& buf, int chars)
{
  buf->place_cursor(buf->get_iter_at_offset(chars));
}

int cursor_at(const Glib::RefPtr<Gtk::TextBuffer>& buf)
{
  return buf->get_iter_at_mark(buf->get_insert()).get_offset();
}

}  // namespace

int main(int argc, char** argv)
{
  {
    const Glib::ustring line("• hello world");
    const auto mid = dispatch::compose_list_return(line, 7);
    CHECK(mid.handled);
    CHECK(!mid.erase_line);
    CHECK(mid.drop_space);
    CHECK(mid.split_at == 8);

    const auto end = dispatch::compose_list_return(line, static_cast<int>(line.length()));
    CHECK(end.handled);
    CHECK(!end.drop_space);
    CHECK(end.split_at == static_cast<int>(line.length()));

    const auto inside = dispatch::compose_list_return(line, 0);
    CHECK(inside.handled);
    CHECK(!inside.drop_space);
    CHECK(inside.split_at == 2);

    const auto word = dispatch::compose_list_return(Glib::ustring("• hello"), 5);
    CHECK(word.handled);
    CHECK(!word.drop_space);
    CHECK(word.split_at == 5);

    const auto neg = dispatch::compose_list_return(line, -4);
    CHECK(neg.split_at == 2);
    CHECK(!neg.drop_space);
    const auto past = dispatch::compose_list_return(line, 100);
    CHECK(past.split_at == static_cast<int>(line.length()));
    CHECK(!past.drop_space);

    const auto empty = dispatch::compose_list_return(Glib::ustring("• "), 2);
    CHECK(empty.handled);
    CHECK(empty.erase_line);
    const auto bare = dispatch::compose_list_return(Glib::ustring("•"), 0);
    CHECK(bare.handled);
    CHECK(bare.erase_line);

    const auto plain = dispatch::compose_list_return(Glib::ustring("hello world"), 6);
    CHECK(!plain.handled);
    CHECK(!plain.erase_line);
  }

  Gtk::Main kit(argc, argv);

  {
    auto buf = Gtk::TextBuffer::create();
    buf->set_text("• hello world");
    place(buf, 7);
    CHECK(dispatch::compose_list_handle_return(buf));
    CHECK(buf->get_text().raw() == "• hello\n• world");
    CHECK(cursor_at(buf) == static_cast<int>(Glib::ustring("• hello\n• ").length()));

    const std::string html = dispatch::compose_to_html(buf);
    CHECK(html.find("<ul>\n") != std::string::npos);
    CHECK(html.find("<li>hello</li>\n") != std::string::npos);
    CHECK(html.find("<li>world</li>\n") != std::string::npos);
    CHECK(dispatch::compose_to_plain(buf) == "• hello\n• world");
  }

  {
    auto buf = Gtk::TextBuffer::create();
    buf->set_text("• hello world");
    place(buf, static_cast<int>(Glib::ustring("• hello world").length()));
    CHECK(dispatch::compose_list_handle_return(buf));
    CHECK(buf->get_text().raw() == "• hello world\n• ");
    CHECK(cursor_at(buf) == static_cast<int>(Glib::ustring("• hello world\n• ").length()));
    CHECK(dispatch::compose_list_handle_return(buf));
    CHECK(buf->get_text().raw() == "• hello world\n");
  }

  {
    auto buf = Gtk::TextBuffer::create();
    buf->set_text("• hello");
    place(buf, 5);
    CHECK(dispatch::compose_list_handle_return(buf));
    CHECK(buf->get_text().raw() == "• hel\n• lo");
    CHECK(cursor_at(buf) == static_cast<int>(Glib::ustring("• hel\n• ").length()));
  }

  {
    auto buf = Gtk::TextBuffer::create();
    buf->set_text("• hello world");
    place(buf, 0);
    CHECK(dispatch::compose_list_handle_return(buf));
    CHECK(buf->get_text().raw() == "• \n• hello world");
    CHECK(cursor_at(buf) == static_cast<int>(Glib::ustring("• \n• ").length()));
  }

  {
    auto buf = Gtk::TextBuffer::create();
    buf->set_text("hello");
    place(buf, 2);
    CHECK(!dispatch::compose_list_handle_return(buf));
    CHECK(buf->get_text().raw() == "hello");
    CHECK(cursor_at(buf) == 2);
  }

  {
    auto buf = Gtk::TextBuffer::create();
    buf->set_text("hello");
    place(buf, 5);
    dispatch::compose_toggle_list(buf);
    CHECK(buf->get_text().raw() == "• hello");
    dispatch::compose_toggle_list(buf);
    CHECK(buf->get_text().raw() == "hello");
  }

  {
    auto buf = Gtk::TextBuffer::create();
    buf->set_text("• hello world");
    dispatch::compose_ensure_tags(buf);
    auto strong = buf->get_tag_table()->lookup("strong");
    auto from = buf->get_iter_at_offset(static_cast<int>(Glib::ustring("• hello ").length()));
    buf->apply_tag(strong, from, buf->end());
    place(buf, 7);
    CHECK(dispatch::compose_list_handle_return(buf));
    CHECK(buf->get_text().raw() == "• hello\n• world");
    auto world = buf->get_iter_at_offset(static_cast<int>(Glib::ustring("• hello\n• ").length()));
    CHECK(world.has_tag(strong));
    auto head = buf->get_iter_at_offset(2);
    CHECK(!head.has_tag(strong));
  }

  return suite_test::done("compose");
}
