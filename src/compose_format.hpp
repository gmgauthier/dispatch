/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>
#include <string>

namespace dispatch {

void compose_ensure_tags(const Glib::RefPtr<Gtk::TextBuffer>& buf);
void compose_toggle_inline(const Glib::RefPtr<Gtk::TextBuffer>& buf, const char* name);
void compose_apply_paragraph(const Glib::RefPtr<Gtk::TextBuffer>& buf, const char* name);
void compose_toggle_list(const Glib::RefPtr<Gtk::TextBuffer>& buf);

// Where Return falls in one compose line. `split_at` is a character index in
// `line`. When `drop_space` is set, the character just before `split_at` is a
// gap and is removed so it does not become the next item's first character.
struct ComposeListReturn {
  bool handled = false;
  bool erase_line = false;
  int split_at = 0;
  bool drop_space = false;
};

ComposeListReturn compose_list_return(const Glib::ustring& line, int caret);
bool compose_list_handle_return(const Glib::RefPtr<Gtk::TextBuffer>& buf);
std::string compose_to_plain(const Glib::RefPtr<Gtk::TextBuffer>& buf);
std::string compose_to_html(const Glib::RefPtr<Gtk::TextBuffer>& buf);
std::string plain_to_letter_html(const std::string& plain);

}  // namespace dispatch
