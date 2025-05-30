#include "ffpp.hpp"

void FFPP::ffpp_api_convert(){
  const char* filename_in = this->in_filename.c_str();
  const char* filename_out = this->out_filename.c_str();

  //av_register_all();

  AVFormatContext* in_fmt_ctx = nullptr;
  if (avformat_open_input(&in_fmt_ctx, filename_in, nullptr, nullptr) < 0) {
    std::cerr << "Error opening input file.\n";
    std::exit(1);
  }

  if (avformat_find_stream_info(in_fmt_ctx, nullptr) < 0) {
    std::cerr << "error fetching stream information.\n";
    std::exit(1);
  }

  const AVOutputFormat* out_fmt = av_guess_format(this->ext.c_str(), nullptr, nullptr);
  if (!out_fmt) {
    std::cerr << "Format '" << this->ext << "' not supported.\n";
    std::exit(1);
  }

  AVFormatContext* out_fmt_ctx = nullptr;
  if (avformat_alloc_output_context2(&out_fmt_ctx, out_fmt, nullptr, filename_out) < 0) {
    std::cerr << "Error allocating output context.\n";
    std::exit(1);
  }


  for (unsigned i = 0; i < in_fmt_ctx->nb_streams; i++) {
    AVStream* in_stream = in_fmt_ctx->streams[i];
    AVCodecParameters* in_codecpar = in_stream->codecpar;

    AVStream* out_stream = avformat_new_stream(out_fmt_ctx, nullptr);
    if (!out_stream) {
      std::cerr << "Error creating output stream.\n";
      std::exit(1);
    }

    if (avcodec_parameters_copy(out_stream->codecpar, in_codecpar) < 0) {
      std::cerr << "Error copying codec parameters.\n";
      std::exit(1);
    }
    out_stream->codecpar->codec_tag = 0;
  }

  if (!(out_fmt->flags & AVFMT_NOFILE)) {
    if (avio_open(&out_fmt_ctx->pb, filename_out, AVIO_FLAG_WRITE) < 0) {
      std::cerr << "Error opening output file.\n";
      std::exit(1);
    }
  }

  if (avformat_write_header(out_fmt_ctx, nullptr) < 0) {
    std::cerr << "Error writing header.\n";
    std::exit(1);
  }

  AVPacket pkt;
  while (av_read_frame(in_fmt_ctx, &pkt) >= 0) {
    AVStream* in_stream = in_fmt_ctx->streams[pkt.stream_index];
    AVStream* out_stream = out_fmt_ctx->streams[pkt.stream_index];

    pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
        (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
        (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
    pkt.pos = -1;

    if (av_interleaved_write_frame(out_fmt_ctx, &pkt) < 0) {
      std::cerr << "Error writing frame.\n";
      break;
    }
    av_packet_unref(&pkt);
  }

  av_write_trailer(out_fmt_ctx);
  avformat_close_input(&in_fmt_ctx);

  if (!(out_fmt->flags & AVFMT_NOFILE))
    avio_closep(&out_fmt_ctx->pb);

  avformat_free_context(out_fmt_ctx);
}

void FFPP::ffpp_api_extract(){
  const char* filename = this->in_filename.c_str();

  AVFormatContext* fmt_ctx = nullptr;
  if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) < 0) {
    std::cerr << "Error opening file.\n";
    std::exit(1);
  }

  if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
    std::cerr << "Error fetching stream information.\n";
    std::exit(1);
  }

  int video_stream_index = -1;
  for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
    if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_index = i;
      break;
    }
  }
  if (video_stream_index == -1) {
    std::cerr << "Video stream not found.\n";
    std::exit(1);
  }

  AVCodecParameters* codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
  const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
  AVCodecContext* codec_ctx = avcodec_alloc_context3(decoder);
  avcodec_parameters_to_context(codec_ctx, codecpar);
  avcodec_open2(codec_ctx, decoder, nullptr);

  AVFrame* frame = av_frame_alloc();
  AVFrame* rgb_frame = av_frame_alloc();
  AVPacket packet;
  int frame_index = 0;

  SwsContext* sws_ctx = sws_getContext(
      codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
      codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,
      SWS_BILINEAR, nullptr, nullptr, nullptr);

  int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width,
      codec_ctx->height, 1);
  uint8_t* buffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
  av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, buffer,
      AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);

  while (av_read_frame(fmt_ctx, &packet) >= 0) {
    if (packet.stream_index == video_stream_index) {
      if (avcodec_send_packet(codec_ctx, &packet) == 0) {
        while (avcodec_receive_frame(codec_ctx, frame) == 0) {
          sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height,
              rgb_frame->data, rgb_frame->linesize);
          // Dont use: .png por .bmp, .jpg, ...(Only .ppm)
          ffpp_save_img(rgb_frame, codec_ctx->width, codec_ctx->height, frame_index++, this->img_ext);
        }
      }
    }
    av_packet_unref(&packet);
  }

  av_free(buffer);
  av_frame_free(&frame);
  av_frame_free(&rgb_frame);
  avcodec_free_context(&codec_ctx);
  avformat_close_input(&fmt_ctx);
  sws_freeContext(sws_ctx);
}

std::vector<std::string> FFPP::ffpp_api_info() {
  const char* filename = this->in_filename.c_str();
  std::vector<std::string> info(5);

  AVFormatContext* fmt_ctx = nullptr;
  if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) < 0) {
    throw std::runtime_error("Error opening file.");
  }

  if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
    avformat_close_input(&fmt_ctx);
    throw std::runtime_error("Error getting stream information.");
  }

  // Duration
  int64_t duration = fmt_ctx->duration;
  if (duration != AV_NOPTS_VALUE) {
    int seconds = duration / AV_TIME_BASE;
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << std::ceil(h) << ":"
      << std::setw(2) << m << ":"
      << std::setw(2) << s;
    info[0] = oss.str();
  } else {
    info[0] = "N/A";
  }

  // Bitrate
  if (fmt_ctx->bit_rate > 0) {
    info[1] = std::to_string(fmt_ctx->bit_rate / 1000) + " kb/s";
  } else {
    info[1] = "N/A";
  }

  AVCodecParameters* video_par = nullptr;
  AVCodecParameters* audio_par = nullptr;
  AVRational video_frame_rate = {0,1};

  for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
    AVStream* stream = fmt_ctx->streams[i];
    AVCodecParameters* codecpar = stream->codecpar;

    if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !video_par) {
      video_par = codecpar;
      video_frame_rate = av_guess_frame_rate(fmt_ctx, stream, nullptr);
    } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !audio_par) {
      audio_par = codecpar;
    }
  }

  // Resolution
  if (video_par && video_par->width > 0 && video_par->height > 0) {
    info[2] = std::to_string(video_par->width) + "x" + std::to_string(video_par->height);
  } else {
    info[2] = "N/A";
  }

  // FPS
  if (video_frame_rate.num > 0 && video_frame_rate.den > 0) {
    double fps = av_q2d(video_frame_rate);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << fps;
    info[3] = oss.str();
  } else {
    info[3] = "N/A";
  }

  // Audio frequency
  if (audio_par && audio_par->sample_rate > 0) {
    info[4] = std::to_string(audio_par->sample_rate) + " Hz";
  } else {
    info[4] = "N/A";
  }

  avformat_close_input(&fmt_ctx);
  return info;
}

void FFPP::ffpp_api_cut(){
  const char* in_filename = this->in_filename.c_str();
  const char* out_filename = this->out_filename.c_str();
  const int64_t start_time_sec = this->start_secs;
  const int64_t duration_sec = this->duration_time;

  AVFormatContext* in_fmt_ctx = nullptr;
  AVFormatContext* out_fmt_ctx = nullptr;
  AVPacket* pkt = av_packet_alloc();
  if (!pkt) {
    throw std::runtime_error("Failed to allocate AVPacket.");
  }

  try {
    this->check_error(avformat_open_input(&in_fmt_ctx, in_filename, nullptr, nullptr), 
        "Error opening input.");
    this->check_error(avformat_find_stream_info(in_fmt_ctx, nullptr), 
        "Error getting input info.");

    int video_stream_idx = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_idx < 0) {
      throw std::runtime_error("No video streams found.");
    }

    int64_t start_timestamp = start_time_sec * AV_TIME_BASE;
    int64_t end_timestamp = (start_time_sec + duration_sec) * AV_TIME_BASE;

    AVStream* video_stream = in_fmt_ctx->streams[video_stream_idx];
    int64_t seek_target = av_rescale_q(start_timestamp, AV_TIME_BASE_Q, video_stream->time_base);

    this->check_error(av_seek_frame(in_fmt_ctx, video_stream_idx, seek_target, AVSEEK_FLAG_BACKWARD),
        "Error doing initial seek.");

    this->check_error(avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, out_filename),
        "Error allocating output.");

    std::vector<int> stream_mapping(in_fmt_ctx->nb_streams, -1);
    int out_stream_index = 0;

    for (unsigned i = 0; i < in_fmt_ctx->nb_streams; ++i) {
      AVStream* in_stream = in_fmt_ctx->streams[i];
      AVCodecParameters* in_codecpar = in_stream->codecpar;

      if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
          in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
          in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
        continue;
      }

      stream_mapping[i] = out_stream_index++;
      AVStream* out_stream = avformat_new_stream(out_fmt_ctx, nullptr);
      if (!out_stream) {
        throw std::runtime_error("Failed to create stream in output.");
      }

      this->check_error(avcodec_parameters_copy(out_stream->codecpar, in_codecpar),
          "Error copying codec parameters.");
      out_stream->codecpar->codec_tag = 0;
      out_stream->time_base = in_stream->time_base;
    }

    if (!(out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
      this->check_error(avio_open(&out_fmt_ctx->pb, out_filename, AVIO_FLAG_WRITE),
          "Error opening output file.");
    }

    this->check_error(avformat_write_header(out_fmt_ctx, nullptr), "Error writing header.");

    std::vector<int64_t> stream_start_pts(in_fmt_ctx->nb_streams, AV_NOPTS_VALUE);
    std::vector<int64_t> stream_start_dts(in_fmt_ctx->nb_streams, AV_NOPTS_VALUE);
    bool video_started = false;
    bool found_first_keyframe = false;
    int packets_written = 0;

    while (av_read_frame(in_fmt_ctx, pkt) >= 0) {
      if (pkt->stream_index >= static_cast<int>(stream_mapping.size()) || 
          stream_mapping[pkt->stream_index] < 0) {
        av_packet_unref(pkt);
        continue;
      }

      AVStream* in_stream = in_fmt_ctx->streams[pkt->stream_index];

      int64_t pkt_time = AV_NOPTS_VALUE;
      if (pkt->pts != AV_NOPTS_VALUE) {
        pkt_time = av_rescale_q(pkt->pts, in_stream->time_base, AV_TIME_BASE_Q);
      }

      if (pkt_time != AV_NOPTS_VALUE && pkt_time >= end_timestamp) {
        av_packet_unref(pkt);
        break;
      }

      if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        if (!found_first_keyframe) {
          if (pkt_time >= start_timestamp && (pkt->flags & AV_PKT_FLAG_KEY)) {
            found_first_keyframe = true;
            video_started = true;
            stream_start_pts[pkt->stream_index] = pkt->pts;
            if (pkt->dts != AV_NOPTS_VALUE) {
              stream_start_dts[pkt->stream_index] = pkt->dts;
            }
          } else {
            av_packet_unref(pkt);
            continue;
          }
        } else if (!video_started) {
          av_packet_unref(pkt);
          continue;
        }
      } else {
        if (!video_started && pkt_time < start_timestamp) {
          av_packet_unref(pkt);
          continue;
        }

        if (stream_start_pts[pkt->stream_index] == AV_NOPTS_VALUE) {
          if (pkt_time >= start_timestamp || video_started) {
            stream_start_pts[pkt->stream_index] = pkt->pts;
            if (pkt->dts != AV_NOPTS_VALUE) {
              stream_start_dts[pkt->stream_index] = pkt->dts;
            }
          } else {
            av_packet_unref(pkt);
            continue;
          }
        }
      }

      AVStream* out_stream = out_fmt_ctx->streams[stream_mapping[pkt->stream_index]];

      if (stream_start_pts[pkt->stream_index] != AV_NOPTS_VALUE) {

        if (pkt->pts != AV_NOPTS_VALUE) {
          int64_t adjusted_pts = pkt->pts - stream_start_pts[pkt->stream_index];
          pkt->pts = av_rescale_q(adjusted_pts, in_stream->time_base, out_stream->time_base);
          if (pkt->pts < 0) pkt->pts = 0;
        }

        if (pkt->dts != AV_NOPTS_VALUE && stream_start_dts[pkt->stream_index] != AV_NOPTS_VALUE) {
          int64_t adjusted_dts = pkt->dts - stream_start_dts[pkt->stream_index];
          pkt->dts = av_rescale_q(adjusted_dts, in_stream->time_base, out_stream->time_base);
          if (pkt->dts < 0) pkt->dts = 0;
        } else if (pkt->dts != AV_NOPTS_VALUE) {
          pkt->dts = pkt->pts;
        }

        if (pkt->pts != AV_NOPTS_VALUE && pkt->dts != AV_NOPTS_VALUE) {
          if (pkt->pts < pkt->dts) {
            pkt->pts = pkt->dts;
          }
        }

        if (pkt->duration > 0) {
          pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
        }
      }

      pkt->pos = -1;
      pkt->stream_index = stream_mapping[pkt->stream_index];

      this->check_error(av_interleaved_write_frame(out_fmt_ctx, pkt), "Error writing frame.");
      packets_written++;

      av_packet_unref(pkt);
    }

    this->check_error(av_write_trailer(out_fmt_ctx), "Error writing trailer.");

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << '.' << std::endl;

    if (out_fmt_ctx && !(out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
      avio_closep(&out_fmt_ctx->pb);
    }
    av_packet_free(&pkt);
    if (in_fmt_ctx) avformat_close_input(&in_fmt_ctx);
    if (out_fmt_ctx) avformat_free_context(out_fmt_ctx);
    std::exit(1);
  }

  if (out_fmt_ctx && !(out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&out_fmt_ctx->pb);
  }

  av_packet_free(&pkt);
  if (in_fmt_ctx) avformat_close_input(&in_fmt_ctx);
  if (out_fmt_ctx) avformat_free_context(out_fmt_ctx);
}
