/* SPDX-License-Identifier: Unlicense */

#include "article_window.hpp"

namespace dispatch {

ArticleWindow::ArticleWindow(const Glib::ustring& subject, const std::string& html,
                             const std::string& base_url, const std::vector<Enclosure>& extra)
{
  set_title(subject.empty() ? "Dispatch" : subject);
  set_default_size(560, 420);
  get_style_context()->add_class("dispatch-article");

  auto* scroll = Gtk::manage(new Gtk::ScrolledWindow());
  scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  auto* view = Gtk::manage(new BodyView());
  view->get_style_context()->add_class("dispatch-article-body");
  view->load(html, base_url, extra);
  scroll->add(*view);
  add(*scroll);
  show_all();
}

}  // namespace dispatch
