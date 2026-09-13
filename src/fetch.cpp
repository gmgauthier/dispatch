/* SPDX-License-Identifier: Unlicense */

#include "fetch.hpp"
#include "config.hpp"

#include <libsoup/soup.h>

#include <glib.h>

namespace dispatch {
namespace {

const int kTimeoutSec = 20;
const gsize kMaxBytes = 8u * 1024u * 1024u;

}  // namespace

std::string http_get(const std::string& url, std::string& error)
{
  error.clear();
  if (url.compare(0, 7, "http://") != 0 && url.compare(0, 8, "https://") != 0) {
    error = "URL must start with http:// or https://";
    return {};
  }

  SoupSession* session = soup_session_new();
  g_object_set(session, "timeout", kTimeoutSec, "user-agent",
               "Dispatch/" VERSION " (https://github.com/gmgauthier/dispatch)", nullptr);

  SoupMessage* msg = soup_message_new("GET", url.c_str());
  if (!msg) {
    error = "Invalid URL";
    g_object_unref(session);
    return {};
  }

  GError* gerr = nullptr;
  GBytes* bytes = soup_session_send_and_read(session, msg, nullptr, &gerr);
  const guint status = soup_message_get_status(msg);
  g_object_unref(msg);
  g_object_unref(session);

  if (gerr) {
    error = gerr->message ? gerr->message : "Fetch failed";
    g_error_free(gerr);
    if (bytes)
      g_bytes_unref(bytes);
    return {};
  }
  if (status < 200 || status >= 300) {
    error = "HTTP " + std::to_string(status);
    if (bytes)
      g_bytes_unref(bytes);
    return {};
  }
  if (!bytes) {
    error = "Empty response";
    return {};
  }

  gsize len = 0;
  const char* data = static_cast<const char*>(g_bytes_get_data(bytes, &len));
  if (len > kMaxBytes) {
    error = "Feed larger than 8 MiB";
    g_bytes_unref(bytes);
    return {};
  }
  std::string out(data ? data : "", len);
  g_bytes_unref(bytes);
  return out;
}

}  // namespace dispatch
