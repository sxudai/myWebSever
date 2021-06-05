#include<string>
#include<unordered_map>

#include "singleton.h"

std::unordered_map<std::string, std::string> MimeType::mime;

MimeType::MimeType(){
	mime[".html"] = "text/html";
    mime[".avi"] = "video/x-msvideo";
    mime[".bmp"] = "image/bmp";
    mime[".c"] = "text/plain";
    mime[".doc"] = "application/msword";
    mime[".gif"] = "image/gif";
    mime[".gz"] = "application/x-gzip";
    mime[".htm"] = "text/html";
    mime[".ico"] = "application/x-ico";
    mime[".jpg"] = "image/jpeg";
    mime[".png"] = "image/png";
    mime[".txt"] = "text/plain";
    mime[".mp3"] = "audio/mp3";
    mime[".jpeg"] = "image/jpeg";
    mime["default"] = "text/html";
}

std::string MimeType::getMime(const std::string &suffix){
    static MimeType instance;
    if (mime.find(suffix) == mime.end()) return mime["default"];
    return mime[suffix];
}