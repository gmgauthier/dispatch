/* SPDX-License-Identifier: Unlicense */

#include "compose_window.hpp"
#include "ephemeris_contacts.hpp"
#include "mail_parse.hpp"
#include "mail_smtp.hpp"
#include "mail_store.hpp"

#include <cstdint>

namespace dispatch {

ComposeWindow::ComposeWindow(const Settings& settings,
                             std::function<void(bool sent, std::string error)> done,
                             ComposeFill fill)
    : settings_(settings),
      done_(std::move(done))
{
  set_title(fill.title.empty() ? "New Mail" : fill.title);
  set_default_size(560, 420);
  get_style_context()->add_class("dispatch-window");

  from_label_.set_text(settings_.mail_user.empty() ? "(no account)" : settings_.mail_user);
  from_label_.set_halign(Gtk::ALIGN_START);
  from_label_.set_selectable(true);
  to_.set_hexpand(true);
  cc_.set_hexpand(true);
  subject_.set_hexpand(true);
  to_.set_activates_default(false);
  body_.set_wrap_mode(Gtk::WRAP_WORD_CHAR);
  body_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  body_scroll_.add(body_);
  body_scroll_.set_hexpand(true);
  body_scroll_.set_vexpand(true);

  grid_.set_column_spacing(8);
  grid_.set_row_spacing(6);
  int row = 0;
  grid_.attach(*Gtk::manage(new Gtk::Label("From", Gtk::ALIGN_START)), 0, row, 1, 1);
  grid_.attach(from_label_, 1, row, 1, 1);
  ++row;
  auto* to_l = Gtk::manage(new Gtk::Label("_To", true));
  to_l->set_halign(Gtk::ALIGN_START);
  to_l->set_mnemonic_widget(to_);
  grid_.attach(*to_l, 0, row, 1, 1);
  auto* to_row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
  to_row->pack_start(to_, Gtk::PACK_EXPAND_WIDGET);
  auto* btn_to = Gtk::manage(new Gtk::Button("…"));
  btn_to->set_tooltip_text("Choose from Ephemeris");
  btn_to->signal_clicked().connect([this]() { on_pick(to_); });
  to_row->pack_start(*btn_to, Gtk::PACK_SHRINK);
  grid_.attach(*to_row, 1, row, 1, 1);
  ++row;
  auto* cc_l = Gtk::manage(new Gtk::Label("_Cc", true));
  cc_l->set_halign(Gtk::ALIGN_START);
  cc_l->set_mnemonic_widget(cc_);
  grid_.attach(*cc_l, 0, row, 1, 1);
  auto* cc_row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
  cc_row->pack_start(cc_, Gtk::PACK_EXPAND_WIDGET);
  auto* btn_cc = Gtk::manage(new Gtk::Button("…"));
  btn_cc->set_tooltip_text("Choose from Ephemeris");
  btn_cc->signal_clicked().connect([this]() { on_pick(cc_); });
  cc_row->pack_start(*btn_cc, Gtk::PACK_SHRINK);
  grid_.attach(*cc_row, 1, row, 1, 1);
  ++row;
  auto* sub_l = Gtk::manage(new Gtk::Label("_Subject", true));
  sub_l->set_halign(Gtk::ALIGN_START);
  sub_l->set_mnemonic_widget(subject_);
  grid_.attach(*sub_l, 0, row, 1, 1);
  grid_.attach(subject_, 1, row, 1, 1);

  btn_send_.signal_clicked().connect(sigc::mem_fun(*this, &ComposeWindow::on_send));
  buttons_.pack_start(btn_send_, Gtk::PACK_SHRINK);
  buttons_.pack_start(status_, Gtk::PACK_EXPAND_WIDGET);
  status_.set_halign(Gtk::ALIGN_START);

  root_.set_border_width(12);
  root_.pack_start(grid_, Gtk::PACK_SHRINK);
  root_.pack_start(body_scroll_, Gtk::PACK_EXPAND_WIDGET);
  root_.pack_start(buttons_, Gtk::PACK_SHRINK);
  add(root_);
  show_all();

  send_conn_ = send_done_.connect(sigc::mem_fun(*this, &ComposeWindow::on_send_done));
  if (!fill.to.empty())
    to_.set_text(fill.to);
  if (!fill.cc.empty())
    cc_.set_text(fill.cc);
  if (!fill.subject.empty())
    subject_.set_text(fill.subject);
  if (!fill.body.empty())
    body_.get_buffer()->set_text(fill.body);
  in_reply_to_ = fill.in_reply_to;
  references_ = fill.references;
}

ComposeWindow::~ComposeWindow()
{
  send_conn_.disconnect();
  if (send_thread_.joinable())
    send_thread_.join();
}

void ComposeWindow::on_pick(Gtk::Entry& dest)
{
  const auto people = load_ephemeris_emails();
  if (people.empty()) {
    status_.set_text("No email addresses in the current Ephemeris binder.");
    return;
  }

  Gtk::Dialog dlg("Ephemeris contacts", *this, true);
  dlg.set_default_size(420, 320);
  dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dlg.add_button("_Add", Gtk::RESPONSE_OK);
  dlg.set_default_response(Gtk::RESPONSE_OK);

  Gtk::TreeModelColumn<Glib::ustring> col_name;
  Gtk::TreeModelColumn<Glib::ustring> col_email;
  Gtk::TreeModelColumnRecord cols;
  cols.add(col_name);
  cols.add(col_email);
  auto store = Gtk::ListStore::create(cols);
  for (const auto& p : people) {
    auto it = store->append();
    (*it)[col_name] = p.name;
    (*it)[col_email] = p.email;
  }
  Gtk::TreeView view(store);
  view.append_column("Name", col_name);
  view.append_column("Email", col_email);
  view.set_headers_visible(true);
  view.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
  Gtk::ScrolledWindow scroll;
  scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  scroll.add(view);
  dlg.get_content_area()->set_border_width(8);
  dlg.get_content_area()->pack_start(scroll, Gtk::PACK_EXPAND_WIDGET);
  dlg.show_all();
  if (dlg.run() != Gtk::RESPONSE_OK)
    return;

  std::vector<Glib::ustring> add;
  view.get_selection()->selected_foreach(
      [&](const Gtk::TreeModel::Path&, const Gtk::TreeModel::iterator& it) {
        const Glib::ustring email = (*it)[col_email];
        const Glib::ustring name = (*it)[col_name];
        if (email.empty())
          return;
        if (name.empty())
          add.push_back(email);
        else
          add.push_back(name + " <" + email + ">");
      });
  if (add.empty())
    return;
  Glib::ustring cur = dest.get_text();
  auto b = cur.begin();
  auto e = cur.end();
  while (e != b) {
    auto p = e;
    --p;
    if (*p != ' ' && *p != '\t' && *p != ',')
      break;
    e = p;
  }
  cur = Glib::ustring(b, e);
  for (const auto& a : add) {
    if (!cur.empty())
      cur += ", ";
    cur += a;
  }
  dest.set_text(cur);
}

void ComposeWindow::on_send()
{
  if (send_thread_.joinable())
    return;
  const auto to = split_addresses(to_.get_text().raw());
  const auto cc = split_addresses(cc_.get_text().raw());
  if (to.empty() && cc.empty()) {
    status_.set_text("To is required.");
    return;
  }
  if (settings_.smtp_host.empty() || settings_.mail_user.empty()) {
    status_.set_text("Set Options → Account… first.");
    return;
  }

  std::vector<std::string> rcpt = to;
  rcpt.insert(rcpt.end(), cc.begin(), cc.end());
  const std::string rfc822 =
      build_rfc822(settings_.mail_user, to, cc, subject_.get_text().raw(),
                   body_.get_buffer()->get_text().raw(), in_reply_to_, references_);
  if (rfc822.empty()) {
    status_.set_text("Could not build the message.");
    return;
  }

  btn_send_.set_sensitive(false);
  status_.set_text("Sending…");
  SmtpAccount acct;
  acct.host = settings_.smtp_host;
  acct.port = static_cast<uint16_t>(settings_.smtp_port);
  acct.starttls = settings_.smtp_tls != "implicit";
  acct.user = settings_.mail_user;
  acct.password = settings_.mail_password;
  const std::string from = settings_.mail_user;
  send_thread_ = std::thread([this, acct, from, rcpt, rfc822]() {
    const SmtpResult r = smtp_send(acct, from, rcpt, rfc822);
    {
      std::lock_guard<std::mutex> lock(mu_);
      send_ok_ = r.error.empty();
      send_error_ = r.error;
    }
    if (r.error.empty())
      folder_write(kFolderSent, rfc822.data(), rfc822.size(), true);
    else
      folder_write(kFolderOutbox, rfc822.data(), rfc822.size(), true);
    send_done_.emit();
  });
}

void ComposeWindow::on_send_done()
{
  bool ok = false;
  std::string err;
  {
    std::lock_guard<std::mutex> lock(mu_);
    ok = send_ok_;
    err = send_error_;
  }
  if (send_thread_.joinable())
    send_thread_.join();
  if (done_)
    done_(ok, err);
  Glib::signal_idle().connect_once([this]() { hide(); });
}

}  // namespace dispatch
