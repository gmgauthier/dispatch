/* SPDX-License-Identifier: Unlicense */

#include "account_dialog.hpp"

namespace dispatch {
namespace {

Glib::ustring trim(const Glib::ustring& u)
{
  auto b = u.begin();
  auto e = u.end();
  while (b != e && (*b == ' ' || *b == '\t'))
    ++b;
  while (e != b) {
    auto p = e;
    --p;
    if (*p != ' ' && *p != '\t')
      break;
    e = p;
  }
  return Glib::ustring(b, e);
}

void fill_tls(Gtk::ComboBoxText& box, const std::string& id)
{
  box.append("starttls", "STARTTLS");
  box.append("implicit", "Implicit TLS");
  if (id == "implicit")
    box.set_active_id("implicit");
  else
    box.set_active_id("starttls");
}

void setup_port(Gtk::SpinButton& port, int value)
{
  port.set_range(1, 65535);
  port.set_increments(1, 10);
  port.set_digits(0);
  port.set_numeric(true);
  int v = value;
  if (v < 1 || v > 65535)
    v = 1;
  port.set_value(v);
}

Gtk::Label* lab(const Glib::ustring& text, Gtk::Widget& for_widget)
{
  auto* l = Gtk::manage(new Gtk::Label(text, true));
  l->set_halign(Gtk::ALIGN_START);
  l->set_mnemonic_widget(for_widget);
  return l;
}

}  // namespace

AccountDialog::AccountDialog(Gtk::Window& parent, const Settings& settings)
    : Gtk::Dialog("Account", parent, true)
{
  set_resizable(false);
  add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  add_button("_OK", Gtk::RESPONSE_OK);
  set_default_response(Gtk::RESPONSE_OK);

  imap_host_.set_placeholder_text("127.0.0.1");
  smtp_host_.set_placeholder_text("127.0.0.1");
  imap_host_.set_width_chars(28);
  smtp_host_.set_width_chars(28);
  user_.set_width_chars(28);
  password_.set_width_chars(28);
  password_.set_visibility(false);
  password_.set_input_purpose(Gtk::INPUT_PURPOSE_PASSWORD);

  imap_host_.set_text(settings.imap_host);
  smtp_host_.set_text(settings.smtp_host);
  user_.set_text(settings.mail_user);
  password_.set_text(settings.mail_password);
  setup_port(imap_port_, settings.imap_port);
  setup_port(smtp_port_, settings.smtp_port);
  fill_tls(imap_tls_, settings.imap_tls);
  fill_tls(smtp_tls_, settings.smtp_tls);

  auto* grid = Gtk::manage(new Gtk::Grid());
  grid->set_column_spacing(8);
  grid->set_row_spacing(6);
  int row = 0;

  auto* imap = Gtk::manage(new Gtk::Label("IMAP"));
  imap->set_halign(Gtk::ALIGN_START);
  grid->attach(*imap, 0, row, 4, 1);
  ++row;
  grid->attach(*lab("H_ost", imap_host_), 0, row, 1, 1);
  grid->attach(imap_host_, 1, row, 3, 1);
  ++row;
  grid->attach(*lab("_Port", imap_port_), 0, row, 1, 1);
  grid->attach(imap_port_, 1, row, 1, 1);
  grid->attach(*lab("Securit_y", imap_tls_), 2, row, 1, 1);
  grid->attach(imap_tls_, 3, row, 1, 1);
  ++row;

  auto* smtp = Gtk::manage(new Gtk::Label("SMTP"));
  smtp->set_halign(Gtk::ALIGN_START);
  smtp->set_margin_top(8);
  grid->attach(*smtp, 0, row, 4, 1);
  ++row;
  grid->attach(*lab("Ho_st", smtp_host_), 0, row, 1, 1);
  grid->attach(smtp_host_, 1, row, 3, 1);
  ++row;
  grid->attach(*lab("P_ort", smtp_port_), 0, row, 1, 1);
  grid->attach(smtp_port_, 1, row, 1, 1);
  grid->attach(*lab("Sec_urity", smtp_tls_), 2, row, 1, 1);
  grid->attach(smtp_tls_, 3, row, 1, 1);
  ++row;

  auto* signin = Gtk::manage(new Gtk::Label("Sign-in"));
  signin->set_halign(Gtk::ALIGN_START);
  signin->set_margin_top(8);
  grid->attach(*signin, 0, row, 4, 1);
  ++row;
  grid->attach(*lab("_Username", user_), 0, row, 1, 1);
  grid->attach(user_, 1, row, 3, 1);
  ++row;
  grid->attach(*lab("Pass_word", password_), 0, row, 1, 1);
  grid->attach(password_, 1, row, 3, 1);

  auto* box = get_content_area();
  box->set_border_width(12);
  box->set_spacing(8);
  box->pack_start(*grid, Gtk::PACK_SHRINK);
  show_all_children();
}

void AccountDialog::apply_to(Settings& settings) const
{
  settings.imap_host = trim(imap_host_.get_text()).raw();
  settings.smtp_host = trim(smtp_host_.get_text()).raw();
  settings.mail_user = trim(user_.get_text()).raw();
  settings.mail_password = password_.get_text().raw();
  settings.imap_port = imap_port_.get_value_as_int();
  settings.smtp_port = smtp_port_.get_value_as_int();
  const std::string imap_id = imap_tls_.get_active_id();
  settings.imap_tls = (imap_id == "implicit") ? "implicit" : "starttls";
  const std::string smtp_id = smtp_tls_.get_active_id();
  settings.smtp_tls = (smtp_id == "implicit") ? "implicit" : "starttls";
}

}  // namespace dispatch
