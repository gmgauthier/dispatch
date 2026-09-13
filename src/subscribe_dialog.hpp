/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

namespace dispatch {

class SubscribeDialog : public Gtk::Dialog {
 public:
  explicit SubscribeDialog(Gtk::Window& parent);
  Glib::ustring url() const;

 private:
  Gtk::Entry url_;
};

}  // namespace dispatch
