/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "settings.hpp"

#include <gtkmm.h>

namespace dispatch {

class AccountDialog : public Gtk::Dialog {
 public:
  AccountDialog(Gtk::Window& parent, const Settings& settings);
  void apply_to(Settings& settings) const;

 private:
  Gtk::Entry imap_host_;
  Gtk::SpinButton imap_port_;
  Gtk::ComboBoxText imap_tls_;
  Gtk::Entry smtp_host_;
  Gtk::SpinButton smtp_port_;
  Gtk::ComboBoxText smtp_tls_;
  Gtk::Entry user_;
  Gtk::Entry password_;
};

}  // namespace dispatch
