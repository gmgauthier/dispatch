/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

#include <vector>

namespace dispatch {

class MainWindow : public Gtk::Window {
 public:
  MainWindow();

 private:
  struct Headline {
    Glib::ustring subject;
    Glib::ustring date;
    Glib::ustring body;
  };
  struct Feed {
    Glib::ustring name;
    int unread = 0;
    std::vector<Headline> items;
  };

  void load_css();
  void build_menu();
  void build_toolbar();
  void build_body();
  void load_stub_feeds();
  void fill_feeds();
  void fill_headlines();
  void show_preview();
  void open_article();
  void set_preview_visible(bool on);
  void set_status(const Glib::ustring& text);
  void not_yet(const Glib::ustring& feature);
  void on_quit();
  void on_about();
  void on_subscribe();
  void on_unsubscribe();
  void on_refresh();
  void on_mark_read();
  void on_mark_unread();
  void on_toggle_preview();
  void on_mail_clicked();
  void style_nav_column(Gtk::TreeView& view);
  void on_feed_cell_data(Gtk::CellRenderer* cell, const Gtk::TreeModel::const_iterator& it);
  void on_headline_cell_data(Gtk::CellRenderer* cell, const Gtk::TreeModel::const_iterator& it);
  bool on_feed_motion(GdkEventMotion* event);
  bool on_feed_leave(GdkEventCrossing* event);
  bool on_feed_button(GdkEventButton* event);
  bool on_headline_motion(GdkEventMotion* event);
  bool on_headline_leave(GdkEventCrossing* event);
  bool on_headline_button(GdkEventButton* event);
  bool on_headline_key(GdkEventKey* event);

  Gtk::MenuItem* add_item(Gtk::Menu& menu, const Glib::ustring& label,
                          const sigc::slot<void()>& slot, guint key = 0,
                          Gdk::ModifierType mods = Gdk::ModifierType(0));

  Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 0};
  Gtk::MenuBar menubar_;
  Gtk::Box toolbar_{Gtk::ORIENTATION_HORIZONTAL, 4};
  Gtk::Button btn_refresh_{"Refresh"};
  Gtk::Button btn_subscribe_{"Subscribe…"};
  Gtk::Button btn_mark_read_{"Mark read"};
  Gtk::RadioButton btn_mail_{"MAIL"};
  Gtk::RadioButton btn_feed_{"FEED"};
  Gtk::CheckMenuItem* view_preview_item_ = nullptr;
  Glib::RefPtr<Gtk::AccelGroup> accel_;

  Gtk::Paned outer_{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Paned inner_{Gtk::ORIENTATION_VERTICAL};
  Gtk::ScrolledWindow feed_scroll_;
  Gtk::TreeView feed_view_;
  Gtk::ScrolledWindow headline_scroll_;
  Gtk::TreeView headline_view_;
  Gtk::ScrolledWindow body_scroll_;
  Gtk::TextView body_view_;
  Gtk::Statusbar status_;
  guint status_ctx_ = 0;

  Glib::RefPtr<Gtk::ListStore> feed_store_;
  Gtk::TreeModelColumn<Glib::ustring> col_feed_name_;
  Gtk::TreeModelColumn<Glib::ustring> col_feed_unread_;
  Gtk::TreeModelColumn<int> col_feed_index_;
  Gtk::TreeModelColumnRecord feed_cols_;

  Glib::RefPtr<Gtk::ListStore> headline_store_;
  Gtk::TreeModelColumn<Glib::ustring> col_subject_;
  Gtk::TreeModelColumn<Glib::ustring> col_date_;
  Gtk::TreeModelColumn<int> col_headline_index_;
  Gtk::TreeModelColumnRecord headline_cols_;

  std::vector<Feed> feeds_;
  int current_feed_ = -1;
  int current_headline_ = -1;
  Gtk::TreeModel::Path feed_current_path_;
  Gtk::TreeModel::Path feed_hover_path_;
  Gtk::TreeModel::Path headline_current_path_;
  Gtk::TreeModel::Path headline_hover_path_;
  bool preview_visible_ = true;
};

}  // namespace dispatch
