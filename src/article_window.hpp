/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "body_view.hpp"
#include "feed.hpp"

#include <gtkmm.h>

namespace dispatch {

class ArticleWindow : public Gtk::Window {
 public:
  ArticleWindow(const Glib::ustring& subject, const std::string& html, const std::string& base_url,
                const std::vector<Enclosure>& extra);
};

}  // namespace dispatch
