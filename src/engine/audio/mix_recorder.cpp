#include "mix_recorder.h"

#if defined(_WIN32)

#include <xapobase.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#pragma comment(lib, "xapobase.lib")

namespace gbtg
{
namespace
{
class __declspec(uuid("6B5D3C2E-8E1F-4C9A-9D61-3F2B7A1E5C40")) MixRecorder : public CXAPOBase
{
  public:
    explicit MixRecorder(std::FILE* file) : CXAPOBase(&kProperties), file_(file)
    {
    }
    ~MixRecorder() override
    {
        finish();
    }

    STDMETHOD(LockForProcess)(UINT32 input_count, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* inputs,
        UINT32 output_count, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* outputs) override
    {
        const HRESULT status = CXAPOBase::LockForProcess(input_count, inputs, output_count, outputs);
        if (SUCCEEDED(status))
        {
            channels_ = inputs[0].pFormat->nChannels;
            rate_ = inputs[0].pFormat->nSamplesPerSec;
            write_header();
        }
        return status;
    }

    STDMETHOD_(void, Process)(UINT32, const XAPO_PROCESS_BUFFER_PARAMETERS* inputs, UINT32,
        XAPO_PROCESS_BUFFER_PARAMETERS*, BOOL) override
    {
        if (file_ == nullptr)
        {
            return;
        }
        const size_t samples = size_t(inputs[0].ValidFrameCount) * channels_;
        if (inputs[0].BufferFlags == XAPO_BUFFER_VALID)
        {
            std::fwrite(inputs[0].pBuffer, sizeof(float), samples, file_);
        }
        else
        {
            silence_.assign(samples, 0.f);
            std::fwrite(silence_.data(), sizeof(float), samples, file_);
        }
        data_bytes_ += std::uint32_t(samples * sizeof(float));
        // Keep the header's sizes current about once a second, so a killed process leaves a playable file.
        if (data_bytes_ - header_at_ >= rate_ * channels_ * sizeof(float))
        {
            write_header();
        }
    }

  private:
    void write_header()
    {
        const long at = std::ftell(file_);
        std::fseek(file_, 0, SEEK_SET);
        const std::uint32_t bytes_per_second = rate_ * channels_ * 4;
        const std::uint16_t block = std::uint16_t(channels_ * 4);
        const std::uint32_t riff = 36 + data_bytes_;
        const std::uint32_t fmt_size = 16;
        const std::uint16_t format = 3; // WAVE_FORMAT_IEEE_FLOAT
        const std::uint16_t channels = std::uint16_t(channels_);
        const std::uint16_t bits = 32;
        std::fwrite("RIFF", 1, 4, file_);
        std::fwrite(&riff, 4, 1, file_);
        std::fwrite("WAVEfmt ", 1, 8, file_);
        std::fwrite(&fmt_size, 4, 1, file_);
        std::fwrite(&format, 2, 1, file_);
        std::fwrite(&channels, 2, 1, file_);
        std::fwrite(&rate_, 4, 1, file_);
        std::fwrite(&bytes_per_second, 4, 1, file_);
        std::fwrite(&block, 2, 1, file_);
        std::fwrite(&bits, 2, 1, file_);
        std::fwrite("data", 1, 4, file_);
        std::fwrite(&data_bytes_, 4, 1, file_);
        std::fflush(file_);
        if (at > 44)
        {
            std::fseek(file_, at, SEEK_SET);
        }
        header_at_ = data_bytes_;
    }

    void finish()
    {
        if (file_ != nullptr)
        {
            write_header();
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    static XAPO_REGISTRATION_PROPERTIES kProperties;
    std::FILE* file_ = nullptr;
    std::uint32_t channels_ = 2;
    std::uint32_t rate_ = 48000;
    std::uint32_t data_bytes_ = 0;
    std::uint32_t header_at_ = 0;
    std::vector<float> silence_;
};

XAPO_REGISTRATION_PROPERTIES MixRecorder::kProperties = {__uuidof(MixRecorder), L"GbtG mix recorder", L"", 1, 0,
    XAPO_FLAG_INPLACE_REQUIRED | XAPO_FLAG_CHANNELS_MUST_MATCH | XAPO_FLAG_FRAMERATE_MUST_MATCH
        | XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH | XAPO_FLAG_BUFFERCOUNT_MUST_MATCH | XAPO_FLAG_INPLACE_SUPPORTED,
    1, 1, 1, 1};
}

IUnknown* create_mix_recorder(const char* path)
{
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr)
    {
        return nullptr;
    }
    return static_cast<IXAPO*>(new MixRecorder(file));
}
}

#endif
