/* SPDX-License-Identifier: Unlicense */

#include "compose_format.hpp"

#include <sstream>

namespace dispatch {
namespace {

std::string html_escape(const Glib::ustring& raw)
{
  std::string out;
  out.reserve(raw.bytes() + 8);
  for (gunichar c : raw) {
    if (c == '&')
      out += "&amp;";
    else if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else if (c == '"')
      out += "&quot;";
    else {
      char buf[8];
      const int n = g_unichar_to_utf8(c, buf);
      out.append(buf, static_cast<size_t>(n));
    }
  }
  return out;
}

bool has_tag(const Gtk::TextIter& it, const char* name)
{
  auto tags = it.get_tags();
  for (const auto& t : tags) {
    if (t->property_name() == name)
      return true;
  }
  return false;
}

void paragraph_bounds(const Glib::RefPtr<Gtk::TextBuffer>& buf, Gtk::TextIter& a, Gtk::TextIter& b)
{
  a = buf->get_iter_at_mark(buf->get_insert());
  if (!a.starts_line())
    a.set_line_offset(0);
  b = a;
  if (!b.ends_line())
    b.forward_to_line_end();
}

}  // namespace

void compose_ensure_tags(const Glib::RefPtr<Gtk::TextBuffer>& buf)
{
  auto table = buf->get_tag_table();
  auto mk = [&](const char* name, auto fn) {
    if (table->lookup(name))
      return;
    auto tag = Gtk::TextBuffer::Tag::create(name);
    fn(tag);
    table->add(tag);
  };
  mk("strong", [](auto t) { t->property_weight() = Pango::WEIGHT_BOLD; });
  mk("em", [](auto t) { t->property_style() = Pango::STYLE_ITALIC; });
  mk("u", [](auto t) { t->property_underline() = Pango::UNDERLINE_SINGLE; });
  mk("h2", [](auto t) {
    t->property_weight() = Pango::WEIGHT_BOLD;
    t->property_scale() = 1.2;
  });
  mk("blockquote", [](auto t) {
    t->property_left_margin() = 20;
    t->property_style() = Pango::STYLE_ITALIC;
  });
}

void compose_toggle_inline(const Glib::RefPtr<Gtk::TextBuffer>& buf, const char* name)
{
  compose_ensure_tags(buf);
  auto tag = buf->get_tag_table()->lookup(name);
  if (!tag)
    return;
  Gtk::TextIter a, b;
  buf->get_selection_bounds(a, b);
  if (a == b)
    return;
  if (a.has_tag(tag))
    buf->remove_tag(tag, a, b);
  else
    buf->apply_tag(tag, a, b);
}

void compose_apply_paragraph(const Glib::RefPtr<Gtk::TextBuffer>& buf, const char* name)
{
  compose_ensure_tags(buf);
  auto tag = buf->get_tag_table()->lookup(name);
  if (!tag)
    return;
  Gtk::TextIter a, b;
  paragraph_bounds(buf, a, b);
  if (a.has_tag(tag))
    buf->remove_tag(tag, a, b);
  else
    buf->apply_tag(tag, a, b);
}

void compose_toggle_list(const Glib::RefPtr<Gtk::TextBuffer>& buf)
{
  Gtk::TextIter a, b;
  paragraph_bounds(buf, a, b);
  const Glib::ustring line = buf->get_text(a, b);
  if (line.compare(0, 2, "• ") == 0 || line == "•") {
    auto end = a;
    end.forward_chars(line == "•" ? 1 : 2);
    buf->erase(a, end);
  } else {
    auto after = buf->insert(a, "• ");
    buf->place_cursor(after);
  }
}

bool compose_list_handle_return(const Glib::RefPtr<Gtk::TextBuffer>& buf)
{
  Gtk::TextIter a, b;
  paragraph_bounds(buf, a, b);
  const Glib::ustring line = buf->get_text(a, b);
  if (line.compare(0, 2, "• ") != 0 && line != "•")
    return false;
  if (line == "• " || line == "•") {
    buf->erase(a, b);
    return true;
  }
  auto after = buf->insert(b, "\n• ");
  buf->place_cursor(after);
  return true;
}

std::string compose_to_plain(const Glib::RefPtr<Gtk::TextBuffer>& buf)
{
  return buf->get_text().raw();
}

std::string compose_to_html(const Glib::RefPtr<Gtk::TextBuffer>& buf)
{
  compose_ensure_tags(buf);
  std::string html;
  auto it = buf->begin();
  bool in_ul = false;
  auto close_ul = [&]() {
    if (in_ul) {
      html += "</ul>\n";
      in_ul = false;
    }
  };
  while (!it.is_end()) {
    Gtk::TextIter line_end = it;
    if (!line_end.ends_line())
      line_end.forward_to_line_end();
    const Glib::ustring line = buf->get_text(it, line_end);
    const bool is_h2 = has_tag(it, "h2");
    const bool is_quote = has_tag(it, "blockquote");
    const bool is_li = line.compare(0, 2, "• ") == 0;
    if (is_li) {
      if (!in_ul)
        html += "<ul>\n";
      in_ul = true;
      html += "<li>";
    } else {
      close_ul();
      if (is_h2)
        html += "<h2>";
      else if (is_quote)
        html += "<blockquote><p>";
      else
        html += "<p>";
    }
    Gtk::TextIter cur = it;
    if (is_li) {
      cur.forward_chars(2);
      if (cur > line_end)
        cur = line_end;
    }
    bool on_strong = false, on_em = false, on_u = false;
    auto flush_close = [&](bool s, bool e, bool u) {
      if (on_u && !u) {
        html += "</u>";
        on_u = false;
      }
      if (on_em && !e) {
        html += "</em>";
        on_em = false;
      }
      if (on_strong && !s) {
        html += "</strong>";
        on_strong = false;
      }
    };
    auto flush_open = [&](bool s, bool e, bool u) {
      if (s && !on_strong) {
        html += "<strong>";
        on_strong = true;
      }
      if (e && !on_em) {
        html += "<em>";
        on_em = true;
      }
      if (u && !on_u) {
        html += "<u>";
        on_u = true;
      }
    };
    while (cur < line_end) {
      Gtk::TextIter run = cur;
      run.forward_to_tag_toggle(Glib::RefPtr<Gtk::TextTag>());
      if (run > line_end)
        run = line_end;
      const bool s = has_tag(cur, "strong");
      const bool e = has_tag(cur, "em");
      const bool u = has_tag(cur, "u");
      flush_close(s, e, u);
      flush_open(s, e, u);
      html += html_escape(buf->get_text(cur, run));
      cur = run;
    }
    flush_close(false, false, false);
    if (is_li)
      html += "</li>\n";
    else if (is_h2)
      html += "</h2>\n";
    else if (is_quote)
      html += "</p></blockquote>\n";
    else
      html += "</p>\n";
    if (line_end.is_end())
      break;
    it = line_end;
    it.forward_char();
    if (it.is_end() && line_end.ends_line())
      break;
  }
  close_ul();
  if (html.empty())
    html = "<p></p>\n";
  return html;
}

std::string plain_to_letter_html(const std::string& plain)
{
  if (plain.empty())
    return "<p>(no text body)</p>";
  std::ostringstream html;
  std::istringstream in(plain);
  std::string line;
  bool any = false;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    html << "<p>" << html_escape(Glib::ustring(line)) << "</p>\n";
    any = true;
  }
  return any ? html.str() : "<p></p>\n";
}

}  // namespace dispatch
