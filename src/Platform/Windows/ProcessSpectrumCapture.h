#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
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
  static unsigned long __stdcall threadEntry(void* context);
  std::wstring appId_;
  std::uint32_t processId_{};
  std::atomic_bool stopping_{};
  std::atomic_bool captureFailed_{};
  std::atomic_uint32_t response_{1};
  std::array<std::atomic<float>,BandCount> bands_{};
  void* audioClient_{};
  void* captureClient_{};
  void* sampleEvent_{};
  void* thread_{}; // Win32 thread handle (no std::thread, so the no-CRT build can share this code)
  std::uint32_t sampleRate_{};
  std::uint16_t channels_{};
  bool floatSamples_{};
  };
  }
