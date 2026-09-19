/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "body_view.hpp"
#include "feed.hpp"

#include <gtkmm.h>

namespace dispatch {

struct MessageHeader {
  Glib::ustring from;
  Glib::ustring to;
  Glib::ustring cc;
  Glib::ustring date;
  Glib::ustring subject;
};

class ArticleWindow : public Gtk::Window {
 public:
  ArticleWindow(const Glib::ustring& subject, const std::string& html, const std::string& base_url,
                const std::vector<Enclosure>& extra, MessageHeader hdr = {});
};

}  // namespace dispatch
