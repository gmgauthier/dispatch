/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

namespace dispatch {

class ArticleWindow : public Gtk::Window {
 public:
  ArticleWindow(const Glib::ustring& subject, const Glib::ustring& body);
};

}  // namespace dispatch
