#pragma once

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
}
#define MY_GL_TEXTURE_TYPE GL_TEXTURE_RECTANGLE
#define MY_GL_PIXEL_TYPE GL_BGRA
#define MY_AV_PIXEL_TYPE AV_PIX_FMT_BGRA
#define INBUF_SIZE 4096
#include <GL/glew.h>
#include <string>
class FFDecoder
{

private:
	AVFormatContext *_fmt_ctx;
	int _stream_idx;
	AVStream *_video_stream;
	AVCodecContext *_codec_ctx;
	AVCodec *_decoder;
	AVPacket *_packet;
	AVFrame *_av_frame;
	AVFrame *_gl_frame;
	struct SwsContext *_conv_ctx;
	std::string filename;
	GLuint _frame_tex;

public:
	FFDecoder(std::string fn);
	bool renderVideoFrame(bool next);
	bool init();
	~FFDecoder();

};

