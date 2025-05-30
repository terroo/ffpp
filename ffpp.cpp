#include "ffpp.hpp"

void init_console_utf8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

FFPP::FFPP(){

  init_console_utf8();

  out_filename = {};
  in_filename = {};
  ext = {};
  dir = ".";
  img_ext = ".ppm"; // save img
  start_secs = {};
  duration_time = {};

  exts_img = {".ppm", ".jpg", ".png", ".bmp"};
  exts_vid = {"mp4", "flv", "mov", "wmv"};
}

std::string FFPP::ffpp_ext(const std::string& filename){
  auto pos = filename.find_last_of('.');
  if(pos != std::string::npos){
    ext = filename.substr(pos + 1);
  }

  if(std::find(exts_vid.begin(), exts_vid.end(), ext) == exts_vid.end()) {
    std::cerr << "Invalid format: '" << ext << "'.\n";
    std::exit(EXIT_FAILURE);
  }

  if(ext == "wmv"){ ext = "asf"; }

  if(!ext.empty()){
    return ext;
  }

  throw std::runtime_error("File without extension.");
}

void FFPP::ffpp_convert(const std::string& in, const std::string& out){
  this->in_filename = in;
  this->out_filename = out;

  this->ffpp_ext(in);
  this->ffpp_ext(out);

  this->ffpp_api_convert();
}

void FFPP::ffpp_extract_frames(
    const std::string& in, 
    const std::optional<fs::path>& dir_frames/*, const std::optional<std::string>& fileext*/){

  if(dir_frames.has_value()){ this->dir = dir_frames.value(); } 

  this->ffpp_ext(in);
  this->in_filename = in; 

  /*if(fileext.has_value()){ 
    this->img_ext = fileext.value(); 
    if(this->img_ext[0] != '.' || this->img_ext.size() != 4){
    std::cerr << "Invalid format: '" << this->img_ext << "'.\n";
    std::exit(1);
    }
    } */

  this->ffpp_api_extract();
}

void FFPP::ffpp_save_img(AVFrame* frame, int width, int height, int index, const std::string& img_format) {

  if(!can_write()){
    std::cerr << "Directory for frames does not exist or does not have permission.\n";
    std::exit(1);
  }

  fs::path filepath = this->dir / ("frame" + std::to_string(index) + img_format);

  std::ofstream ofs(filepath, std::ios::binary);
  if(!ofs){
    std::cerr << "Error opening file for writing: " << filepath << "\n";
    return;
  }

  ofs << "P6\n" << width << " " << height << "\n255\n";

  for (int y = 0; y < height; ++y){
    ofs.write(reinterpret_cast<char*>(frame->data[0] + y * frame->linesize[0]), width * 3);

  }

  ofs.close();
}

bool FFPP::can_write(){
  if (!fs::exists(dir)) {
    if (!fs::create_directories(dir)) {
      std::cerr << "Failed to create directory: " << dir << "\n";
      return false;
    }
  }
  if (!fs::is_directory(dir)) {
    std::cerr << "Path is not directory: " << dir << "\n";
    return false;
  }

  try {
    fs::path temp_file = dir / ".__perm_test__";
    std::ofstream ofs(temp_file);
    if(!ofs){ return false; }
    ofs.close();
    fs::remove(temp_file);
    return true;
  } catch (...) {
    return false;
  }
}

std::string FFPP::ffpp_info(FFPP_INFO info, const std::string& video){
  this->ffpp_ext(video);
  this->in_filename = video; 
  std::vector<std::string> all = this->ffpp_api_info();

  int index = static_cast<int>(info);

  if(index < 0 || index >= static_cast<int>(all.size())){
    throw std::runtime_error("Invalid enum value.\n");
  }

  return all[index];
}

void FFPP::ffpp_cut(const std::string& in, const std::string& start, 
        int time, const std::string& out){

  this->ffpp_ext(in);
  this->ffpp_ext(out);

  this->in_filename = in;
  this->out_filename = out;

  if(!this->same_ext()){
    std::exit(1);
  }

  if(!this->validate_time_format(start)){
    std::cerr << "Incorrect 'start: " + start + "' format.\n";
    std::exit(1);
  }
  this->start_secs = this->hms_to_seconds(start);
  this->duration_time = time;

  this->ffpp_api_cut();
}

void FFPP::check_error(int ret, const char* msg) {
  if (ret < 0) {
    char errbuf[128];
    av_strerror(ret, errbuf, sizeof(errbuf));
    throw std::runtime_error(std::string(msg) + ": " + errbuf);
  }
}

bool FFPP::same_ext() {
  auto ext1 = fs::path(in_filename).extension().string();
  auto ext2 = fs::path(out_filename).extension().string();

  if (!ext1.empty() && ext1[0] == '.') ext1.erase(0, 1);
  if (!ext2.empty() && ext2[0] == '.') ext2.erase(0, 1);

  return ext1 == ext2;
}

bool FFPP::is_valid_part(const std::string& part) {
  if (part.size() != 2) return false;
  if (!isdigit(part[0]) || !isdigit(part[1])) return false;
  int val = std::stoi(part);
  return val >= 0 && val <= 59;
}

bool FFPP::validate_time_format(const std::string& input) {
  std::vector<std::string> parts;
  std::stringstream ss(input);
  std::string token;

  while (std::getline(ss, token, ':')) {
    parts.push_back(token);
  }

  if (parts.size() < 1 || parts.size() > 3) return false;

  for (const auto& part : parts) {
    if (!this->is_valid_part(part)) return false;
  }

  return true;
}

int FFPP::hms_to_seconds(const std::string& time_str) {
    int h = 0, m = 0, s = 0;
    char sep1, sep2;

    std::istringstream iss(time_str);
    if(!(iss >> h >> sep1 >> m >> sep2 >> s) || sep1 != ':' || sep2 != ':'){
        throw std::invalid_argument("Invalid time format: " + time_str);
    }

    if(m < 0 || m > 59 || s < 0 || s > 59 || h < 0){
        throw std::out_of_range("Time value out of range: " + time_str);
    }

    return h * 3600 + m * 60 + s;
}
