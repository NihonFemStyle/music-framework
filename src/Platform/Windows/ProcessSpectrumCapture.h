#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
namespace mo::win
{
class ProcessSpectrumCapture
{
  public:
  static constexpr std::size_t BandCount=64;
  ProcessSpectrumCapture()=default;
  ~ProcessSpectrumCapture(){stop();}
  ProcessSpectrumCapture(const ProcessSpectrumCapture&)=delete;
  ProcessSpectrumCapture& operator=(const ProcessSpectrumCapture&)=delete;
  bool startForApp(const std::wstring& appId);
  bool ensureForApp(const std::wstring& appId);
  void setResponse(std::uint32_t response) noexcept{response_=response>2?2:response;}
  void stop();
  [[nodiscard]] std::array<float,BandCount> snapshot() const;
  [[nodiscard]] const std::wstring& activeAppId() const noexcept{return appId_;}
  private:
  bool startForProcess(const std::wstring& appId,std::uint32_t processId);
  void captureLoop();
  std::wstring appId_;
  std::uint32_t processId_{};
  std::atomic_bool stopping_{};
  std::atomic_bool captureFailed_{};
  std::atomic_uint32_t response_{1};
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
