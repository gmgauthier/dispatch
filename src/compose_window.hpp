/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "settings.hpp"

#include <functional>
#include <gtkmm.h>
#include <mutex>
#include <string>
#include <thread>

namespace dispatch {

struct ComposeFill {
  Glib::ustring title = "New Mail";
  std::string to;
  std::string cc;
  std::string subject;
  std::string body;
  std::string in_reply_to;
  std::string references;
};

class ComposeWindow : public Gtk::Window {
 public:
  ComposeWindow(const Settings& settings, std::function<void(bool sent, std::string error)> done,
                ComposeFill fill = {});
  ~ComposeWindow() override;

 private:
  void on_send();
  void on_send_done();
  void on_pick(Gtk::Entry& dest);
  void on_fmt_bold();
  void on_fmt_italic();
  void on_fmt_underline();
  void on_fmt_heading();
  void on_fmt_list();
  void on_fmt_quote();
  bool on_body_key(GdkEventKey* event);

  Settings settings_;
  std::function<void(bool sent, std::string error)> done_;
  Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 8};
  Gtk::Grid grid_;
  Gtk::Label from_label_;
  Gtk::Entry to_;
  Gtk::Entry cc_;
  Gtk::Entry subject_;
  Gtk::Box fmt_{Gtk::ORIENTATION_HORIZONTAL, 4};
  Gtk::Button btn_bold_{"B"};
  Gtk::Button btn_italic_{"I"};
  Gtk::Button btn_underline_{"U"};
  Gtk::Button btn_heading_{"H"};
  Gtk::Button btn_list_{"•"};
  Gtk::Button btn_quote_{"“"};
  Gtk::ScrolledWindow body_scroll_;
  Gtk::TextView body_;
  Gtk::Box buttons_{Gtk::ORIENTATION_HORIZONTAL, 8};
  Gtk::Button btn_send_{"Send"};
  Gtk::Label status_;
  Glib::Dispatcher send_done_;
  sigc::connection send_conn_;
  std::mutex mu_;
  std::thread send_thread_;
  bool send_ok_ = false;
  std::string send_error_;
  std::string in_reply_to_;
  std::string references_;
};

}  // namespace dispatch
