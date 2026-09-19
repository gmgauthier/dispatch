/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>
#include <string>

namespace dispatch {

void compose_ensure_tags(const Glib::RefPtr<Gtk::TextBuffer>& buf);
void compose_toggle_inline(const Glib::RefPtr<Gtk::TextBuffer>& buf, const char* name);
void compose_apply_paragraph(const Glib::RefPtr<Gtk::TextBuffer>& buf, const char* name);
void compose_toggle_list(const Glib::RefPtr<Gtk::TextBuffer>& buf);
bool compose_list_handle_return(const Glib::RefPtr<Gtk::TextBuffer>& buf);
std::string compose_to_plain(const Glib::RefPtr<Gtk::TextBuffer>& buf);
std::string compose_to_html(const Glib::RefPtr<Gtk::TextBuffer>& buf);
std::string plain_to_letter_html(const std::string& plain);

}  // namespace dispatch
