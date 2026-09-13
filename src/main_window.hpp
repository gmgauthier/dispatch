/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "body_view.hpp"
#include "feed.hpp"
#include "fetch.hpp"
#include "opml.hpp"
#include "settings.hpp"

#include <gtkmm.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace dispatch {

class MainWindow : public Gtk::Window {
 public:
  MainWindow();
  ~MainWindow() override;

 private:
  struct Item {
    Glib::ustring subject;
    Glib::ustring date;
    Glib::ustring html;
    Glib::ustring link;
    std::vector<Enclosure> enclosures;
    bool unread = true;
  };
  struct PendingFetch {
    Glib::ustring url;
    int replace_index = -1;
    bool select = false;
  };
  struct Feed {
    Glib::ustring url;
    Glib::ustring name;
    int unread = 0;
    std::vector<Item> items;
  };
  struct FetchJob {
    Glib::ustring url;
    int replace_index = -1;
    ParsedFeed parsed;
    std::string error;
  };

  void load_css();
  void build_menu();
  void build_toolbar();
  void build_body();
  void fill_feeds();
  void fill_headlines();
  void select_feed(int index);
  void show_preview();
  void open_article();
  void set_preview_visible(bool on);
  void set_status(const Glib::ustring& text);
  void set_busy(bool on);
  void persist();
  void recount(Feed& feed);
  int total_unread() const;
  std::string key_of(const Item& item) const;
  void apply_read_state(Feed& feed);
  void mark_item(int feed_index, int item_index, bool unread);
  void request_fetch(const Glib::ustring& url, int replace_index, bool select);
  void start_fetch(const Glib::ustring& url, int replace_index);
  void pump_fetch_queue();
  void on_fetch_done();
  int find_url(const Glib::ustring& url) const;
  void on_quit();
  void on_about();
  void on_subscribe();
  void on_unsubscribe();
  void on_import_opml();
  void on_export_opml();
  void on_refresh();
  void on_refresh_all();
  void on_mark_read();
  void on_toggle_unread();
  void on_toggle_preview();
  void on_appearance();
  void apply_appearance();
  void on_mail_clicked();
  void style_nav_column(Gtk::TreeView& view);
  void on_feed_cell_data(Gtk::CellRenderer* cell, const Gtk::TreeModel::const_iterator& it);
  void on_headline_cell_data(Gtk::CellRenderer* cell, const Gtk::TreeModel::const_iterator& it);
  bool on_feed_motion(GdkEventMotion* event);
  bool on_feed_leave(GdkEventCrossing* event);
  bool on_feed_button(GdkEventButton* event);
  bool on_feed_key(GdkEventKey* event);
  bool on_headline_motion(GdkEventMotion* event);
  bool on_headline_leave(GdkEventCrossing* event);
  bool on_headline_button(GdkEventButton* event);
  bool on_headline_key(GdkEventKey* event);
  void step_feed(int delta);
  void step_headline(int delta);

  Gtk::MenuItem* add_item(Gtk::Menu& menu, const Glib::ustring& label,
                          const sigc::slot<void()>& slot, guint key = 0,
                          Gdk::ModifierType mods = Gdk::ModifierType(0));

  Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 0};
  Gtk::MenuBar menubar_;
  Gtk::Box toolbar_{Gtk::ORIENTATION_HORIZONTAL, 4};
  Gtk::Button btn_refresh_{"Refresh"};
  Gtk::Button btn_subscribe_{"Subscribe…"};
  Gtk::Button btn_toggle_unread_{"Toggle Unread"};
  Gtk::RadioButton btn_mail_{"MAIL"};
  Gtk::RadioButton btn_feed_{"FEED"};
  Gtk::CheckMenuItem* view_preview_item_ = nullptr;
  Glib::RefPtr<Gtk::AccelGroup> accel_;

  Gtk::Paned outer_{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Paned inner_{Gtk::ORIENTATION_VERTICAL};
  Gtk::Frame head_frame_;
  Gtk::Frame body_frame_;
  Gtk::ScrolledWindow feed_scroll_;
  Gtk::TreeView feed_view_;
  Gtk::ScrolledWindow headline_scroll_;
  Gtk::TreeView headline_view_;
  Gtk::ScrolledWindow body_scroll_;
  BodyView body_view_;
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

  Settings settings_;
  std::vector<Feed> feeds_;
  std::vector<PendingFetch> fetch_queue_;
  bool select_after_fetch_ = false;
  int current_feed_ = -1;
  int current_headline_ = -1;
  Gtk::TreeModel::Path feed_current_path_;
  Gtk::TreeModel::Path feed_hover_path_;
  Gtk::TreeModel::Path headline_current_path_;
  Gtk::TreeModel::Path headline_hover_path_;
  bool preview_visible_ = true;

  Glib::Dispatcher fetch_done_;
  sigc::connection fetch_conn_;
  std::mutex fetch_mutex_;
  FetchJob fetch_job_;
  std::thread fetch_thread_;
  std::atomic<bool> fetching_{false};
};

}  // namespace dispatch
