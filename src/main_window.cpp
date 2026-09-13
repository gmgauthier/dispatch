/* SPDX-License-Identifier: Unlicense */

#include "main_window.hpp"
#include "about_dialog.hpp"
#include "article_window.hpp"
#include "font_dialog.hpp"
#include "paths.hpp"
#include "subscribe_dialog.hpp"

#include <glib.h>

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

/* Motion/button events on the list arrive on the bin window already.
 * get_path_at_pos wants those coords. convert_widget_to_bin_window_coords
 * subtracts the header height a second time — one row high when headers
 * are visible (Partyline/Read-O-Matic hide headers, so they never saw it). */
bool path_at_bin_event(Gtk::TreeView& view, GdkWindow* win, double ex, double ey,
                       Gtk::TreeModel::Path& path)
{
  auto bin = view.get_bin_window();
  if (!bin || !win || win != bin->gobj())
    return false;
  Gtk::TreeViewColumn* col = nullptr;
  int cx = 0, cy = 0;
  return view.get_path_at_pos(static_cast<int>(ex), static_cast<int>(ey), path, col, cx, cy) &&
         path.size() > 0;
}

bool nav_motion(Gtk::TreeView& view, Gtk::TreeModel::Path& hover, GdkEventMotion* event)
{
  if (!event)
    return false;
  Gtk::TreeModel::Path path;
  if (path_at_bin_event(view, event->window, event->x, event->y, path)) {
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

Glib::ustring u8(const std::string& raw)
{
  if (raw.empty() || g_utf8_validate(raw.data(), static_cast<gssize>(raw.size()), nullptr))
    return Glib::ustring(raw);
  gchar* v = g_utf8_make_valid(raw.data(), static_cast<gssize>(raw.size()));
  Glib::ustring u(v ? v : "");
  g_free(v);
  return u;
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
  build_menu();
  build_toolbar();
  build_body();

  status_ctx_ = status_.get_context_id("main");
  fetch_conn_ = fetch_done_.connect(sigc::mem_fun(*this, &MainWindow::on_fetch_done));
  signal_hide().connect(sigc::mem_fun(*this, &MainWindow::persist));

  settings_.load();
  apply_appearance();
  for (const auto& saved : settings_.feeds) {
    Feed f;
    f.url = u8(saved.url);
    f.name = saved.title.empty() ? f.url : u8(saved.title);
    feeds_.push_back(std::move(f));
  }

  add(root_);
  show_all();
  fill_feeds();
  if (feeds_.empty()) {
    set_status("No feeds. Subscribe… to add a URL.");
  } else {
    select_feed(0);
    set_status(Glib::ustring::compose("%1 feeds. Refreshing…", feeds_.size()));
    on_refresh_all();
  }
}

MainWindow::~MainWindow()
{
  persist();
  fetch_conn_.disconnect();
  if (fetch_thread_.joinable())
    fetch_thread_.join();
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
  add_item(*file, "_Import OPML…", sigc::mem_fun(*this, &MainWindow::on_import_opml));
  add_item(*file, "_Export OPML…", sigc::mem_fun(*this, &MainWindow::on_export_opml));
  file->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  add_item(*file, "E_xit", sigc::mem_fun(*this, &MainWindow::on_quit), GDK_KEY_q,
           Gdk::CONTROL_MASK);
  add_menu("_File", *file);

  auto* edit = Gtk::manage(new Gtk::Menu());
  add_item(*edit, "Mark as _Read", sigc::mem_fun(*this, &MainWindow::on_mark_read));
  add_item(*edit, "_Toggle Unread", sigc::mem_fun(*this, &MainWindow::on_toggle_unread));
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
  add_item(*feeds, "Refresh _All", sigc::mem_fun(*this, &MainWindow::on_refresh_all));
  add_menu("F_eeds", *feeds);

  auto* options = Gtk::manage(new Gtk::Menu());
  add_item(*options, "_Appearance…", sigc::mem_fun(*this, &MainWindow::on_appearance));
  add_menu("_Options", *options);

  auto* help = Gtk::manage(new Gtk::Menu());
  add_item(*help, "_About Dispatch", sigc::mem_fun(*this, &MainWindow::on_about));
  add_menu("_Help", *help);
}

void MainWindow::build_toolbar()
{
  toolbar_.set_border_width(4);
  toolbar_.pack_start(btn_refresh_, Gtk::PACK_SHRINK);
  toolbar_.pack_start(btn_subscribe_, Gtk::PACK_SHRINK);
  toolbar_.pack_start(btn_toggle_unread_, Gtk::PACK_SHRINK);

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
  btn_toggle_unread_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_toggle_unread));
}

void MainWindow::style_nav_column(Gtk::TreeView& view)
{
  view.set_headers_visible(true);
  view.set_enable_search(false);
  view.set_fixed_height_mode(true);
  view.set_can_focus(true);
  view.get_selection()->set_mode(Gtk::SELECTION_NONE);
  view.add_events(Gdk::POINTER_MOTION_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::BUTTON_PRESS_MASK |
                   Gdk::KEY_PRESS_MASK);
}

void MainWindow::build_body()
{
  feed_cols_.add(col_feed_name_);
  feed_cols_.add(col_feed_unread_);
  feed_cols_.add(col_feed_index_);
  feed_store_ = Gtk::ListStore::create(feed_cols_);
  feed_view_.set_model(feed_store_);
  {
    auto* name_cell = Gtk::manage(new Gtk::CellRendererText());
    name_cell->property_weight() = Pango::WEIGHT_BOLD;
    name_cell->property_xpad() = 8;
    name_cell->property_ypad() = 4;
    name_cell->property_ellipsize() = Pango::ELLIPSIZE_END;
    feed_view_.append_column("Feeds", *name_cell);
    if (auto* col = feed_view_.get_column(0)) {
      col->add_attribute(*name_cell, "text", col_feed_name_);
      col->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
      col->set_expand(true);
      col->set_min_width(60);
    }
    auto* unread_cell = Gtk::manage(new Gtk::CellRendererText());
    unread_cell->property_xpad() = 6;
    unread_cell->property_ypad() = 4;
    unread_cell->property_xalign() = 1.0;
    feed_view_.append_column("", *unread_cell);
    if (auto* col = feed_view_.get_column(1)) {
      col->add_attribute(*unread_cell, "text", col_feed_unread_);
      col->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
      col->set_expand(false);
      col->set_fixed_width(36);
      col->set_alignment(1.0);
    }
  }
  feed_view_.get_style_context()->add_class("dispatch-tree");
  style_nav_column(feed_view_);
  for (guint c = 0; c < feed_view_.get_n_columns(); ++c) {
    if (auto* col = feed_view_.get_column(c)) {
      if (auto* cell = col->get_first_cell())
        col->set_cell_data_func(*cell, sigc::mem_fun(*this, &MainWindow::on_feed_cell_data));
    }
  }
  feed_view_.signal_motion_notify_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_feed_motion), false);
  feed_view_.signal_leave_notify_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_feed_leave), false);
  feed_view_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_feed_button), false);
  feed_view_.signal_key_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_feed_key), false);
  feed_scroll_.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
  feed_scroll_.add(feed_view_);
  feed_scroll_.set_size_request(180, -1);

  headline_cols_.add(col_subject_);
  headline_cols_.add(col_date_);
  headline_cols_.add(col_headline_index_);
  headline_store_ = Gtk::ListStore::create(headline_cols_);
  headline_view_.set_model(headline_store_);
  {
    auto* subj_cell = Gtk::manage(new Gtk::CellRendererText());
    subj_cell->property_xpad() = 1;
    subj_cell->property_ypad() = 1;
    subj_cell->property_ellipsize() = Pango::ELLIPSIZE_END;
    headline_view_.append_column("Subject", *subj_cell);
    if (auto* col = headline_view_.get_column(0)) {
      col->add_attribute(*subj_cell, "text", col_subject_);
      col->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
      col->set_expand(true);
      col->set_min_width(80);
    }
    auto* date_cell = Gtk::manage(new Gtk::CellRendererText());
    date_cell->property_xpad() = 1;
    date_cell->property_ypad() = 1;
    date_cell->property_xalign() = 1.0;
    headline_view_.append_column("Date", *date_cell);
    if (auto* col = headline_view_.get_column(1)) {
      col->add_attribute(*date_cell, "text", col_date_);
      col->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
      col->set_expand(false);
      col->set_fixed_width(108);
    }
  }
  headline_view_.get_style_context()->add_class("dispatch-headlines");
  style_nav_column(headline_view_);
  for (guint c = 0; c < headline_view_.get_n_columns(); ++c) {
    if (auto* col = headline_view_.get_column(c)) {
      if (auto* cell = col->get_first_cell())
        col->set_cell_data_func(*cell, sigc::mem_fun(*this, &MainWindow::on_headline_cell_data));
    }
  }
  headline_view_.signal_motion_notify_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_headline_motion), false);
  headline_view_.signal_leave_notify_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_headline_leave), false);
  headline_view_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_headline_button), false);
  headline_view_.signal_key_press_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_headline_key), false);
  headline_scroll_.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
  headline_scroll_.add(headline_view_);
  headline_scroll_.get_style_context()->add_class("dispatch-headlines-scroll");

  body_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  body_scroll_.add(body_view_);
  body_view_.set_can_focus(true);
  feed_scroll_.signal_button_press_event().connect(
      [this](GdkEventButton*) {
        feed_view_.grab_focus();
        return false;
      },
      false);
  headline_scroll_.signal_button_press_event().connect(
      [this](GdkEventButton*) {
        headline_view_.grab_focus();
        return false;
      },
      false);
  body_scroll_.signal_button_press_event().connect(
      [this](GdkEventButton*) {
        body_view_.grab_focus();
        return false;
      },
      false);

  head_frame_.set_shadow_type(Gtk::SHADOW_IN);
  head_frame_.add(headline_scroll_);
  body_frame_.set_shadow_type(Gtk::SHADOW_IN);
  body_frame_.add(body_scroll_);

  inner_.get_style_context()->add_class("dispatch-split");
  inner_.pack1(head_frame_, true, false);
  inner_.pack2(body_frame_, true, false);
  inner_.set_position(220);

  outer_.pack1(feed_scroll_, false, false);
  outer_.pack2(inner_, true, false);
  outer_.set_position(200);

  root_.pack_start(menubar_, Gtk::PACK_SHRINK);
  root_.pack_start(toolbar_, Gtk::PACK_SHRINK);
  root_.pack_start(outer_, Gtk::PACK_EXPAND_WIDGET);
  root_.pack_start(status_, Gtk::PACK_SHRINK);
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
  if (current_feed_ >= 0 && current_feed_ < static_cast<int>(feeds_.size()))
    feed_current_path_ = Gtk::TreeModel::Path(std::to_string(current_feed_));
}

void MainWindow::fill_headlines()
{
  headline_store_->clear();
  headline_current_path_.clear();
  headline_hover_path_.clear();
  current_headline_ = -1;
  body_view_.load({}, {}, {});
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

void MainWindow::select_feed(int index)
{
  current_feed_ = index;
  current_headline_ = -1;
  if (index >= 0 && index < static_cast<int>(feeds_.size()))
    feed_current_path_ = Gtk::TreeModel::Path(std::to_string(index));
  else
    feed_current_path_.clear();
  fill_headlines();
  feed_view_.queue_draw();
}

void MainWindow::show_preview()
{
  if (current_feed_ < 0 || current_headline_ < 0)
    return;
  const auto& items = feeds_[static_cast<size_t>(current_feed_)].items;
  if (current_headline_ >= static_cast<int>(items.size()))
    return;
  const auto& item = items[static_cast<size_t>(current_headline_)];
  body_view_.load(item.html.raw(), item.link.raw(), item.enclosures);
  mark_item(current_feed_, current_headline_, false);
}

void MainWindow::open_article()
{
  if (current_feed_ < 0 || current_headline_ < 0)
    return;
  const auto& items = feeds_[static_cast<size_t>(current_feed_)].items;
  if (current_headline_ >= static_cast<int>(items.size()))
    return;
  const auto& h = items[static_cast<size_t>(current_headline_)];
  auto* win = new ArticleWindow(h.subject, h.html.raw(), h.link.raw(), h.enclosures);
  if (auto app = get_application())
    app->add_window(*win);
  win->signal_hide().connect([win]() { delete win; });
  win->present();
}

void MainWindow::set_preview_visible(bool on)
{
  preview_visible_ = on;
  if (on)
    body_frame_.show();
  else
    body_frame_.hide();
}

void MainWindow::set_status(const Glib::ustring& text)
{
  status_.pop(status_ctx_);
  status_.push(text, status_ctx_);
}

void MainWindow::set_busy(bool on)
{
  fetching_ = on;
  btn_refresh_.set_sensitive(!on);
  btn_subscribe_.set_sensitive(!on);
}

void MainWindow::persist()
{
  settings_.feeds.clear();
  settings_.feeds.reserve(feeds_.size());
  for (const auto& f : feeds_)
    settings_.feeds.push_back({f.url.raw(), f.name.raw()});
  settings_.save();
}

void MainWindow::recount(Feed& feed)
{
  int n = 0;
  for (const auto& it : feed.items) {
    if (it.unread)
      ++n;
  }
  feed.unread = n;
}

int MainWindow::total_unread() const
{
  int n = 0;
  for (const auto& f : feeds_)
    n += f.unread;
  return n;
}

std::string MainWindow::key_of(const Item& item) const
{
  return item_key(item.link.raw(), item.subject.raw(), item.date.raw());
}

void MainWindow::apply_read_state(Feed& feed)
{
  for (auto& it : feed.items)
    it.unread = !settings_.is_read(key_of(it));
  recount(feed);
}

void MainWindow::mark_item(int feed_index, int item_index, bool unread)
{
  if (feed_index < 0 || feed_index >= static_cast<int>(feeds_.size()))
    return;
  auto& feed = feeds_[static_cast<size_t>(feed_index)];
  if (item_index < 0 || item_index >= static_cast<int>(feed.items.size()))
    return;
  auto& item = feed.items[static_cast<size_t>(item_index)];
  if (item.unread == unread)
    return;
  item.unread = unread;
  const std::string key = key_of(item);
  if (unread)
    settings_.mark_unread(key);
  else
    settings_.mark_read(key);
  recount(feed);
  persist();
  fill_feeds();
  headline_view_.queue_draw();
  feed_view_.queue_draw();
}

int MainWindow::find_url(const Glib::ustring& url) const
{
  for (int i = 0; i < static_cast<int>(feeds_.size()); ++i) {
    if (feeds_[static_cast<size_t>(i)].url == url)
      return i;
  }
  return -1;
}

void MainWindow::request_fetch(const Glib::ustring& url, int replace_index, bool select)
{
  if (fetching_) {
    fetch_queue_.push_back({url, replace_index, select});
    return;
  }
  select_after_fetch_ = select;
  start_fetch(url, replace_index);
}

void MainWindow::pump_fetch_queue()
{
  if (fetch_queue_.empty() || fetching_)
    return;
  const PendingFetch next = fetch_queue_.front();
  fetch_queue_.erase(fetch_queue_.begin());
  select_after_fetch_ = next.select;
  start_fetch(next.url, next.replace_index);
}

void MainWindow::start_fetch(const Glib::ustring& url, int replace_index)
{
  set_busy(true);
  set_status("Fetching " + url + " …");
  if (fetch_thread_.joinable())
    fetch_thread_.join();

  {
    std::lock_guard<std::mutex> lock(fetch_mutex_);
    fetch_job_ = {};
    fetch_job_.url = url;
    fetch_job_.replace_index = replace_index;
  }

  const std::string url_raw = url.raw();
  fetch_thread_ = std::thread([this, url_raw]() {
    std::string err;
    const std::string xml = http_get(url_raw, err);
    ParsedFeed parsed;
    if (err.empty())
      parsed = parse_feed(xml, url_raw);
    {
      std::lock_guard<std::mutex> lock(fetch_mutex_);
      if (!err.empty())
        fetch_job_.error = err;
      else if (!parsed.error.empty())
        fetch_job_.error = parsed.error;
      fetch_job_.parsed = std::move(parsed);
    }
    fetch_done_.emit();
  });
}

void MainWindow::on_fetch_done()
{
  FetchJob job;
  {
    std::lock_guard<std::mutex> lock(fetch_mutex_);
    job = fetch_job_;
  }
  if (fetch_thread_.joinable())
    fetch_thread_.join();
  set_busy(false);

  int idx = job.replace_index;
  if (!job.error.empty()) {
    set_status("Fetch failed: " + u8(job.error));
    pump_fetch_queue();
    return;
  }

  Feed f;
  f.url = job.url;
  f.name = u8(job.parsed.title);
  if (f.name.empty())
    f.name = job.url;
  f.items.reserve(job.parsed.items.size());
  for (const auto& h : job.parsed.items) {
    Item it;
    it.subject = u8(h.subject);
    it.date = u8(h.date);
    it.html = u8(h.html);
    it.link = u8(h.link);
    it.enclosures = h.enclosures;
    f.items.push_back(std::move(it));
  }
  apply_read_state(f);

  if (idx >= 0 && idx < static_cast<int>(feeds_.size())) {
    feeds_[static_cast<size_t>(idx)] = std::move(f);
  } else {
    feeds_.push_back(std::move(f));
    idx = static_cast<int>(feeds_.size()) - 1;
  }
  persist();
  fill_feeds();
  if (select_after_fetch_)
    select_feed(idx);
  else if (idx == current_feed_)
    fill_headlines();
  set_status(Glib::ustring::compose("%1 — %2 items, %3 unread",
                                    feeds_[static_cast<size_t>(idx)].name,
                                    feeds_[static_cast<size_t>(idx)].items.size(),
                                    total_unread()));
  pump_fetch_queue();
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
  SubscribeDialog dlg(*this);
  if (dlg.run() != Gtk::RESPONSE_OK)
    return;
  const Glib::ustring url = dlg.url();
  if (url.empty()) {
    set_status("Enter a feed URL.");
    return;
  }
  int idx = find_url(url);
  if (idx < 0) {
    Feed f;
    f.url = url;
    f.name = url;
    feeds_.push_back(std::move(f));
    idx = static_cast<int>(feeds_.size()) - 1;
    fill_feeds();
    persist();
  }
  request_fetch(url, idx, true);
}

void MainWindow::on_unsubscribe()
{
  if (current_feed_ < 0 || current_feed_ >= static_cast<int>(feeds_.size())) {
    set_status("No feed selected.");
    return;
  }
  const int gone = current_feed_;
  feeds_.erase(feeds_.begin() + gone);
  persist();
  fill_feeds();
  if (feeds_.empty()) {
    select_feed(-1);
    set_status("No feeds. Subscribe… to add a URL.");
    return;
  }
  int next = gone;
  if (next >= static_cast<int>(feeds_.size()))
    next = static_cast<int>(feeds_.size()) - 1;
  select_feed(next);
  set_status("Unsubscribed.");
}

void MainWindow::on_import_opml()
{
  Gtk::FileChooserDialog dlg(*this, "Import OPML", Gtk::FILE_CHOOSER_ACTION_OPEN);
  dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dlg.add_button("_Open", Gtk::RESPONSE_OK);
  auto opml = Gtk::FileFilter::create();
  opml->set_name("OPML");
  opml->add_pattern("*.opml");
  opml->add_pattern("*.xml");
  dlg.add_filter(opml);
  auto all = Gtk::FileFilter::create();
  all->set_name("All files");
  all->add_pattern("*");
  dlg.add_filter(all);
  if (dlg.run() != Gtk::RESPONSE_OK)
    return;

  std::string err;
  const auto outlines = parse_opml_file(dlg.get_filename(), err);
  if (!err.empty() && outlines.empty()) {
    set_status("Import failed: " + u8(err));
    return;
  }

  int added = 0;
  int skipped = 0;
  int first_new = -1;
  for (const auto& o : outlines) {
    if (o.url.compare(0, 7, "http://") != 0 && o.url.compare(0, 8, "https://") != 0) {
      ++skipped;
      continue;
    }
    const Glib::ustring url = u8(o.url);
    if (find_url(url) >= 0) {
      ++skipped;
      continue;
    }
    Feed f;
    f.url = url;
    f.name = o.title.empty() ? url : u8(o.title);
    feeds_.push_back(std::move(f));
    const int idx = static_cast<int>(feeds_.size()) - 1;
    if (first_new < 0)
      first_new = idx;
    ++added;
    request_fetch(url, idx, false);
  }
  persist();
  fill_feeds();
  if (first_new >= 0)
    select_feed(first_new);
  if (added == 0)
    set_status(skipped ? "Import: nothing new." : "Import: no feeds found.");
  else
    set_status(Glib::ustring::compose("Imported %1 feed(s), skipped %2.", added, skipped));
}

void MainWindow::on_export_opml()
{
  if (feeds_.empty()) {
    set_status("Nothing to export.");
    return;
  }
  Gtk::FileChooserDialog dlg(*this, "Export OPML", Gtk::FILE_CHOOSER_ACTION_SAVE);
  dlg.set_do_overwrite_confirmation(true);
  dlg.set_current_name("subscriptions.opml");
  dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dlg.add_button("_Save", Gtk::RESPONSE_OK);
  auto opml = Gtk::FileFilter::create();
  opml->set_name("OPML");
  opml->add_pattern("*.opml");
  dlg.add_filter(opml);
  if (dlg.run() != Gtk::RESPONSE_OK)
    return;

  std::vector<OpmlOutline> outlines;
  outlines.reserve(feeds_.size());
  for (const auto& f : feeds_)
    outlines.push_back({f.url.raw(), f.name.raw()});
  std::string err;
  if (!write_opml_file(dlg.get_filename(), outlines, err)) {
    set_status("Export failed: " + u8(err));
    return;
  }
  set_status(Glib::ustring::compose("Exported %1 feed(s).", outlines.size()));
}

void MainWindow::on_refresh()
{
  if (current_feed_ < 0 || current_feed_ >= static_cast<int>(feeds_.size())) {
    set_status("Subscribe to a feed first.");
    return;
  }
  request_fetch(feeds_[static_cast<size_t>(current_feed_)].url, current_feed_, false);
}

void MainWindow::on_refresh_all()
{
  if (feeds_.empty()) {
    set_status("Subscribe to a feed first.");
    return;
  }
  for (int i = 0; i < static_cast<int>(feeds_.size()); ++i)
    request_fetch(feeds_[static_cast<size_t>(i)].url, i, false);
}

void MainWindow::on_mark_read()
{
  if (current_feed_ < 0 || current_feed_ >= static_cast<int>(feeds_.size()))
    return;
  if (current_headline_ >= 0) {
    mark_item(current_feed_, current_headline_, false);
    return;
  }
  auto& feed = feeds_[static_cast<size_t>(current_feed_)];
  for (int i = 0; i < static_cast<int>(feed.items.size()); ++i)
    mark_item(current_feed_, i, false);
}

void MainWindow::on_toggle_unread()
{
  if (current_feed_ < 0 || current_feed_ >= static_cast<int>(feeds_.size()) ||
      current_headline_ < 0) {
    set_status("Select a headline.");
    return;
  }
  const auto& items = feeds_[static_cast<size_t>(current_feed_)].items;
  if (current_headline_ >= static_cast<int>(items.size()))
    return;
  const bool unread = items[static_cast<size_t>(current_headline_)].unread;
  mark_item(current_feed_, current_headline_, !unread);
}

void MainWindow::on_toggle_preview()
{
  if (!view_preview_item_)
    return;
  set_preview_visible(view_preview_item_->get_active());
}

void MainWindow::apply_appearance()
{
  BodyView::apply_all(settings_.font_family, settings_.font_size, settings_.font_weight,
                      settings_.palette);
}

void MainWindow::on_appearance()
{
  FontDialog dlg(*this, settings_, [this]() { apply_appearance(); });
  if (dlg.run() == Gtk::RESPONSE_OK)
    persist();
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
  auto* text = dynamic_cast<Gtk::CellRendererText*>(cell);
  if (!text)
    return;
  if (current_feed_ < 0 || current_feed_ >= static_cast<int>(feeds_.size()))
    return;
  const int i = (*it)[col_headline_index_];
  const auto& items = feeds_[static_cast<size_t>(current_feed_)].items;
  const bool unread = i >= 0 && i < static_cast<int>(items.size()) &&
                      items[static_cast<size_t>(i)].unread;
  text->property_weight() = unread ? Pango::WEIGHT_BOLD : Pango::WEIGHT_NORMAL;
}

bool MainWindow::on_feed_motion(GdkEventMotion* event)
{
  return nav_motion(feed_view_, feed_hover_path_, event);
}

bool MainWindow::on_feed_leave(GdkEventCrossing* event)
{
  return nav_leave(feed_view_, feed_hover_path_, event);
}

void MainWindow::step_feed(int delta)
{
  if (feeds_.empty())
    return;
  int i = current_feed_;
  if (i < 0)
    i = delta >= 0 ? 0 : static_cast<int>(feeds_.size()) - 1;
  else
    i += delta;
  if (i < 0)
    i = 0;
  if (i >= static_cast<int>(feeds_.size()))
    i = static_cast<int>(feeds_.size()) - 1;
  select_feed(i);
  if (feed_current_path_.size() > 0) {
    if (auto* col = feed_view_.get_column(0))
      feed_view_.scroll_to_cell(feed_current_path_, *col);
  }
  feed_view_.queue_draw();
}

void MainWindow::step_headline(int delta)
{
  if (current_feed_ < 0 || current_feed_ >= static_cast<int>(feeds_.size()))
    return;
  const auto& items = feeds_[static_cast<size_t>(current_feed_)].items;
  if (items.empty())
    return;
  int i = current_headline_;
  if (i < 0)
    i = delta >= 0 ? 0 : static_cast<int>(items.size()) - 1;
  else
    i += delta;
  if (i < 0)
    i = 0;
  if (i >= static_cast<int>(items.size()))
    i = static_cast<int>(items.size()) - 1;
  current_headline_ = i;
  headline_current_path_ = Gtk::TreeModel::Path(std::to_string(i));
  show_preview();
  if (auto* col = headline_view_.get_column(0))
    headline_view_.scroll_to_cell(headline_current_path_, *col);
  headline_view_.queue_draw();
}

bool MainWindow::on_feed_button(GdkEventButton* event)
{
  if (!event || event->type != GDK_BUTTON_PRESS)
    return false;
  feed_view_.grab_focus();
  Gtk::TreeModel::Path path;
  if (!path_at_bin_event(feed_view_, event->window, event->x, event->y, path))
    return false;
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
  if (event->type == GDK_BUTTON_PRESS)
    headline_view_.grab_focus();
  Gtk::TreeModel::Path path;
  if (!path_at_bin_event(headline_view_, event->window, event->x, event->y, path))
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

bool MainWindow::on_feed_key(GdkEventKey* event)
{
  if (!event)
    return false;
  const int n = static_cast<int>(feeds_.size());
  switch (event->keyval) {
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
      step_feed(-1);
      return true;
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      step_feed(1);
      return true;
    case GDK_KEY_Page_Up:
    case GDK_KEY_KP_Page_Up:
      step_feed(-8);
      return true;
    case GDK_KEY_Page_Down:
    case GDK_KEY_KP_Page_Down:
      step_feed(8);
      return true;
    case GDK_KEY_Home:
    case GDK_KEY_KP_Home:
      if (n > 0)
        step_feed(-n);
      return true;
    case GDK_KEY_End:
    case GDK_KEY_KP_End:
      if (n > 0)
        step_feed(n);
      return true;
    default:
      return false;
  }
}

bool MainWindow::on_headline_key(GdkEventKey* event)
{
  if (!event)
    return false;
  if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
    open_article();
    return true;
  }
  int n = 0;
  if (current_feed_ >= 0 && current_feed_ < static_cast<int>(feeds_.size()))
    n = static_cast<int>(feeds_[static_cast<size_t>(current_feed_)].items.size());
  switch (event->keyval) {
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
      step_headline(-1);
      return true;
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      step_headline(1);
      return true;
    case GDK_KEY_Page_Up:
    case GDK_KEY_KP_Page_Up:
      step_headline(-8);
      return true;
    case GDK_KEY_Page_Down:
    case GDK_KEY_KP_Page_Down:
      step_headline(8);
      return true;
    case GDK_KEY_Home:
    case GDK_KEY_KP_Home:
      if (n > 0)
        step_headline(-n);
      return true;
    case GDK_KEY_End:
    case GDK_KEY_KP_End:
      if (n > 0)
        step_headline(n);
      return true;
    default:
      return false;
  }
}

}  // namespace dispatch
