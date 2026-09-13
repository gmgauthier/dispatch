/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

namespace dispatch {

class AboutDialog : public Gtk::Dialog {
 public:
  explicit AboutDialog(Gtk::Window& parent);
};

}  // namespace dispatch
