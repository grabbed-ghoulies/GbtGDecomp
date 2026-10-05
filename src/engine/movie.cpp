#include "gbtg/movie.hpp"

#include "gbtg/bundle.hpp"
#include "gbtg/engine_internal.hpp"
#include "gbtg/hash.hpp"
#include "gbtg/resource.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#if defined(GBTG_HAS_FFMPEG)
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#endif

namespace gbtg
{
namespace
{

// 0x001A6010 open flags. 0x00100000 is the alpha open. 0x04000000 is added after a
// successful open and selects the memory pointer on the later open.
constexpr unsigned kOpenAlpha = 0x00100000u;
constexpr unsigned kOpenFromMemory = 0x04000000u;
constexpr unsigned kSoundVolume = 0x4000u;

struct MovieClip
{
    // BINK fields 0x00107420 and 0x00107360 read: Height +4, Frames +8, FrameNum +0xC.
    u32 width = 0;
    u32 height = 0;
    u32 frames = 1;
    u32 frame_num = 0;
    RgbaImage image;
    double frame_dur = 1.0 / 30.0;
    double next_time = 0.0;
    bool eof = false;
    bool audio_done = false;
    bool loop = false;
    std::string cue;
#if defined(GBTG_HAS_FFMPEG)
    AVFormatContext* fmt = nullptr;
    AVCodecContext* video = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    SwsContext* scale = nullptr;
    AVIOContext* io = nullptr;
    u8* io_buf = nullptr;
    int video_index = -1;
    struct Mem
    {
        const u8* data = nullptr;
        size_t size = 0;
        size_t pos = 0;
    } mem;
#endif
};

// 0x3C node from 0x001071A0. Field order matches the offsets the tick reads.
struct MovieNode
{
    MovieNode* prev = nullptr; // +0
    MovieNode* next = nullptr; // +4
    MovieClip* clip = nullptr; // +8
    unsigned open_flags = kOpenAlpha; // +0xC
    unsigned flags = 0;               // +0x10
    int loop = 0;                     // +0x14
    int play = 0;                     // +0x18
    int decoded = 0;                  // +0x1C
    int pending = 0;                  // +0x20
    int first_copy = 1;               // +0x24
    int failed = 0;                   // +0x28
    int file = 0;                     // +0x2C
    int paced = 0;                    // cutscene event drives the displayed frame
    int show_frame = 0;
    int enabled = 0;                  // +0x34, stored after the buffer pointer below
    int logo = 0;                     // +0x38
    std::vector<u8> bytes;            // +0x30 buffer. Owned here; the dump keeps a heap block.
    std::string name;
    std::string path;
};

// One cutscene-event movie. 0x0004C6C0 / 0x0004CC28 opens it at `start`
// and 0x00106DD0 starts Bink playback at `end` (frontend movie mode is 2).
struct MovieCue
{
    float start = 0.f;
    float end = 0.f;
    int slot = 0;
    std::string name;
    bool started = false;
};

// DAT_0050E0B8. DAT_0050E0BC is g_ready. DAT_0050E0B0 is the logo preload, left clear.
MovieNode* g_movies = nullptr;
std::string g_cue_clip;
float g_cue_time = 0.f;
std::vector<MovieCue> g_cues;
bool g_ready = false;
std::filesystem::path g_root;
double g_now = 0.0;

#if defined(GBTG_HAS_FFMPEG)

int mem_read(void* opaque, u8* buf, int buf_size)
{
    auto* mem = static_cast<MovieClip::Mem*>(opaque);
    if (mem->pos >= mem->size)
    {
        return AVERROR_EOF;
    }
    const size_t n = std::min(size_t(buf_size), mem->size - mem->pos);
    std::memcpy(buf, mem->data + mem->pos, n);
    mem->pos += n;
    return int(n);
}

int64_t mem_seek(void* opaque, int64_t offset, int whence)
{
    auto* mem = static_cast<MovieClip::Mem*>(opaque);
    if (whence == AVSEEK_SIZE)
    {
        return int64_t(mem->size);
    }
    int64_t pos = 0;
    if (whence == SEEK_SET)
    {
        pos = offset;
    }
    else if (whence == SEEK_CUR)
    {
        pos = int64_t(mem->pos) + offset;
    }
    else if (whence == SEEK_END)
    {
        pos = int64_t(mem->size) + offset;
    }
    else
    {
        return AVERROR(EINVAL);
    }
    if (pos < 0 || size_t(pos) > mem->size)
    {
        return AVERROR(EINVAL);
    }
    mem->pos = size_t(pos);
    return pos;
}

void clip_close(MovieClip* clip)
{
    if (clip == nullptr)
    {
        return;
    }
    if (!clip->cue.empty())
    {
        pcm_stop_cue(clip->cue.c_str());
    }
    sws_freeContext(clip->scale);
    av_frame_free(&clip->frame);
    av_packet_free(&clip->packet);
    avcodec_free_context(&clip->video);
    if (clip->fmt != nullptr)
    {
        clip->fmt->pb = nullptr;
        avformat_close_input(&clip->fmt);
    }
    if (clip->io != nullptr)
    {
        avio_context_free(&clip->io);
    }
    av_freep(&clip->io_buf);
    delete clip;
}

bool clip_seek_start(MovieClip* clip)
{
    if (av_seek_frame(clip->fmt, clip->video_index, 0, AVSEEK_FLAG_BACKWARD) < 0 &&
        av_seek_frame(clip->fmt, clip->video_index, 0, AVSEEK_FLAG_BYTE) < 0)
    {
        return false;
    }
    avcodec_flush_buffers(clip->video);
    clip->eof = false;
    return true;
}

void clip_audio(MovieClip* clip, bool loop)
{
    const int index = av_find_best_stream(clip->fmt, AVMEDIA_TYPE_AUDIO, -1, clip->video_index, nullptr, 0);
    if (index < 0)
    {
        return;
    }
    AVStream* stream = clip->fmt->streams[index];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr)
    {
        return;
    }
    AVCodecContext* audio = avcodec_alloc_context3(codec);
    if (audio == nullptr || avcodec_parameters_to_context(audio, stream->codecpar) < 0 || avcodec_open2(audio, codec, nullptr) < 0)
    {
        avcodec_free_context(&audio);
        return;
    }
    SwrContext* swr = nullptr;
    std::vector<u8> pcm;
    int rate = audio->sample_rate > 0 ? audio->sample_rate : 44100;
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    while (packet != nullptr && frame != nullptr && av_read_frame(clip->fmt, packet) >= 0)
    {
        if (packet->stream_index != index)
        {
            av_packet_unref(packet);
            continue;
        }
        if (avcodec_send_packet(audio, packet) < 0)
        {
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);
        while (avcodec_receive_frame(audio, frame) >= 0)
        {
            if (swr == nullptr)
            {
                AVChannelLayout out_layout;
                av_channel_layout_default(&out_layout, 2);
                AVChannelLayout in_layout = frame->ch_layout;
                if (in_layout.nb_channels <= 0)
                {
                    av_channel_layout_default(&in_layout, 1);
                }
                if (swr_alloc_set_opts2(&swr, &out_layout, AV_SAMPLE_FMT_S16, rate, &in_layout, AVSampleFormat(frame->format),
                                        frame->sample_rate > 0 ? frame->sample_rate : rate, 0, nullptr) < 0 ||
                    swr_init(swr) < 0)
                {
                    swr_free(&swr);
                    av_channel_layout_uninit(&out_layout);
                    av_frame_unref(frame);
                    break;
                }
                av_channel_layout_uninit(&out_layout);
            }
            const int out_samples = swr_get_out_samples(swr, frame->nb_samples);
            if (out_samples <= 0)
            {
                av_frame_unref(frame);
                continue;
            }
            const size_t at = pcm.size();
            pcm.resize(at + size_t(out_samples) * 4);
            u8* dst = pcm.data() + at;
            const int got = swr_convert(swr, &dst, out_samples, frame->extended_data, frame->nb_samples);
            if (got > 0)
            {
                pcm.resize(at + size_t(got) * 4);
            }
            else
            {
                pcm.resize(at);
            }
            av_frame_unref(frame);
        }
    }
    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swr);
    avcodec_free_context(&audio);
    clip_seek_start(clip);
    if (!pcm.empty())
    {
        const float volume = float(kSoundVolume) / 32768.f;
        pcm_play_pcm(clip->cue.c_str(), pcm, rate, 2, loop, volume);
    }
}

bool clip_decode(MovieClip* clip)
{
    while (av_read_frame(clip->fmt, clip->packet) >= 0)
    {
        if (clip->packet->stream_index != clip->video_index)
        {
            av_packet_unref(clip->packet);
            continue;
        }
        const int sent = avcodec_send_packet(clip->video, clip->packet);
        av_packet_unref(clip->packet);
        if (sent < 0)
        {
            continue;
        }
        const int got = avcodec_receive_frame(clip->video, clip->frame);
        if (got < 0)
        {
            continue;
        }
        const int w = clip->frame->width;
        const int h = clip->frame->height;
        if (w <= 0 || h <= 0)
        {
            av_frame_unref(clip->frame);
            return false;
        }
        clip->scale = sws_getCachedContext(clip->scale, w, h, AVPixelFormat(clip->frame->format), w, h, AV_PIX_FMT_BGRA,
                                           SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (clip->scale == nullptr)
        {
            av_frame_unref(clip->frame);
            return false;
        }
        clip->image.width = w;
        clip->image.height = h;
        clip->image.bgra.resize(size_t(w) * size_t(h) * 4);
        u8* dst = clip->image.bgra.data();
        const int pitch = w * 4;
        sws_scale(clip->scale, clip->frame->data, clip->frame->linesize, 0, h, &dst, &pitch);
        // yuv420p has no plane. swscale leaves alpha at 0, and an alpha-tested page discards the picture.
        const int fmt = clip->frame->format;
        const bool has_alpha = fmt == AV_PIX_FMT_YUVA420P || fmt == AV_PIX_FMT_YUVA422P || fmt == AV_PIX_FMT_YUVA444P;
        if (!has_alpha)
        {
            for (size_t i = 3; i < clip->image.bgra.size(); i += 4)
            {
                clip->image.bgra[i] = 255;
            }
        }
        clip->width = u32(w);
        clip->height = u32(h);
        clip->image.serial += 1;
        av_frame_unref(clip->frame);
        return true;
    }
    clip->eof = true;
    return false;
}

MovieClip* clip_open(const void* src, unsigned flags, bool loop, const std::string& cue, const std::vector<u8>* bytes)
{
    if (src == nullptr && bytes == nullptr)
    {
        return nullptr;
    }
    av_log_set_level(AV_LOG_ERROR);
    const AVInputFormat* bink = av_find_input_format("bink");
    AVFormatContext* fmt = nullptr;
    MovieClip* clip = new MovieClip();
    clip->cue = cue;
    if ((flags & kOpenFromMemory) != 0)
    {
        if (bytes == nullptr || bytes->size() < 4)
        {
            delete clip;
            return nullptr;
        }
        clip->mem.data = bytes->data();
        clip->mem.size = bytes->size();
        clip->io_buf = static_cast<u8*>(av_malloc(4096));
        clip->io = avio_alloc_context(clip->io_buf, 4096, 0, &clip->mem, mem_read, nullptr, mem_seek);
        if (clip->io == nullptr)
        {
            clip_close(clip);
            return nullptr;
        }
        fmt = avformat_alloc_context();
        fmt->pb = clip->io;
        fmt->flags |= AVFMT_FLAG_CUSTOM_IO;
        if (avformat_open_input(&fmt, nullptr, bink, nullptr) < 0)
        {
            clip->fmt = fmt;
            clip_close(clip);
            return nullptr;
        }
    }
    else
    {
        const char* path = static_cast<const char*>(src);
        if (avformat_open_input(&fmt, path, bink, nullptr) < 0)
        {
            delete clip;
            return nullptr;
        }
    }
    clip->fmt = fmt;
    if (avformat_find_stream_info(fmt, nullptr) < 0)
    {
        clip_close(clip);
        return nullptr;
    }
    const int index = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (index < 0)
    {
        clip_close(clip);
        return nullptr;
    }
    AVStream* stream = fmt->streams[index];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr)
    {
        clip_close(clip);
        return nullptr;
    }
    clip->video = avcodec_alloc_context3(codec);
    if (clip->video == nullptr || avcodec_parameters_to_context(clip->video, stream->codecpar) < 0 ||
        avcodec_open2(clip->video, codec, nullptr) < 0)
    {
        clip_close(clip);
        return nullptr;
    }
    clip->video_index = index;
    clip->packet = av_packet_alloc();
    clip->frame = av_frame_alloc();
    clip->width = u32(stream->codecpar->width);
    clip->height = u32(stream->codecpar->height);
    if (stream->nb_frames > 0)
    {
        clip->frames = u32(stream->nb_frames);
    }
    const double fps = av_q2d(stream->avg_frame_rate);
    if (fps > 1.0 && fps < 120.0)
    {
        clip->frame_dur = 1.0 / fps;
    }
    if (clip->frames <= 1 && fmt->duration > 0)
    {
        const double seconds = double(fmt->duration) / double(AV_TIME_BASE);
        const u32 count = u32(seconds / clip->frame_dur + 0.5);
        if (count > 1)
        {
            clip->frames = count;
        }
    }
    clip->loop = loop;
    std::cout << "movie " << cue << " " << clip->width << "x" << clip->height << " frames " << clip->frames << "\n";
    return clip;
}

// Xbox 0x001A7870. Zero means a frame is due.
int bink_wait(MovieClip* clip)
{
    if (g_now + 1.0e-4 < clip->next_time)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x001A7D40.
void bink_do_frame(MovieClip* clip)
{
    // 0x001A7B70 arms track 0. Audio starts with the first displayed frame so a clip
    // that is open but not playing stays quiet.
    if (!clip->audio_done)
    {
        clip->audio_done = true;
        clip_audio(clip, clip->loop);
    }
    if (!clip_decode(clip) && clip->eof && clip->loop)
    {
        clip_seek_start(clip);
        clip_decode(clip);
    }
    clip->next_time = g_now + clip->frame_dur;
}

// Xbox 0x001A8350.
void bink_next_frame(MovieClip* clip)
{
    if (clip->frame_num + 1 >= clip->frames)
    {
        if (clip->loop)
        {
            clip_seek_start(clip);
            clip->frame_num = 0;
        }
        return;
    }
    clip->frame_num += 1;
}

// Cutscene events name a clock. Show that frame instead of running ahead of the page.
void pace_to_frame(MovieClip* clip, int want)
{
    if (!clip->audio_done)
    {
        clip->audio_done = true;
        clip_audio(clip, false);
    }
    const int last = clip->frames > 0 ? int(clip->frames) - 1 : 0;
    if (want < 0)
    {
        want = 0;
    }
    if (want > last)
    {
        want = last;
    }
    // A later event for the same clip starts again at its own end. 0x00106DD0
    // opens a new node; this seek is that restart on the one node we keep.
    if (want < int(clip->frame_num))
    {
        if (!clip_seek_start(clip))
        {
            return;
        }
        clip->frame_num = 0;
        clip->image.bgra.clear();
    }
    if (clip->image.bgra.empty())
    {
        if (!clip_decode(clip))
        {
            return;
        }
        clip->frame_num = 0;
    }
    while (int(clip->frame_num) < want)
    {
        if (!clip_decode(clip))
        {
            break;
        }
        clip->frame_num += 1;
    }
}

#else

void clip_close(MovieClip* clip)
{
    delete clip;
}

MovieClip* clip_open(const void* src, unsigned flags, bool loop, const std::string& cue, const std::vector<u8>* bytes)
{
    (void)src;
    (void)flags;
    (void)loop;
    (void)cue;
    (void)bytes;
    return nullptr;
}

int bink_wait(MovieClip* clip)
{
    (void)clip;
    return 1;
}

void bink_do_frame(MovieClip* clip)
{
    (void)clip;
}

void bink_next_frame(MovieClip* clip)
{
    (void)clip;
}

void pace_to_frame(MovieClip* clip, int want)
{
    (void)clip;
    (void)want;
}

#endif

bool load_file(const std::filesystem::path& path, std::vector<u8>& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff len = in.tellg();
    if (len <= 0)
    {
        return false;
    }
    in.seekg(0);
    out.resize(size_t(len));
    in.read(reinterpret_cast<char*>(out.data()), len);
    return bool(in);
}

// Xbox 0x000358B0, skipping the "aid_" the hash function starts past. aid_hash is that fold.
// Xbox 0x00106EF0. "%s%s%08x" with the disc root, "movie\\", and the hash.
// DAT_005096E0 / DAT_005096E4 would try a "pal" suffix first. Both stay clear here.
std::string movie_path(const char* name)
{
    if (name == nullptr || name[0] == 0 || g_root.empty())
    {
        return {};
    }
    return (g_root / "movie" / aid_hash_hex(aid_hash(name))).string();
}

MovieNode* find_name(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    for (MovieNode* node = g_movies; node != nullptr; node = node->next)
    {
        if (node->name == name)
        {
            return node;
        }
    }
    return nullptr;
}

void unlink_node(MovieNode* node)
{
    if (node->prev != nullptr)
    {
        node->prev->next = node->next;
    }
    else if (g_movies == node)
    {
        g_movies = node->next;
    }
    if (node->next != nullptr)
    {
        node->next->prev = node->prev;
    }
    node->prev = nullptr;
    node->next = nullptr;
}

void free_node(MovieNode* node)
{
    if (node == nullptr)
    {
        return;
    }
    // 0x001072B0. 0x001A84F0 closes the clip. The heap free around it is 0x000663E0.
    clip_close(node->clip);
    node->clip = nullptr;
    unlink_node(node);
    delete node;
}

// Xbox 0x00107030. ECX is the path, EDX is the node. Returns 0 on success.
int movie_open(MovieNode* node)
{
    // 0x000357D0 against aid_movie_ghoulies_attractmode_logoscaleup. DAT_0050E0B0 is the
    // preload pointer; it stays null, so a matching name still takes the file path.
    node->logo = node->name == "aid_movie_ghoulies_attractmode_logoscaleup" ? 1 : 0;
    if ((node->flags & 0x10) == 0)
    {
        // 0x001A6010(path, flags). 0x001A7B70(clip, 0, 0x4000) on success.
        node->clip = clip_open(node->path.c_str(), node->open_flags, node->loop != 0, node->name, nullptr);
        if (node->clip == nullptr)
        {
            // 0x000E96A0 / 0x00032700 report 0x6E. Those calls are not translated.
            std::cout << "movie missing " << node->name << "\n";
            return 0x6e;
        }
        node->file = 0;
        node->enabled = 1;
        return 0;
    }
    // 0x000EAA90, 0x000EAED0, 0x000EB310, 0x000EB0E0 and the callback at 0x00106FE0 read the
    // file and then set +0x34. Those functions are not translated. This read is that file IO.
    // The callback's store of 1 at +0x34 is the success state the tick tests.
    if (!load_file(node->path, node->bytes))
    {
        std::cout << "movie missing " << node->name << "\n";
        return 2;
    }
    node->enabled = 1;
    return 0;
}

// Xbox 0x00107420. ESI is the node.
void movie_tick(MovieNode* node)
{
    if (node->enabled == 0 || node->failed != 0)
    {
        return;
    }
    if (node->clip == nullptr)
    {
        // 0x001A6010(buffer, flags). flags already include 0x04000000.
        node->clip = clip_open(node->bytes.data(), node->open_flags, node->loop != 0, node->name, &node->bytes);
        if (node->clip == nullptr)
        {
            node->failed = 1;
            return;
        }
    }
    if (node->paced != 0)
    {
        pace_to_frame(node->clip, node->show_frame);
        node->decoded = 1;
        node->pending = 1;
        return;
    }
    if (bink_wait(node->clip) != 0)
    {
        return;
    }
    if (node->play <= 0)
    {
        return;
    }
    bink_do_frame(node->clip);
    node->decoded = 1;
    if (node->loop != 0 || node->clip->frame_num < node->clip->frames)
    {
        node->pending += 1;
        bink_next_frame(node->clip);
    }
}

} // namespace

void free_named(const std::string& name)
{
    MovieNode* node = find_name(name.c_str());
    if (node != nullptr)
    {
        free_node(node);
    }
}

void paint_movie(RgbaImage& dst, const RgbaImage& src)
{
    if (dst.width <= 0 || dst.height <= 0 || dst.bgra.empty() || src.bgra.empty())
    {
        return;
    }
    if (dst.movie_base.empty())
    {
        dst.movie_base = dst.bgra;
    }
    // BinkCopyToBuffer writes the frame, including its alpha, over the page.
    // A logo plane is transparent around the title, so the page art underneath stays.
    const bool same = dst.width == src.width && dst.height == src.height && dst.bgra.size() == src.bgra.size() &&
                      dst.movie_base.size() == dst.bgra.size();
    if (same)
    {
        const u8* s = src.bgra.data();
        const u8* base = dst.movie_base.data();
        u8* d = dst.bgra.data();
        for (size_t i = 0; i < dst.bgra.size(); i += 4)
        {
            const u32 a = s[i + 3];
            if (a >= 250)
            {
                std::memcpy(d + i, s + i, 4);
            }
            else if (a == 0)
            {
                std::memcpy(d + i, base + i, 4);
            }
            else
            {
                const u32 inv = 255 - a;
                d[i] = u8((s[i] * a + base[i] * inv) / 255);
                d[i + 1] = u8((s[i + 1] * a + base[i + 1] * inv) / 255);
                d[i + 2] = u8((s[i + 2] * a + base[i + 2] * inv) / 255);
                d[i + 3] = 255;
            }
        }
    }
    else
    {
        for (int y = 0; y < dst.height; ++y)
        {
            const int sy = src.height > 0 ? y * src.height / dst.height : 0;
            u8* row = dst.bgra.data() + size_t(y) * size_t(dst.width) * 4;
            const u8* srow = src.bgra.data() + size_t(sy) * size_t(src.width) * 4;
            const u8* brow = dst.movie_base.data() + size_t(y) * size_t(dst.width) * 4;
            for (int x = 0; x < dst.width; ++x)
            {
                const int sx = src.width > 0 ? x * src.width / dst.width : 0;
                const u8* sp = srow + size_t(sx) * 4;
                u8* dp = row + size_t(x) * 4;
                const u8* bp = brow + size_t(x) * 4;
                const u32 a = sp[3];
                if (a >= 250)
                {
                    std::memcpy(dp, sp, 4);
                }
                else if (a == 0)
                {
                    std::memcpy(dp, bp, 4);
                }
                else
                {
                    const u32 inv = 255 - a;
                    dp[0] = u8((sp[0] * a + bp[0] * inv) / 255);
                    dp[1] = u8((sp[1] * a + bp[1] * inv) / 255);
                    dp[2] = u8((sp[2] * a + bp[2] * inv) / 255);
                    dp[3] = 255;
                }
            }
        }
    }
    dst.serial += 1;
}

void restore_movie(RgbaImage& dst)
{
    if (dst.movie_base.empty())
    {
        return;
    }
    if (dst.bgra.size() == dst.movie_base.size())
    {
        std::memcpy(dst.bgra.data(), dst.movie_base.data(), dst.bgra.size());
    }
    dst.movie_base.clear();
    dst.movie_seen = 0;
    dst.serial += 1;
}

u32 rd_u32(std::span<const u8> b, size_t off)
{
    u32 v = 0;
    if (off + 4 <= b.size())
    {
        std::memcpy(&v, b.data() + off, 4);
    }
    return v;
}

float rd_f32(std::span<const u8> b, size_t off)
{
    float v = 0.f;
    if (off + 4 <= b.size())
    {
        std::memcpy(&v, b.data() + off, 4);
    }
    return v;
}

std::string rd_str(std::span<const u8> b, size_t off, size_t room)
{
    if (off >= b.size())
    {
        return {};
    }
    room = std::min(room, b.size() - off);
    size_t n = 0;
    while (n < room && b[off + n] != 0)
    {
        ++n;
    }
    return std::string(reinterpret_cast<const char*>(b.data() + off), n);
}

void add_cue(std::span<const u8> rec, int slot_off, size_t name_off)
{
    if (rec.size() < size_t(slot_off) + 4 || name_off >= rec.size())
    {
        return;
    }
    MovieCue cue;
    cue.start = rd_f32(rec, 4);
    cue.end = rd_f32(rec, 8);
    cue.slot = int(rd_u32(rec, size_t(slot_off)) & 0xff);
    cue.name = rd_str(rec, name_off, 120);
    if (cue.slot == 0 || cue.name.rfind("aid_movie_", 0) != 0 || !(cue.end > cue.start))
    {
        return;
    }
    g_cues.push_back(std::move(cue));
}

void collect_cues(const Game& g, const Cutscene& clip)
{
    g_cues.clear();
    g_cue_clip = clip.name;
    if (g.scene_bundle == nullptr)
    {
        return;
    }
    std::string prefix = clip.name;
    const std::string from = "aid_cutscene_";
    if (prefix.rfind(from, 0) == 0)
    {
        prefix.replace(0, from.size(), "aid_cutsceneevents_");
    }
    for (const auto& e : g.scene_bundle->entries)
    {
        if (e.type != ResourceType::CutsceneEvents || e.name.rfind(prefix, 0) != 0)
        {
            continue;
        }
        // grabbedbytheghoulies is a prefix of grabbedbytheghouliestwo. The event
        // file continues with '_' (the _camera suffix).
        if (e.name.size() > prefix.size() && e.name[prefix.size()] != '_')
        {
            continue;
        }
        size_t pos = 0;
        while (pos + 16 <= e.cpu.size())
        {
            const u32 size = rd_u32(e.cpu, pos);
            if (size < 16 || pos + size > e.cpu.size())
            {
                break;
            }
            const auto rec = e.cpu.subspan(pos, size);
            const u32 type = rd_u32(rec, 0x0c) & 0xffff;
            // 0x0004C850 case 0x0D / 0x1F. One name, destination byte at +0x90.
            if ((type == 0x0e || type == 0x20) && size > 0x90)
            {
                add_cue(rec, 0x90, 0x10);
            }
            // Case 0x1E. Up to five names, destination byte at +0x290. The first name is the one opened.
            else if (type == 0x1f && size > 0x290)
            {
                add_cue(rec, 0x290, 0x10);
            }
            pos += size;
        }
    }
}

void movie_set_root(const std::filesystem::path& game_dir)
{
    g_root = game_dir;
}

void movie_boot_probe()
{
    // 0x0014B280("d:\\movie\\0de65e94"). -1 calls GetLastError and WinMain continues.
    const std::filesystem::path path = g_root / "movie" / "0de65e94";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
    {
        std::cout << "movie probe " << path.string() << "\n";
    }
}

void movie_reset()
{
    while (g_movies != nullptr)
    {
        free_node(g_movies);
    }
    g_cues.clear();
    g_cue_clip.clear();
    g_cue_time = 0.f;
    g_now = 0.0;
}

void movie_alloc(const char* name, unsigned flags)
{
    if (!g_ready)
    {
        // 0x0015D380 push-buffer size, with 0x001073E0 / 0x00107400. Not translated.
        g_movies = nullptr;
        g_ready = true;
    }
    if (find_name(name) != nullptr)
    {
        return;
    }
    const std::string path = movie_path(name);
    if (path.empty())
    {
        return;
    }
    unsigned open_flags = kOpenAlpha;
    if ((flags & 8) != 0)
    {
        open_flags = 0x40100000u;
    }
    MovieNode* node = new MovieNode();
    node->loop = int((flags >> 2) & 1u);
    node->flags = flags;
    node->play = 0;
    node->decoded = 0;
    node->pending = 0;
    node->first_copy = 1;
    node->clip = nullptr;
    node->open_flags = open_flags;
    node->file = 0;
    node->enabled = 0;
    node->failed = 0;
    node->name = name == nullptr ? std::string() : name;
    node->path = path;
    if (movie_open(node) != 0)
    {
        delete node;
        return;
    }
    node->open_flags = open_flags | kOpenFromMemory;
    node->failed = 0;
    node->prev = nullptr;
    node->next = nullptr;
    if (g_movies == nullptr)
    {
        g_movies = node;
    }
    else
    {
        MovieNode* tail = g_movies;
        while (tail->next != nullptr)
        {
            tail = tail->next;
        }
        tail->next = node;
        node->prev = tail;
    }
    if ((flags & 2) != 0)
    {
        node->play += 1;
    }
}

void movie_open_gameselect()
{
    // 0x004290C8 stride 0x124, then 0x0042941C stride 0x10C. Both use flags 0x14.
    const char* empty_slots[] = {
        "aid_movie_ghoulies_gameselect_emptyslots_one",
        "aid_movie_ghoulies_gameselect_emptyslots_two",
        "aid_movie_ghoulies_gameselect_emptyslots_three",
    };
    const char* chapters[] = {
        "aid_movie_ghoulies_gameselect_chapter1",
        "aid_movie_ghoulies_gameselect_chapter2",
        "aid_movie_ghoulies_gameselect_chapter3",
        "aid_movie_ghoulies_gameselect_chapter4",
        "aid_movie_ghoulies_gameselect_chapter5",
    };
    for (const char* name : empty_slots)
    {
        movie_alloc(name, 0x14);
    }
    for (const char* name : chapters)
    {
        movie_alloc(name, 0x14);
    }
}

void movie_open_attract()
{
    movie_alloc("aid_movie_ghoulies_attractmode_logoscaleup", 6);
}

void movie_open_contents()
{
    const char* names[] = {
        "aid_movie_ghoulies_gameselect_house",
        "aid_movie_ghoulies_gameselect_zombieclap",
        "aid_movie_ghoulies_gameselect_skellyscratchhead",
    };
    for (const char* name : names)
    {
        movie_alloc(name, 6);
    }
}

void movie_advance_time(float dt)
{
    if (dt > 0.f)
    {
        g_now += double(dt);
    }
}

void movie_present()
{
    for (MovieNode* node = g_movies; node != nullptr; node = node->next)
    {
        movie_tick(node);
    }
}

void movie_note_visible(const char* name)
{
    MovieNode* node = find_name(name);
    if (node != nullptr && node->play < 1)
    {
        node->play += 1;
    }
}

void movie_sync_cutscene(const Game& g)
{
    if (!g.cutscenes.playing || g.cutscenes.index >= g.cutscenes.items.size())
    {
        for (MovieCue& cue : g_cues)
        {
            if (cue.started)
            {
                free_named(cue.name);
                cue.started = false;
            }
        }
        g_cues.clear();
        g_cue_clip.clear();
        g_cue_time = 0.f;
        return;
    }
    const Cutscene& clip = g.cutscenes.items[g.cutscenes.index];
    if (clip.name != g_cue_clip)
    {
        for (MovieCue& cue : g_cues)
        {
            if (cue.started)
            {
                free_named(cue.name);
            }
        }
        collect_cues(g, clip);
    }
    const float time = g.cutscenes.time;
    g_cue_time = time;
    for (MovieCue& cue : g_cues)
    {
        // 0x0004CC51 opens at the window start with flags 0x10. Play stays 0, so the tick
        // does not decode yet. 0x00106DD0 increments play when the window ends.
        if (time >= cue.start && !cue.started)
        {
            movie_alloc(cue.name.c_str(), 0x10);
            cue.started = true;
        }
    }
    for (const MovieCue& cue : g_cues)
    {
        if (time < cue.end)
        {
            continue;
        }
        MovieNode* node = find_name(cue.name.c_str());
        if (node == nullptr)
        {
            continue;
        }
        double step = 1.0 / 30.0;
        if (node->clip != nullptr && node->clip->frame_dur > 0.0)
        {
            step = node->clip->frame_dur;
        }
        int show = 0;
        if (time > cue.end)
        {
            show = int((double(time) - double(cue.end)) / step);
        }
        node->paced = 1;
        node->show_frame = show;
    }
}

void movie_apply(Mesh& mesh)
{
    for (RgbaImage& tex : mesh.textures)
    {
        if (tex.movie_slot == 0)
        {
            continue;
        }
        const MovieCue* live = nullptr;
        for (const MovieCue& cue : g_cues)
        {
            // The latest window whose end has passed owns the texture. Earlier
            // plays hold their last frame until that next one begins.
            if (cue.slot == int(tex.movie_slot) && g_cue_time >= cue.end)
            {
                live = &cue;
            }
        }
        // Slots 2/3/6/7 sit under stills 11/12/13/14 on the same page quad.
        // The still is later in the mesh, so the playing movie has to draw after it.
        const bool covered = tex.movie_slot == 2 || tex.movie_slot == 3 || tex.movie_slot == 6 || tex.movie_slot == 7;
        if (live == nullptr)
        {
            tex.movie_front = false;
            restore_movie(tex);
            continue;
        }
        bool running = true;
        if (const MovieNode* node = find_name(live->name.c_str()))
        {
            if (node->clip != nullptr && node->clip->frames > 0 && node->clip->frame_dur > 0.0)
            {
                const double span = double(node->clip->frames) * node->clip->frame_dur;
                running = double(g_cue_time) - double(live->end) < span;
            }
        }
        tex.movie_front = covered && running;
        const RgbaImage* frame = movie_image(live->name.c_str());
        if (frame == nullptr || frame->serial == tex.movie_seen)
        {
            continue;
        }
        paint_movie(tex, *frame);
        tex.movie_seen = frame->serial;
    }
}

const RgbaImage* movie_image(const char* name)
{
    MovieNode* node = find_name(name);
    if (node == nullptr || node->clip == nullptr || node->clip->image.bgra.empty())
    {
        return nullptr;
    }
    return &node->clip->image;
}

} // namespace gbtg
