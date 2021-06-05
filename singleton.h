#include<unordered_map>
#include<string>

class MimeType {
private:
    MimeType();
    static std::unordered_map<std::string, std::string> mime;
    
    MimeType(const MimeType &m);
    MimeType& operator=(const MimeType&);
public:
    static std::string getMime(const std::string &suffix);
};