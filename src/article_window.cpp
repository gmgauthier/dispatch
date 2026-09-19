/* SPDX-License-Identifier: Unlicense */

#include "article_window.hpp"

namespace dispatch {
namespace {

void pack_mail_header(Gtk::Box& root, const MessageHeader& hdr)
{
  if (hdr.from.empty() && hdr.to.empty() && hdr.subject.empty())
    return;
  auto* grid = Gtk::manage(new Gtk::Grid());
  grid->set_column_spacing(8);
  grid->set_row_spacing(2);
  grid->set_border_width(6);
  grid->get_style_context()->add_class("dispatch-mail-hdr");
  auto add = [&](int row, const char* lab, const Glib::ustring& val, bool hide_empty) {
    if (hide_empty && val.empty())
      return;
    auto* l = Gtk::manage(new Gtk::Label(lab, Gtk::ALIGN_END));
    l->set_valign(Gtk::ALIGN_START);
    auto* v = Gtk::manage(new Gtk::Label());
    v->set_text(val);
    v->set_xalign(0.0);
    v->set_line_wrap(true);
    v->set_selectable(true);
    v->set_hexpand(true);
    grid->attach(*l, 0, row, 1, 1);
    grid->attach(*v, 1, row, 1, 1);
  };
  add(0, "From:", hdr.from, false);
  add(1, "To:", hdr.to, false);
  add(2, "Cc:", hdr.cc, true);
  add(3, "Date:", hdr.date, false);
  add(4, "Subject:", hdr.subject, false);
  root.pack_start(*grid, Gtk::PACK_SHRINK);
}

}  // namespace

ArticleWindow::ArticleWindow(const Glib::ustring& subject, const std::string& html,
                             const std::string& base_url, const std::vector<Enclosure>& extra,
                             MessageHeader hdr)
{
  set_title(subject.empty() ? "Dispatch" : subject);
  set_default_size(560, 420);
  get_style_context()->add_class("dispatch-article");

  auto* root = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
  pack_mail_header(*root, hdr);
  auto* scroll = Gtk::manage(new Gtk::ScrolledWindow());
  scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  auto* view = Gtk::manage(new BodyView());
  view->get_style_context()->add_class("dispatch-article-body");
  view->load(html, base_url, extra);
  scroll->add(*view);
  root->pack_start(*scroll, Gtk::PACK_EXPAND_WIDGET);
  add(*root);
  show_all();
}

}  // namespace dispatch
