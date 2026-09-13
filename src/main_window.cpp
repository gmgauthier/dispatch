/* SPDX-License-Identifier: Unlicense */

#include "main_window.hpp"
#include "about_dialog.hpp"
#include "article_window.hpp"
#include "paths.hpp"

#include <iostream>

namespace dispatch {
namespace {

void paint_nav_cell(Gtk::CellRenderer* cell, const Gtk::TreeModel::Path& path,
                    const Gtk::TreeModel::Path& current, const Gtk::TreeModel::Path& hover)
{
  if (!cell)
    return;
  const bool is_cur = current.size() > 0 && path.size() > 0 && path == current;
  const bool is_hov = hover.size() > 0 && path.size() > 0 && path == hover;
  auto* text = dynamic_cast<Gtk::CellRendererText*>(cell);
  if (is_cur || is_hov) {
    const char* color = is_cur ? "#8AADC8" : "#C5D4E8";
    cell->property_cell_background() = color;
    cell->property_cell_background_set() = true;
    if (text) {
      text->property_background() = color;
      text->property_background_set() = true;
    }
  } else {
    cell->property_cell_background_set() = false;
    if (text)
      text->property_background_set() = false;
  }
}

bool nav_motion(Gtk::TreeView& view, Gtk::TreeModel::Path& hover, GdkEventMotion* event)
{
  Gtk::TreeModel::Path path;
  Gtk::TreeViewColumn* col = nullptr;
  int cx = 0, cy = 0, bx = 0, by = 0;
  view.convert_widget_to_bin_window_coords(static_cast<int>(event->x),
                                           static_cast<int>(event->y), bx, by);
  if (view.get_path_at_pos(bx, by, path, col, cx, cy) && path.size() > 0) {
    if (hover.size() == 0 || hover != path) {
      hover = path;
      view.queue_draw();
    }
  } else if (hover.size() > 0) {
    hover.clear();
    view.queue_draw();
  }
  return false;
}

bool nav_leave(Gtk::TreeView& view, Gtk::TreeModel::Path& hover, GdkEventCrossing* event)
{
  if (event && event->detail == GDK_NOTIFY_INFERIOR)
    return false;
  if (hover.size() > 0) {
    hover.clear();
    view.queue_draw();
  }
  return false;
}

}  // namespace

MainWindow::MainWindow()
{
  set_title("Dispatch");
  set_default_size(960, 640);
  set_border_width(0);
  get_style_context()->add_class("dispatch-window");

  accel_ = Gtk::AccelGroup::create();
  add_accel_group(accel_);

  load_css();
  load_stub_feeds();
  build_menu();
  build_toolbar();
  build_body();

  status_ctx_ = status_.get_context_id("main");
  set_status("3 unread   last refresh — (stub)");

  add(root_);
  show_all();
  fill_feeds();
  if (!feeds_.empty()) {
    current_feed_ = 0;
    feed_current_path_ = Gtk::TreeModel::Path("0");
    fill_headlines();
  }
}

void MainWindow::load_css()
{
  const std::string css_path = find_data_file("skin/lcos/lcos.css");
  if (css_path.empty()) {
    std::cerr << "dispatch: lcos.css not found\n";
    return;
  }
  try {
    auto css = Gtk::CssProvider::create();
    css->load_from_path(css_path);
    Gtk::StyleContext::add_provider_for_screen(
        Gdk::Screen::get_default(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  } catch (const Glib::Error& e) {
    std::cerr << "dispatch: CSS: " << e.what() << "\n";
  }
}

Gtk::MenuItem* MainWindow::add_item(Gtk::Menu& menu, const Glib::ustring& label,
                                    const sigc::slot<void()>& slot, guint key,
                                    Gdk::ModifierType mods)
{
  auto* item = Gtk::manage(new Gtk::MenuItem(label, true));
  item->signal_activate().connect(slot);
  if (key != 0)
    item->add_accelerator("activate", accel_, key, mods, Gtk::ACCEL_VISIBLE);
  menu.append(*item);
  return item;
}

void MainWindow::build_menu()
{
  auto add_menu = [this](const Glib::ustring& label, Gtk::Menu& menu) {
    auto* top = Gtk::manage(new Gtk::MenuItem(label, true));
    top->set_submenu(menu);
    menubar_.append(*top);
  };

  auto* file = Gtk::manage(new Gtk::Menu());
  add_item(*file, "_Subscribe…", sigc::mem_fun(*this, &MainWindow::on_subscribe));
  add_item(*file, "_Unsubscribe", sigc::mem_fun(*this, &MainWindow::on_unsubscribe));
  file->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  add_item(*file, "E_xit", sigc::mem_fun(*this, &MainWindow::on_quit), GDK_KEY_q,
           Gdk::CONTROL_MASK);
  add_menu("_File", *file);

  auto* edit = Gtk::manage(new Gtk::Menu());
  add_item(*edit, "Mark as _Read", sigc::mem_fun(*this, &MainWindow::on_mark_read));
  add_item(*edit, "Mark as _Unread", sigc::mem_fun(*this, &MainWindow::on_mark_unread));
  add_menu("_Edit", *edit);

  auto* view = Gtk::manage(new Gtk::Menu());
  view_preview_item_ = Gtk::manage(new Gtk::CheckMenuItem("_Preview Pane", true));
  view_preview_item_->set_active(true);
  view_preview_item_->signal_toggled().connect(
      sigc::mem_fun(*this, &MainWindow::on_toggle_preview));
  view->append(*view_preview_item_);
  add_menu("_View", *view);

  auto* feeds = Gtk::manage(new Gtk::Menu());
  add_item(*feeds, "_Refresh", sigc::mem_fun(*this, &MainWindow::on_refresh), GDK_KEY_r,
           Gdk::CONTROL_MASK);
  add_item(*feeds, "Refresh _All", sigc::mem_fun(*this, &MainWindow::on_refresh));
  add_menu("F_eeds", *feeds);

  auto* help = Gtk::manage(new Gtk::Menu());
  add_item(*help, "_About Dispatch", sigc::mem_fun(*this, &MainWindow::on_about));
  add_menu("_Help", *help);
}

void MainWindow::build_toolbar()
{
  toolbar_.set_border_width(4);
  toolbar_.pack_start(btn_refresh_, Gtk::PACK_SHRINK);
  toolbar_.pack_start(btn_subscribe_, Gtk::PACK_SHRINK);
  toolbar_.pack_start(btn_mark_read_, Gtk::PACK_SHRINK);

  auto* spacer = Gtk::manage(new Gtk::Box());
  spacer->set_hexpand(true);
  toolbar_.pack_start(*spacer, Gtk::PACK_EXPAND_WIDGET);

  Gtk::RadioButtonGroup mode;
  btn_mail_.set_group(mode);
  btn_feed_.set_group(mode);
  btn_mail_.set_mode(false);
  btn_feed_.set_mode(false);
  btn_feed_.set_active(true);
  btn_mail_.set_tooltip_text("Coming soon...");
  btn_mail_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_mail_clicked));
  toolbar_.pack_start(btn_mail_, Gtk::PACK_SHRINK);
  toolbar_.pack_start(btn_feed_, Gtk::PACK_SHRINK);

  btn_refresh_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_refresh));
  btn_subscribe_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_subscribe));
  btn_mark_read_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_mark_read));
}

void MainWindow::style_nav_column(Gtk::TreeView& view)
{
  view.set_headers_visible(true);
  view.set_enable_search(false);
  view.get_selection()->set_mode(Gtk::SELECTION_NONE);
  view.add_events(Gdk::POINTER_MOTION_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::BUTTON_PRESS_MASK);
}

void MainWindow::build_body()
{
  feed_cols_.add(col_feed_name_);
  feed_cols_.add(col_feed_unread_);
  feed_cols_.add(col_feed_index_);
  feed_store_ = Gtk::ListStore::create(feed_cols_);
  feed_view_.set_model(feed_store_);
  feed_view_.append_column("Feeds", col_feed_name_);
  feed_view_.append_column("", col_feed_unread_);
  feed_view_.get_style_context()->add_class("dispatch-tree");
  style_nav_column(feed_view_);
  for (guint c = 0; c < feed_view_.get_n_columns(); ++c) {
    if (auto* col = feed_view_.get_column(c)) {
      if (auto* cell = col->get_first_cell())
        col->set_cell_data_func(*cell, sigc::mem_fun(*this, &MainWindow::on_feed_cell_data));
    }
  }
  if (auto* col = feed_view_.get_column(1)) {
    col->set_alignment(1.0);
    if (auto* cell = dynamic_cast<Gtk::CellRendererText*>(col->get_first_cell()))
      cell->property_xalign() = 1.0;
  }
  feed_view_.signal_motion_notify_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_feed_motion), false);
  feed_view_.signal_leave_notify_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_feed_leave), false);
  feed_view_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_feed_button), false);
  feed_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  feed_scroll_.add(feed_view_);
  feed_scroll_.set_size_request(180, -1);

  headline_cols_.add(col_subject_);
  headline_cols_.add(col_date_);
  headline_cols_.add(col_headline_index_);
  headline_store_ = Gtk::ListStore::create(headline_cols_);
  headline_view_.set_model(headline_store_);
  headline_view_.append_column("Subject", col_subject_);
  headline_view_.append_column("Date", col_date_);
  headline_view_.get_style_context()->add_class("dispatch-headlines");
  style_nav_column(headline_view_);
  for (guint c = 0; c < headline_view_.get_n_columns(); ++c) {
    if (auto* col = headline_view_.get_column(c)) {
      if (auto* cell = col->get_first_cell())
        col->set_cell_data_func(*cell, sigc::mem_fun(*this, &MainWindow::on_headline_cell_data));
    }
  }
  if (auto* col = headline_view_.get_column(0))
    col->set_expand(true);
  headline_view_.signal_motion_notify_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_headline_motion), false);
  headline_view_.signal_leave_notify_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_headline_leave), false);
  headline_view_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_headline_button), false);
  headline_view_.signal_key_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_headline_key), false);
  headline_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  headline_scroll_.add(headline_view_);

  body_view_.set_editable(false);
  body_view_.set_wrap_mode(Gtk::WRAP_WORD_CHAR);
  body_view_.set_left_margin(8);
  body_view_.set_right_margin(8);
  body_view_.set_top_margin(8);
  body_view_.set_bottom_margin(8);
  body_view_.get_style_context()->add_class("dispatch-body");
  body_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  body_scroll_.add(body_view_);

  inner_.pack1(headline_scroll_, true, false);
  inner_.pack2(body_scroll_, true, false);
  inner_.set_position(220);

  outer_.pack1(feed_scroll_, false, false);
  outer_.pack2(inner_, true, false);
  outer_.set_position(200);

  root_.pack_start(menubar_, Gtk::PACK_SHRINK);
  root_.pack_start(toolbar_, Gtk::PACK_SHRINK);
  root_.pack_start(outer_, Gtk::PACK_EXPAND_WIDGET);
  root_.pack_start(status_, Gtk::PACK_SHRINK);
}

void MainWindow::load_stub_feeds()
{
  feeds_ = {
      {"LWN",
       3,
       {{"Kernel 6.x lands in testing", "12:01",
         "A stub item. Dispatch has no HTTP yet (M1). Double-click opens this text in a "
         "separate article window. Links will spawn the ISO browser in M4."},
        {"Weekly edition", "yesterday",
         "Another stub headline from LWN. The preview pane is the Outlook Express stack: "
         "feeds on the left, headlines over the body."},
        {"Security leftovers", "Tue", "Third stub. Mark-read and refresh wait on M1/M2."}}},
      {"Debian",
       1,
       {{"DSA-… example advisory", "11:40",
         "Stub Debian security headline. Real fetching is M1."}}},
      {"Lunduke",
       0,
       {{"Weekly…", "yesterday",
         "Stub Lunduke Journal item. Unread count is zero on this feed."}}},
  };
}

void MainWindow::fill_feeds()
{
  feed_store_->clear();
  for (int i = 0; i < static_cast<int>(feeds_.size()); ++i) {
    auto it = feed_store_->append();
    (*it)[col_feed_name_] = feeds_[static_cast<size_t>(i)].name;
    const int n = feeds_[static_cast<size_t>(i)].unread;
    (*it)[col_feed_unread_] = n > 0 ? Glib::ustring::format(n) : Glib::ustring();
    (*it)[col_feed_index_] = i;
  }
}

void MainWindow::fill_headlines()
{
  headline_store_->clear();
  headline_current_path_.clear();
  headline_hover_path_.clear();
  current_headline_ = -1;
  body_view_.get_buffer()->set_text("");
  if (current_feed_ < 0 || current_feed_ >= static_cast<int>(feeds_.size()))
    return;
  const auto& items = feeds_[static_cast<size_t>(current_feed_)].items;
  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    auto it = headline_store_->append();
    (*it)[col_subject_] = items[static_cast<size_t>(i)].subject;
    (*it)[col_date_] = items[static_cast<size_t>(i)].date;
    (*it)[col_headline_index_] = i;
  }
}

void MainWindow::show_preview()
{
  if (current_feed_ < 0 || current_headline_ < 0)
    return;
  const auto& items = feeds_[static_cast<size_t>(current_feed_)].items;
  if (current_headline_ >= static_cast<int>(items.size()))
    return;
  body_view_.get_buffer()->set_text(items[static_cast<size_t>(current_headline_)].body);
}

void MainWindow::open_article()
{
  if (current_feed_ < 0 || current_headline_ < 0)
    return;
  const auto& items = feeds_[static_cast<size_t>(current_feed_)].items;
  if (current_headline_ >= static_cast<int>(items.size()))
    return;
  const auto& h = items[static_cast<size_t>(current_headline_)];
  auto* win = new ArticleWindow(h.subject, h.body);
  if (auto app = get_application())
    app->add_window(*win);
  win->signal_hide().connect([win]() { delete win; });
  win->present();
}

void MainWindow::set_preview_visible(bool on)
{
  preview_visible_ = on;
  if (on)
    body_scroll_.show();
  else
    body_scroll_.hide();
}

void MainWindow::set_status(const Glib::ustring& text)
{
  status_.pop(status_ctx_);
  status_.push(text, status_ctx_);
}

void MainWindow::not_yet(const Glib::ustring& feature)
{
  set_status(feature + " — not yet (M1).");
}

void MainWindow::on_quit()
{
  hide();
}

void MainWindow::on_about()
{
  AboutDialog dlg(*this);
  dlg.run();
}

void MainWindow::on_subscribe()
{
  not_yet("Subscribe");
}

void MainWindow::on_unsubscribe()
{
  not_yet("Unsubscribe");
}

void MainWindow::on_refresh()
{
  not_yet("Refresh");
}

void MainWindow::on_mark_read()
{
  not_yet("Mark read");
}

void MainWindow::on_mark_unread()
{
  not_yet("Mark unread");
}

void MainWindow::on_toggle_preview()
{
  if (!view_preview_item_)
    return;
  set_preview_visible(view_preview_item_->get_active());
}

void MainWindow::on_mail_clicked()
{
  btn_feed_.set_active(true);
}

void MainWindow::on_feed_cell_data(Gtk::CellRenderer* cell,
                                   const Gtk::TreeModel::const_iterator& it)
{
  if (!it)
    return;
  paint_nav_cell(cell, feed_store_->get_path(it), feed_current_path_, feed_hover_path_);
}

void MainWindow::on_headline_cell_data(Gtk::CellRenderer* cell,
                                       const Gtk::TreeModel::const_iterator& it)
{
  if (!it)
    return;
  paint_nav_cell(cell, headline_store_->get_path(it), headline_current_path_,
                 headline_hover_path_);
}

bool MainWindow::on_feed_motion(GdkEventMotion* event)
{
  return nav_motion(feed_view_, feed_hover_path_, event);
}

bool MainWindow::on_feed_leave(GdkEventCrossing* event)
{
  return nav_leave(feed_view_, feed_hover_path_, event);
}

bool MainWindow::on_feed_button(GdkEventButton* event)
{
  if (event && event->type != GDK_BUTTON_PRESS)
    return false;
  Gtk::TreeModel::Path path;
  Gtk::TreeViewColumn* col = nullptr;
  int cx = 0, cy = 0;
  int x = event ? static_cast<int>(event->x) : 0;
  int y = event ? static_cast<int>(event->y) : 0;
  int bx = 0, by = 0;
  feed_view_.convert_widget_to_bin_window_coords(x, y, bx, by);
  if (event && !feed_view_.get_path_at_pos(bx, by, path, col, cx, cy))
    return false;
  if (!event)
    path = Gtk::TreeModel::Path("0");
  auto it = feed_store_->get_iter(path);
  if (!it)
    return false;
  current_feed_ = (*it)[col_feed_index_];
  feed_current_path_ = path;
  fill_headlines();
  feed_view_.queue_draw();
  return true;
}

bool MainWindow::on_headline_motion(GdkEventMotion* event)
{
  return nav_motion(headline_view_, headline_hover_path_, event);
}

bool MainWindow::on_headline_leave(GdkEventCrossing* event)
{
  return nav_leave(headline_view_, headline_hover_path_, event);
}

bool MainWindow::on_headline_button(GdkEventButton* event)
{
  if (!event)
    return false;
  Gtk::TreeModel::Path path;
  Gtk::TreeViewColumn* col = nullptr;
  int cx = 0, cy = 0, bx = 0, by = 0;
  headline_view_.convert_widget_to_bin_window_coords(static_cast<int>(event->x),
                                                     static_cast<int>(event->y), bx, by);
  if (!headline_view_.get_path_at_pos(bx, by, path, col, cx, cy))
    return false;
  auto it = headline_store_->get_iter(path);
  if (!it)
    return false;
  current_headline_ = (*it)[col_headline_index_];
  headline_current_path_ = path;
  show_preview();
  headline_view_.queue_draw();
  if (event->type == GDK_2BUTTON_PRESS) {
    open_article();
    return true;
  }
  return event->type == GDK_BUTTON_PRESS;
}

bool MainWindow::on_headline_key(GdkEventKey* event)
{
  if (!event)
    return false;
  if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
    open_article();
    return true;
  }
  return false;
}

}  // namespace dispatch
