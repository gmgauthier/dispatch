/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>
#include <vector>

namespace dispatch {

struct EphemerisContact {
  std::string name;
  std::string email;
};

/* Read-only. Uses ~/.config/ephemeris/ephemeris.ini session.path. */
std::vector<EphemerisContact> load_ephemeris_emails();

}  // namespace dispatch
