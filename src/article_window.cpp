/* SPDX-License-Identifier: Unlicense */

#include "article_window.hpp"

namespace dispatch {

ArticleWindow::ArticleWindow(const Glib::ustring& subject, const Glib::ustring& body)
{
  set_title(subject.empty() ? "Dispatch" : subject);
  set_default_size(560, 420);
  get_style_context()->add_class("dispatch-article");

  auto* scroll = Gtk::manage(new Gtk::ScrolledWindow());
  scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  auto* view = Gtk::manage(new Gtk::TextView());
  view->set_editable(false);
  view->set_wrap_mode(Gtk::WRAP_WORD_CHAR);
  view->set_left_margin(10);
  view->set_right_margin(10);
  view->set_top_margin(10);
  view->set_bottom_margin(8);
  view->get_style_context()->add_class("dispatch-article-body");
  view->get_buffer()->set_text(body);
  scroll->add(*view);
  add(*scroll);
  show_all();
}

}  // namespace dispatch
