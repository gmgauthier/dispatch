/* SPDX-License-Identifier: Unlicense */

#include "body_view.hpp"
#include "fetch.hpp"

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxml/HTMLparser.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>

namespace dispatch {

std::vector<BodyView*> BodyView::live_;
std::string BodyView::appear_family_ = "Sans";
int BodyView::appear_size_ = 12;
int BodyView::appear_weight_ = 400;
int BodyView::appear_palette_ = 1;

namespace {

std::string lower(std::string s)
{
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string xml_name(xmlNode* n)
{
  if (!n || !n->name)
    return {};
  return lower(reinterpret_cast<const char*>(n->name));
}

std::string xml_prop(xmlNode* n, const char* key)
{
  xmlChar* v = xmlGetProp(n, BAD_CAST key);
  if (!v)
    return {};
  std::string out(reinterpret_cast<const char*>(v));
  xmlFree(v);
  return out;
}

bool skip_tag(const std::string& name)
{
  return name == "script" || name == "style" || name == "svg" || name == "head" || name == "meta" ||
         name == "link" || name == "title" || name == "noscript";
}

std::string url_path(const std::string& url)
{
  std::string u = url;
  const auto q = u.find('?');
  if (q != std::string::npos)
    u = u.substr(0, q);
  const auto h = u.find('#');
  if (h != std::string::npos)
    u = u.substr(0, h);
  return lower(u);
}

bool has_ext(const std::string& path, const char* ext)
{
  const size_t n = std::strlen(ext);
  return path.size() >= n && path.compare(path.size() - n, n, ext) == 0;
}

bool looks_image(const std::string& url, const std::string& type)
{
  const std::string t = lower(type);
  if (t.compare(0, 6, "image/") == 0 && t.find("svg") == std::string::npos)
    return true;
  const std::string p = url_path(url);
  return has_ext(p, ".jpg") || has_ext(p, ".jpeg") || has_ext(p, ".png") || has_ext(p, ".gif") ||
         has_ext(p, ".webp");
}

bool looks_audio(const std::string& url, const std::string& type)
{
  const std::string t = lower(type);
  if (t.compare(0, 6, "audio/") == 0)
    return true;
  const std::string p = url_path(url);
  return has_ext(p, ".mp3") || has_ext(p, ".m4a") || has_ext(p, ".ogg") || has_ext(p, ".wav") ||
         has_ext(p, ".aac") || has_ext(p, ".opus") || has_ext(p, ".flac");
}

std::string mime_from_url(const std::string& url)
{
  const std::string p = url_path(url);
  if (has_ext(p, ".mp3"))
    return "audio/mpeg";
  if (has_ext(p, ".m4a") || has_ext(p, ".aac"))
    return "audio/mp4";
  if (has_ext(p, ".ogg") || has_ext(p, ".opus"))
    return "audio/ogg";
  if (has_ext(p, ".wav"))
    return "audio/wav";
  if (has_ext(p, ".flac"))
    return "audio/flac";
  if (has_ext(p, ".mp4") || has_ext(p, ".m4v"))
    return "video/mp4";
  if (has_ext(p, ".webm"))
    return "video/webm";
  if (has_ext(p, ".mov"))
    return "video/quicktime";
  if (has_ext(p, ".ogv"))
    return "video/ogg";
  return {};
}

std::string media_mime(const std::string& url, const std::string& type, const std::string& kind)
{
  const std::string t = lower(type);
  if (t.find('/') != std::string::npos && t.find("octet-stream") == std::string::npos &&
      t.find("text/html") == std::string::npos)
    return t;
  std::string from_url = mime_from_url(url);
  if (!from_url.empty())
    return from_url;
  if (kind == "audio")
    return "audio/mpeg";
  if (kind == "video")
    return "video/mp4";
  return {};
}

bool looks_video(const std::string& url, const std::string& type)
{
  const std::string t = lower(type);
  if (t.compare(0, 6, "video/") == 0)
    return true;
  const std::string p = url_path(url);
  return has_ext(p, ".mp4") || has_ext(p, ".webm") || has_ext(p, ".mov") || has_ext(p, ".m4v") ||
         has_ext(p, ".ogv");
}

Glib::RefPtr<Gdk::Pixbuf> pixbuf_from_bytes(const std::string& bytes)
{
  if (bytes.empty())
    return {};
  try {
    auto loader = Gdk::PixbufLoader::create();
    loader->write(reinterpret_cast<const guint8*>(bytes.data()), bytes.size());
    loader->close();
    auto pix = loader->get_pixbuf();
    if (!pix)
      return {};
    const int maxw = 520;
    const int maxh = 720;
    int w = pix->get_width();
    int h = pix->get_height();
    if (w <= 0 || h <= 0)
      return pix;
    double scale = 1.0;
    if (w > maxw)
      scale = static_cast<double>(maxw) / w;
    if (h * scale > maxh)
      scale = static_cast<double>(maxh) / h;
    if (scale < 1.0) {
      const int nw = std::max(1, static_cast<int>(w * scale));
      const int nh = std::max(1, static_cast<int>(h * scale));
      pix = pix->scale_simple(nw, nh, Gdk::INTERP_BILINEAR);
    }
    return pix;
  } catch (const Glib::Error&) {
    return {};
  }
}

}  // namespace

BodyView::BodyView()
{
  set_editable(false);
  set_wrap_mode(Gtk::WRAP_WORD_CHAR);
  set_cursor_visible(false);
  set_left_margin(10);
  set_right_margin(10);
  set_top_margin(10);
  set_bottom_margin(8);
  add_events(Gdk::POINTER_MOTION_MASK);
  get_style_context()->add_class("dispatch-body");
  buf_ = get_buffer();
  ensure_tags();
  img_conn_ = img_done_.connect(sigc::mem_fun(*this, &BodyView::on_img_done));
  live_.push_back(this);
  apply_appearance(appear_family_, appear_size_, appear_weight_, appear_palette_);
}

BodyView::~BodyView()
{
  img_conn_.disconnect();
  stop_images();
  live_.erase(std::remove(live_.begin(), live_.end(), this), live_.end());
}

void BodyView::ensure_tags()
{
  auto table = buf_->get_tag_table();
  auto mk = [&](const char* name, auto fn) {
    if (table->lookup(name))
      return;
    auto tag = Gtk::TextBuffer::Tag::create(name);
    fn(tag);
    table->add(tag);
  };
  mk("h1", [](auto t) {
    t->property_weight() = Pango::WEIGHT_BOLD;
    t->property_scale() = 1.35;
  });
  mk("h2", [](auto t) {
    t->property_weight() = Pango::WEIGHT_BOLD;
    t->property_scale() = 1.2;
  });
  mk("h3", [](auto t) {
    t->property_weight() = Pango::WEIGHT_BOLD;
    t->property_scale() = 1.1;
  });
  mk("em", [](auto t) { t->property_style() = Pango::STYLE_ITALIC; });
  mk("strong", [](auto t) { t->property_weight() = Pango::WEIGHT_BOLD; });
  mk("pre", [](auto t) { t->property_family() = "monospace"; });
  mk("blockquote", [](auto t) {
    t->property_left_margin() = 20;
    t->property_style() = Pango::STYLE_ITALIC;
  });
  mk("link", [this](auto t) {
    t->property_underline() = Pango::UNDERLINE_SINGLE;
    t->property_foreground() = link_color_;
  });
}

void BodyView::apply_all(const std::string& family, int size_pt, int weight, int palette)
{
  appear_family_ = family.empty() ? "Sans" : family;
  appear_size_ = size_pt;
  appear_weight_ = weight;
  appear_palette_ = palette;
  for (auto* v : live_)
    v->apply_appearance(appear_family_, appear_size_, appear_weight_, appear_palette_);
}

void BodyView::apply_appearance(const std::string& family, int size_pt, int weight, int palette)
{
  const char* bg = "#F7F5EF";
  const char* fg = "#1A1A1A";
  const char* link = "#0B3A96";
  const char* sel_bg = "#3D6AA8";
  const char* sel_fg = "#FFFFFF";
  if (palette == 0) {
    bg = "#FFFFFF";
    fg = "#000000";
  } else if (palette == 2) {
    bg = "#111111";
    fg = "#D8D8D8";
    link = "#8CB4E8";
    sel_bg = "#8CB4E8";
    sel_fg = "#111111";
  }
  link_color_ = link;

  Pango::FontDescription desc;
  desc.set_family(family.empty() ? "Sans" : family);
  desc.set_size(std::max(8, std::min(size_pt, 32)) * Pango::SCALE);
  desc.set_weight(static_cast<Pango::Weight>(weight));
  override_font(desc);

  std::string fam_css = "\"";
  for (char c : desc.get_family()) {
    if (c == '"' || c == '\\')
      fam_css += '\\';
    fam_css += c;
  }
  fam_css += "\"";

  char css[1024];
  std::snprintf(css, sizeof(css),
                ".dispatch-body, .dispatch-body text,\n"
                ".dispatch-article-body, .dispatch-article-body text {\n"
                "  background-color: %s;\n"
                "  color: %s;\n"
                "  font-family: %s;\n"
                "  font-size: %dpt;\n"
                "  font-weight: %d;\n"
                "}\n"
                ".dispatch-body text selection,\n"
                "textview.dispatch-body text selection,\n"
                ".dispatch-article-body text selection,\n"
                "textview.dispatch-article-body text selection {\n"
                "  background-color: %s;\n"
                "  color: %s;\n"
                "}\n",
                bg, fg, fam_css.c_str(), std::max(8, std::min(size_pt, 32)), weight, sel_bg,
                sel_fg);

  static Glib::RefPtr<Gtk::CssProvider> screen_css;
  if (!screen_css) {
    screen_css = Gtk::CssProvider::create();
    Gtk::StyleContext::add_provider_for_screen(Gdk::Screen::get_default(), screen_css,
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 50);
  }
  try {
    screen_css->load_from_data(css);
  } catch (const Glib::Error&) {
  }

  Gdk::RGBA bg_rgba;
  bg_rgba.set(bg);
  override_background_color(bg_rgba);
  Gdk::RGBA fg_rgba;
  fg_rgba.set(fg);
  override_color(fg_rgba);
  Gdk::RGBA sel_bg_rgba;
  sel_bg_rgba.set(sel_bg);
  override_background_color(sel_bg_rgba, Gtk::STATE_FLAG_SELECTED);
  Gdk::RGBA sel_fg_rgba;
  sel_fg_rgba.set(sel_fg);
  override_color(sel_fg_rgba, Gtk::STATE_FLAG_SELECTED);

  auto table = buf_->get_tag_table();
  if (auto t = table->lookup("link"))
    t->property_foreground() = link_color_;
  for (auto& kv : link_hrefs_)
    kv.first->property_foreground() = link_color_;
}

void BodyView::clear_body()
{
  buf_->set_text("");
  link_hrefs_.clear();
  tag_stack_.clear();
  img_slots_.clear();
  used_urls_.clear();
  extras_.clear();
}

void BodyView::stop_images()
{
  stop_images_ = true;
  if (cancel_)
    g_cancellable_cancel(cancel_);
  if (img_thread_.joinable())
    img_thread_.join();
  {
    std::lock_guard<std::mutex> lock(img_mu_);
    img_ready_.clear();
  }
  if (cancel_) {
    g_object_unref(cancel_);
    cancel_ = nullptr;
  }
}

void BodyView::load(const std::string& html, const std::string& base_url,
                    const std::vector<Enclosure>& extra)
{
  stop_images();
  clear_body();
  base_url_ = base_url;
  stop_images_ = false;
  cancel_ = g_cancellable_new();

  if (html.empty() && extra.empty())
    return;

  const std::string wrapped = "<div>" + html + "</div>";
  xmlDoc* doc =
      htmlReadMemory(wrapped.data(), static_cast<int>(wrapped.size()), "item.html", "UTF-8",
                     HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING |
                         HTML_PARSE_NONET | HTML_PARSE_NOBLANKS);
  if (doc) {
    xmlNode* root = xmlDocGetRootElement(doc);
    std::function<xmlNode*(xmlNode*)> find_body = [&](xmlNode* n) -> xmlNode* {
      for (; n; n = n->next) {
        if (n->type == XML_ELEMENT_NODE && xml_name(n) == "body")
          return n;
        if (xmlNode* hit = find_body(n->children))
          return hit;
      }
      return nullptr;
    };
    xmlNode* body = find_body(root);
    walk(body ? body->children : root, 0);
    xmlFreeDoc(doc);
  } else if (!html.empty()) {
    insert_text(html, {});
  }

  for (const auto& e : extra) {
    if (e.url.empty() || already_used(e.url))
      continue;
    const std::string url = resolve(e.url);
    if (looks_image(url, e.type))
      insert_image_slot(url, e.title);
    else if (looks_audio(url, e.type) || looks_video(url, e.type)) {
      const std::string kind = looks_video(url, e.type) ? "video" : "audio";
      insert_media_button(url, kind, media_mime(url, e.type, kind));
    } else
      insert_media_button(url, "media", media_mime(url, e.type, "media"));
  }

  start_images();
}

void BodyView::start_images()
{
  if (img_slots_.empty())
    return;
  const std::vector<ImgSlot> slots = img_slots_;
  GCancellable* cancel = cancel_;
  if (cancel)
    g_object_ref(cancel);
  img_thread_ = std::thread([this, slots, cancel]() {
    for (const auto& slot : slots) {
      if (stop_images_ || (cancel && g_cancellable_is_cancelled(cancel)))
        break;
      std::string err;
      const std::string bytes = http_get(slot.url, err, cancel);
      auto pix = err.empty() ? pixbuf_from_bytes(bytes) : Glib::RefPtr<Gdk::Pixbuf>{};
      {
        std::lock_guard<std::mutex> lock(img_mu_);
        img_ready_.push_back({slot.mark, pix});
      }
      img_done_.emit();
    }
    if (cancel)
      g_object_unref(cancel);
  });
}

void BodyView::on_img_done()
{
  std::vector<ImgReady> got;
  {
    std::lock_guard<std::mutex> lock(img_mu_);
    got.swap(img_ready_);
  }
  for (auto& r : got) {
    if (!r.mark || !r.pix)
      continue;
    Gtk::TextIter it = buf_->get_iter_at_mark(r.mark);
    buf_->insert_pixbuf(it, r.pix);
  }
}

bool BodyView::in_pre() const
{
  for (const auto& t : tag_stack_) {
    if (t == "pre")
      return true;
  }
  return false;
}

gunichar BodyView::last_char() const
{
  if (buf_->get_char_count() == 0)
    return 0;
  auto it = buf_->end();
  it.backward_char();
  return it.get_char();
}

void BodyView::ensure_break()
{
  const gunichar c = last_char();
  if (c == 0 || c == '\n')
    return;
  buf_->insert(buf_->end(), "\n");
}

void BodyView::ensure_paragraph()
{
  if (buf_->get_char_count() == 0)
    return;
  gunichar last = last_char();
  gunichar prev = 0;
  if (buf_->get_char_count() >= 2) {
    auto it = buf_->end();
    it.backward_char();
    it.backward_char();
    prev = it.get_char();
  }
  if (last != '\n')
    buf_->insert(buf_->end(), "\n\n");
  else if (prev != '\n')
    buf_->insert(buf_->end(), "\n");
}

void BodyView::pad_space()
{
  if (in_pre())
    return;
  const gunichar c = last_char();
  if (c == 0 || c == ' ' || c == '\n' || c == '\t')
    return;
  buf_->insert(buf_->end(), " ");
}

void BodyView::insert_text(const std::string& text, const std::vector<Glib::ustring>& tags)
{
  if (text.empty())
    return;
  std::string cooked = text;
  if (!in_pre()) {
    std::string tmp;
    tmp.reserve(cooked.size());
    bool space = false;
    for (unsigned char ch : cooked) {
      if (ch == '\r')
        continue;
      if (ch == ' ' || ch == '\t' || ch == '\n') {
        space = true;
        continue;
      }
      if (space) {
        if (!tmp.empty())
          tmp.push_back(' ');
        space = false;
      }
      tmp.push_back(static_cast<char>(ch));
    }
    cooked.swap(tmp);
    if (cooked.empty())
      return;
    const gunichar prev = last_char();
    if (!cooked.empty() && cooked[0] == ' ' && (prev == 0 || prev == ' ' || prev == '\n'))
      cooked.erase(cooked.begin());
    if (cooked.empty())
      return;
  }
  const int start_off = buf_->get_char_count();
  Glib::ustring u;
  if (g_utf8_validate(cooked.data(), static_cast<gssize>(cooked.size()), nullptr))
    u = cooked;
  else {
    gchar* v = g_utf8_make_valid(cooked.data(), static_cast<gssize>(cooked.size()));
    u = v ? v : "";
    g_free(v);
  }
  buf_->insert(buf_->end(), u);
  if (tags.empty())
    return;
  auto table = buf_->get_tag_table();
  auto start = buf_->get_iter_at_offset(start_off);
  auto end = buf_->end();
  for (const auto& n : tags) {
    if (auto t = table->lookup(n))
      buf_->apply_tag(t, start, end);
  }
}

bool BodyView::already_used(const std::string& url) const
{
  for (const auto& u : used_urls_) {
    if (u == url)
      return true;
  }
  return false;
}

void BodyView::insert_image_slot(const std::string& url, const std::string& /*alt*/)
{
  if (url.empty() || already_used(url))
    return;
  used_urls_.push_back(url);
  ensure_paragraph();
  auto mark = buf_->create_mark(buf_->end(), true);
  img_slots_.push_back({mark, url});
  /* Newlines must be inserted *after* the mark. ensure_paragraph() is a
   * no-op when the buffer already ends in \n\n from the previous block,
   * which glued the pixbuf to the following text. */
  buf_->insert(buf_->end(), "\n");
}

void BodyView::insert_media_button(const std::string& url, const std::string& kind,
                                   const std::string& mime)
{
  if (url.empty() || already_used(url))
    return;
  used_urls_.push_back(url);
  ensure_paragraph();
  std::string label = "Open media";
  if (kind == "audio")
    label = "Play audio";
  else if (kind == "video")
    label = "Play video";
  else if (kind == "embed")
    label = "Open embed";
  auto iter = buf_->end();
  auto anchor = buf_->create_child_anchor(iter);
  auto* btn = Gtk::manage(new Gtk::Button(label));
  btn->set_relief(Gtk::RELIEF_NORMAL);
  const std::string launch_mime = mime;
  btn->signal_clicked().connect([this, url, launch_mime]() { open_uri(url, launch_mime); });
  add_child_at_anchor(*btn, anchor);
  extras_.push_back(btn);
  btn->show();
  ensure_paragraph();
}

void BodyView::open_uri(const std::string& uri, const std::string& mime)
{
  if (uri.empty())
    return;
  if (!mime.empty()) {
    try {
      auto app = Gio::AppInfo::get_default_for_type(mime, true);
      if (!app)
        app = Gio::AppInfo::get_default_for_type(mime, false);
      if (app) {
        std::vector<Glib::ustring> uris;
        uris.emplace_back(uri);
        Glib::RefPtr<Gio::AppLaunchContext> ctx;
        if (auto display = get_display())
          ctx = display->get_app_launch_context();
        if (app->launch_uris(uris, ctx))
          return;
      }
    } catch (const Glib::Error&) {
    }
  }
  auto* top = get_toplevel();
  GError* err = nullptr;
  if (top && top->get_is_toplevel())
    gtk_show_uri_on_window(GTK_WINDOW(top->gobj()), uri.c_str(), GDK_CURRENT_TIME, &err);
  else {
    try {
      Gio::AppInfo::launch_default_for_uri(uri);
    } catch (const Glib::Error&) {
    }
  }
  if (err)
    g_error_free(err);
}

std::string collapse_slashes(const std::string& url)
{
  const auto scheme = url.find("://");
  size_t i = scheme == std::string::npos ? 0 : scheme + 3;
  std::string out = url.substr(0, i);
  bool slash = false;
  for (; i < url.size(); ++i) {
    if (url[i] == '/') {
      if (!slash)
        out.push_back('/');
      slash = true;
    } else {
      slash = false;
      out.push_back(url[i]);
    }
  }
  return out;
}

std::string BodyView::resolve(const std::string& src) const
{
  if (src.empty())
    return {};
  if (src.compare(0, 7, "http://") == 0 || src.compare(0, 8, "https://") == 0)
    return collapse_slashes(src);
  std::string out;
  if (src.compare(0, 2, "//") == 0) {
    if (base_url_.compare(0, 8, "https://") == 0)
      out = "https:" + src;
    else
      out = "http:" + src;
  } else if (base_url_.empty()) {
    out = src;
  } else {
    const auto scheme = base_url_.find("://");
    if (scheme == std::string::npos)
      out = src;
    else if (!src.empty() && src[0] == '/') {
      auto slash = base_url_.find('/', scheme + 3);
      const std::string origin =
          slash == std::string::npos ? base_url_ : base_url_.substr(0, slash);
      out = origin + src;
    } else {
      auto slash = base_url_.rfind('/');
      if (slash == std::string::npos || slash < scheme + 3)
        out = base_url_ + "/" + src;
      else
        out = base_url_.substr(0, slash + 1) + src;
    }
  }
  return collapse_slashes(out);
}

void BodyView::walk(xmlNode* node, int list_depth)
{
  for (xmlNode* n = node; n; n = n->next) {
    if (n->type == XML_TEXT_NODE || n->type == XML_CDATA_SECTION_NODE) {
      if (n->content)
        insert_text(reinterpret_cast<const char*>(n->content), tag_stack_);
      continue;
    }
    if (n->type != XML_ELEMENT_NODE)
      continue;
    const std::string name = xml_name(n);
    if (skip_tag(name))
      continue;

    if (name == "br") {
      buf_->insert(buf_->end(), "\n");
      continue;
    }
    if (name == "img") {
      const std::string url = resolve(xml_prop(n, "src"));
      insert_image_slot(url, xml_prop(n, "alt"));
      continue;
    }
    if (name == "iframe") {
      const std::string src = resolve(xml_prop(n, "src"));
      const std::string s = lower(src);
      std::string kind = "embed";
      if (s.find("youtube") != std::string::npos || s.find("rumble") != std::string::npos ||
          s.find("odysee") != std::string::npos || s.find("vimeo") != std::string::npos ||
          s.find("lbry") != std::string::npos)
        kind = "video";
      else if (s.find("anchor.fm") != std::string::npos ||
               s.find("soundcloud") != std::string::npos)
        kind = "audio";
      insert_media_button(src, kind);
      continue;
    }
    if (name == "audio" || name == "video") {
      std::string src = xml_prop(n, "src");
      std::string type = xml_prop(n, "type");
      if (src.empty()) {
        for (xmlNode* c = n->children; c; c = c->next) {
          if (c->type == XML_ELEMENT_NODE && xml_name(c) == "source") {
            src = xml_prop(c, "src");
            if (type.empty())
              type = xml_prop(c, "type");
            if (!src.empty())
              break;
          }
        }
      }
      const std::string resolved = resolve(src);
      insert_media_button(resolved, name, media_mime(resolved, type, name));
      continue;
    }
    if (name == "source") {
      const std::string src = resolve(xml_prop(n, "src"));
      const std::string type = xml_prop(n, "type");
      if (looks_image(src, type))
        insert_image_slot(src, {});
      else if (!src.empty()) {
        const std::string kind = looks_video(src, type) ? "video" : "audio";
        insert_media_button(src, kind, media_mime(src, type, kind));
      }
      continue;
    }

    if (name == "p" || name == "blockquote" || name == "h1" || name == "h2" || name == "h3" ||
        name == "h4" || name == "h5" || name == "h6" || name == "pre" || name == "figure" ||
        name == "ul" || name == "ol")
      ensure_paragraph();
    else if (name == "li" || name == "tr")
      ensure_break();

    std::string pushed;
    if (name == "h1" || name == "h2" || name == "h3")
      pushed = name;
    else if (name == "h4" || name == "h5" || name == "h6")
      pushed = "h3";
    else if (name == "em" || name == "i")
      pushed = "em";
    else if (name == "strong" || name == "b")
      pushed = "strong";
    else if (name == "pre")
      pushed = "pre";
    else if (name == "code")
      pushed = "pre";
    else if (name == "blockquote")
      pushed = "blockquote";

    std::string href;
    if (name == "a") {
      href = resolve(xml_prop(n, "href"));
      if (!href.empty())
        pushed = "link";
    }

    if (name == "li")
      insert_text(std::string(static_cast<size_t>(list_depth + 1) * 2, ' ') + "• ", tag_stack_);

    if (name == "a" && !href.empty())
      pad_space();

    if (!pushed.empty())
      tag_stack_.push_back(pushed);
    const int child_list = (name == "ul" || name == "ol") ? list_depth + 1 : list_depth;
    const int start_off = buf_->get_char_count();
    walk(n->children, child_list);
    if (name == "a" && !href.empty()) {
      auto start = buf_->get_iter_at_offset(start_off);
      auto end = buf_->end();
      auto unique = Gtk::TextBuffer::Tag::create();
      unique->property_underline() = Pango::UNDERLINE_SINGLE;
      unique->property_foreground() = link_color_;
      buf_->get_tag_table()->add(unique);
      buf_->apply_tag(unique, start, end);
      link_hrefs_[unique] = href;
      used_urls_.push_back(href);
      pad_space();
    }
    if (!pushed.empty() && !tag_stack_.empty() && tag_stack_.back() == pushed)
      tag_stack_.pop_back();

    if (name == "p" || name == "blockquote" || name == "h1" || name == "h2" || name == "h3" ||
        name == "h4" || name == "h5" || name == "h6" || name == "pre" || name == "figure" ||
        name == "ul" || name == "ol")
      ensure_paragraph();
    else if (name == "li" || name == "tr")
      ensure_break();
  }
}

bool BodyView::on_button_release_event(GdkEventButton* event)
{
  if (event->button != 1)
    return Gtk::TextView::on_button_release_event(event);
  int x = 0, y = 0;
  window_to_buffer_coords(Gtk::TEXT_WINDOW_TEXT, static_cast<int>(event->x),
                          static_cast<int>(event->y), x, y);
  Gtk::TextIter iter;
  get_iter_at_location(iter, x, y);
  for (const auto& tag : iter.get_tags()) {
    auto it = link_hrefs_.find(tag);
    if (it != link_hrefs_.end()) {
      open_uri(it->second);
      return true;
    }
  }
  return Gtk::TextView::on_button_release_event(event);
}

bool BodyView::on_motion_notify_event(GdkEventMotion* event)
{
  int x = 0, y = 0;
  window_to_buffer_coords(Gtk::TEXT_WINDOW_TEXT, static_cast<int>(event->x),
                          static_cast<int>(event->y), x, y);
  Gtk::TextIter iter;
  get_iter_at_location(iter, x, y);
  bool over_link = false;
  for (const auto& tag : iter.get_tags()) {
    if (link_hrefs_.count(tag)) {
      over_link = true;
      break;
    }
  }
  auto win = get_window(Gtk::TEXT_WINDOW_TEXT);
  if (win)
    win->set_cursor(Gdk::Cursor::create(get_display(), over_link ? Gdk::HAND2 : Gdk::XTERM));
  return Gtk::TextView::on_motion_notify_event(event);
}

}  // namespace dispatch
