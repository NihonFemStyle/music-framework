#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

namespace mo::win {
class ProcessSpectrumCapture {
public:
  static constexpr std::size_t BandCount=32;
  ProcessSpectrumCapture()=default;
  ~ProcessSpectrumCapture(){stop();}
  ProcessSpectrumCapture(const ProcessSpectrumCapture&)=delete;
  ProcessSpectrumCapture& operator=(const ProcessSpectrumCapture&)=delete;
  bool startForApp(const std::wstring& appId);
  void stop();
  [[nodiscard]] std::array<float,BandCount> snapshot() const;
  [[nodiscard]] const std::wstring& activeAppId() const noexcept{return appId_;}
private:
  void captureLoop();
  std::wstring appId_;
  std::atomic_bool stopping_{};
  std::array<std::atomic<float>,BandCount> bands_{};
  void* audioClient_{};
  void* captureClient_{};
  void* sampleEvent_{};
  std::thread thread_;
  std::uint32_t sampleRate_{};
  std::uint16_t channels_{};
  bool floatSamples_{};
};
}
