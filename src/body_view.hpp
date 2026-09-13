/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "feed.hpp"

#include <gtkmm.h>
#include <libxml/tree.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace dispatch {

class BodyView : public Gtk::TextView {
 public:
  BodyView();
  ~BodyView() override;

  void load(const std::string& html, const std::string& base_url,
            const std::vector<Enclosure>& extra);
  void apply_appearance(const std::string& family, int size_pt, int weight, int palette);
  static void apply_all(const std::string& family, int size_pt, int weight, int palette);

 protected:
  bool on_button_release_event(GdkEventButton* event) override;
  bool on_motion_notify_event(GdkEventMotion* event) override;

 private:
  struct ImgSlot {
    Glib::RefPtr<Gtk::TextBuffer::Mark> mark;
    std::string url;
  };
  struct ImgReady {
    Glib::RefPtr<Gtk::TextBuffer::Mark> mark;
    Glib::RefPtr<Gdk::Pixbuf> pix;
  };

  void ensure_tags();
  void clear_body();
  void stop_images();
  void start_images();
  void on_img_done();
  void walk(xmlNode* node, int list_depth);
  void insert_text(const std::string& text, const std::vector<Glib::ustring>& tags);
  void ensure_break();
  void ensure_paragraph();
  void pad_space();
  bool in_pre() const;
  gunichar last_char() const;
  void insert_image_slot(const std::string& url, const std::string& alt);
  void insert_media_button(const std::string& url, const std::string& kind,
                           const std::string& mime = {});
  void open_uri(const std::string& uri, const std::string& mime = {});
  std::string resolve(const std::string& src) const;
  bool already_used(const std::string& url) const;

  Glib::RefPtr<Gtk::TextBuffer> buf_;
  std::vector<Glib::ustring> tag_stack_;
  std::map<Glib::RefPtr<Gtk::TextTag>, std::string> link_hrefs_;
  std::string base_url_;
  std::vector<std::string> used_urls_;
  std::vector<ImgSlot> img_slots_;
  std::vector<Gtk::Widget*> extras_;

  GCancellable* cancel_ = nullptr;
  std::thread img_thread_;
  Glib::Dispatcher img_done_;
  sigc::connection img_conn_;
  std::mutex img_mu_;
  std::vector<ImgReady> img_ready_;
  std::atomic<bool> stop_images_{false};
  Glib::ustring link_color_{"#0B3A96"};

  static std::vector<BodyView*> live_;
  static std::string appear_family_;
  static int appear_size_;
  static int appear_weight_;
  static int appear_palette_;
};

}  // namespace dispatch
