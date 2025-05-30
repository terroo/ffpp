#pragma once

extern "C" {
  #include <libavformat/avformat.h>
  #include <libavcodec/avcodec.h>
  #include <libavutil/opt.h>
  #include <libavutil/timestamp.h>
  #include <libswscale/swscale.h>
  #include <libavutil/imgutils.h>
}

#include <iostream>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
  #include <windows.h>
#endif

namespace fs = std::filesystem;

enum FFPP_INFO {
    DURATION,
    BITRATE,
    RESOLUTION,
    FPS,
    AUDIO_FREQUENCY
};

class FFPP {
  std::string in_filename, out_filename, 
    ext, img_ext;
  fs::path dir;
  int start_secs, duration_time;

  std::vector<std::string> exts_img;
  std::vector<std::string> exts_vid;

  // API
  void ffpp_api_convert();
  void ffpp_api_extract();
  std::vector<std::string> ffpp_api_info();
  void ffpp_api_cut();

  // Utils
  std::string ffpp_ext(const std::string&);
  void ffpp_save_img(AVFrame*, int, int, int, const std::string&);
  bool can_write();
  void check_error(int ret, const char* msg);
  bool same_ext();
  bool is_valid_part(const std::string& part);
  bool validate_time_format(const std::string& input);
  int hms_to_seconds(const std::string& time_str);

  public:
    FFPP();
    void ffpp_convert(const std::string& in, const std::string& out);
    void ffpp_extract_frames(
        const std::string& in, 
        const std::optional<fs::path>& dir_frames = std::nullopt/*, 
        const std::optional<std::string>& fileext = std::nullopt*/);
    std::string ffpp_info(FFPP_INFO info, const std::string&);
    void ffpp_cut(const std::string& in, const std::string& start, 
        int time, const std::string& out);
};
