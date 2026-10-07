#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace mo {
struct TrackInfo {
  std::wstring title;
  std::wstring artist;
  std::wstring album;
  std::wstring sourceAppId;
  std::vector<std::uint8_t> artwork;
  std::int64_t position100ns{};
  std::int64_t duration100ns{};
  bool playing{};
  std::uint64_t revision{};
};
}

