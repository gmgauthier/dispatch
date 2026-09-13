/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>
#include <vector>

namespace dispatch {

struct OpmlOutline {
  std::string url;
  std::string title;
};

std::vector<OpmlOutline> parse_opml_file(const std::string& path, std::string& error);
bool write_opml_file(const std::string& path, const std::vector<OpmlOutline>& outlines,
                     std::string& error);

}  // namespace dispatch
