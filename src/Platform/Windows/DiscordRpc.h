#pragma once

#include <MusicOverlay/Core/TrackInfo.h>
#include <cstdint>
#include <string>

namespace mo::win {

struct DiscordRpcSettings {
  bool enabled{};
  std::wstring applicationId{L"1061319148602392616"};
  std::wstring defaultIconKey{L"logo"};
  std::wstring artworkKey;
};

class DiscordRpc {
public:
  ~DiscordRpc();
  void update(const TrackInfo& track, const DiscordRpcSettings& settings);
  void disconnect();

private:
  bool connect(const std::wstring& applicationId);
  bool sendFrame(std::uint32_t opcode, const std::string& json);
  void drainResponses();

  void* pipe_{};
  std::wstring connectedApplicationId_;
  std::wstring lastSignature_;
  std::uint64_t lastRevision_{~std::uint64_t{}};
  std::uint64_t lastSentTick_{};
  std::uint64_t lastAttemptTick_{};
  std::uint64_t nonce_{};
};

}
