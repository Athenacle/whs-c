#include "whs/entity.h"

#include "whs-internal.h"
#include "fmt/format.h"

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stack>
#include <dirent.h>
#include <openssl/md5.h>
#include <fcntl.h>
#include <random>
using namespace whs;

#ifndef UNIX_HAVE_EPOLL
#error StaticFileServer without epoll(7) has not been implemented
#else
#include <sys/epoll.h>
#endif

#ifndef UNIX_HAVE_INOTIFY
#error StaticFileServer without inotify(7) has not been implemented
#else
#include <sys/inotify.h>
#endif

namespace
{
    static std::random_device rd;

    auto next_rand()
    {
        return rd();
    }

    struct ftype {
        const char* name;
        const char* mime;
    };

    static const char* default_mime = "application/octet-stream";
    // clang-format off
    static const ftype  mime_type[] = {
        {"war", "application/java-archive"},    {"jar", "application/java-archive"},
        {"ear", "application/java-archive"},    {"js", "application/javascript"},
        {"json", "application/json"},           {"hqx", "application/mac-binhex40"},
        {"doc", "application/msword"},          {"so", "application/octet-stream"},
        {"msp", "application/octet-stream"},    {"msm", "application/octet-stream"},
        {"msi", "application/octet-stream"},    {"iso", "application/octet-stream"},
        {"img", "application/octet-stream"},    {"exe", "application/octet-stream"},
        {"dmg", "application/octet-stream"},    {"dll", "application/octet-stream"},
        {"deb", "application/octet-stream"},    {"bin", "application/octet-stream"},
        {"pdf", "application/pdf"},             {"ps", "application/postscript"},
        {"eps", "application/postscript"},      {"ai", "application/postscript"},
        {"rtf", "application/rtf"},             {"m3u8", "application/vnd.apple.mpegurl"},
        {"kml", "application/vnd.google-earth.kml+xml"},
        {"kmz", "application/vnd.google-earth.kmz"},
        {"xls", "application/vnd.ms-excel"},
        {"eot", "application/vnd.ms-fontobject"},
        {"ppt", "application/vnd.ms-powerpoint"},
        {"odg", "application/vnd.oasis.opendocument.graphics"},
        {"odp", "application/vnd.oasis.opendocument.presentation"},
        {"ods", "application/vnd.oasis.opendocument.spreadsheet"},
        {"odt", "application/vnd.oasis.opendocument.text"},
        {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {"wmlc", "application/vnd.wap.wmlc"},       {"7z", "application/x-7z-compressed"},
        {"cco", "application/x-cocoa"},             {"jardiff", "application/x-java-archive-diff"},
        {"jnlp", "application/x-java-jnlp-file"},   {"run", "application/x-makeself"},
        {"pm", "application/x-perl"},               {"pl", "application/x-perl"},
        {"prc", "application/x-pilot"},             {"pdb", "application/x-pilot"},
        {"rar", "application/x-rar-compressed"},    {"rpm", "application/x-redhat-package-manager"},
        {"sea", "application/x-sea"},               {"swf", "application/x-shockwave-flash"},
        {"sit", "application/x-stuffit"},           {"tk", "application/x-tcl"},
        {"tcl", "application/x-tcl"},               {"pem", "application/x-x509-ca-cert"},
        {"der", "application/x-x509-ca-cert"},      {"crt", "application/x-x509-ca-cert"},
        {"xpi", "application/x-xpinstall"},         {"xhtml", "application/xhtml+xml"},
        {"xspf", "application/xspf+xml"},           {"zip", "application/zip"},
        {"midi", "audio/midi"},     {"mid", "audio/midi"},      {"kar", "audio/midi"},
        {"mp3", "audio/mpeg"},      {"ogg", "audio/ogg"},       {"m4a", "audio/x-m4a"},
        {"ra", "audio/x-realaudio"}, {"woff", "font/woff"},     {"woff2", "font/woff2"},
        {"gif", "image/gif"},       {"jpg", "image/jpeg"},      {"jpeg", "image/jpeg"},
        {"png", "image/png"},       {"svgz", "image/svg+xml"},  {"svg", "image/svg+xml"},
        {"tiff", "image/tiff"},     {"tif", "image/tiff"},      {"wbmp", "image/vnd.wap.wbmp"},
        {"webp", "image/webp"},     {"ico", "image/x-icon"},    {"jng", "image/x-jng"},
        {"bmp", "image/x-ms-bmp"},  {"css", "text/css"},        {"shtml", "text/html"},
        {"html", "text/html"},     {"htm", "text/html"},       {"txt", "text/plain"},
        {"xml", "text/xml"},        {"asf", "video/3gpp"},      {"ts", "video/mp2t"},
        {"mp4", "video/mp4"},       {"mpg", "video/mpeg"},      {"mpeg", "video/mpeg"},
        {"mov", "video/quicktime"}, {"webm", "video/webm"},     {"flv", "video/x-flv"},
        {"m4v", "video/x-m4v"},     {"mng", "video/x-mng"},     {"asx", "video/x-ms-asf"},
        {"wmv", "video/x-ms-wmv"},  {"avi", "video/x-msvideo"}
    };
    // clang-format on

    const char* search_mime(const char* name)
    {
        for (const auto& f : mime_type) {
            if (0 == strcmp(f.name, name)) {
                return f.mime;
            }
        }
        return default_mime;
    }
}  // namespace

uint32_t StaticFileServer::file::major_time = 0;

StaticFileServer::StaticFileServer(const std::string& prefix, const std::string& local)
    : fd(0), path(local), prefix(prefix)
{
    auto ct = time(nullptr);
    file::major_time = ct >> 32;
    if (path.at(path.length() - 1) != '/') {
        path.append("/");
    }
}

StaticFileServer::StaticFileServer(StaticFileServer&& another)
{
    fd = another.fd;
    another.fd = 0;
    path.swap((another.path));
    prefix.swap(another.prefix);
}

StaticFileServer::~StaticFileServer()
{
    if (fd != 0) {
        close(fd);
    }
}

bool StaticFileServer::start()
{
    struct stat st;
    int status = stat(path.c_str(), &st);
    if (status != 0) {
        logger::error(fmt::format(
            "inotify-init: access to directory {} failed:{}. StaticFileServer refused to start",
            path,
            strerror(errno)));
        return false;
    }
    if (!(st.st_mode & S_IFMT)) {
        logger::error(fmt::format(
            "inotify-init: '{}' not a directory. StaticFileServer refused to start", path));
        return false;
    }

    int ifd = inotify_init();
    logger::debug(fmt::format("inotify-init return {}", fd));
    if (ifd < 0) {
        logger::error(fmt::format(
            "inotify-init: inotify_init(2) {} failed:{}. StaticFileServer refused to start",
            path,
            strerror(errno)));
        return false;
    } else {
        status = inotify_add_watch(
            ifd, path.c_str(), IN_MODIFY | IN_DELETE | IN_CREATE | IN_MOVED_FROM | IN_MOVED_TO);
        if (status == -1) {
            logger::error(
                fmt::format("inotify-init: inotify_add_watch(2) {} failed:{}. StaticFileServer "
                            "refused to start",
                            path,
                            strerror(errno)));
            close(ifd);
            return false;
        } else {
            logger::info(fmt::format("inotify-init: inotify_add_watch(2) to {} success", path));
            fd = ifd;
            list_files();
            start_thread();
            return true;
        }
    }
}

void StaticFileServer::list_files()
{
    std::stack<std::string> dirs;
    const char ln[] = "StaticFile-startup";
    dirs.push(path);
    while (!dirs.empty()) {
        auto dir = dirs.top();
        dirs.pop();

        DIR* dirp = opendir(dir.c_str());
        if (dirp == nullptr) {
            logger::error(
                fmt::format("{}: open directory '{}' failed: {}", ln, dir, strerror(errno)));
            return;
        }
        std::string tname;
        tname.reserve(dir.length() + 256 + 20);
        while (auto f = readdir(dirp)) {
            tname.clear();
            auto fname = f->d_name;
            if ((fname[0] == '.' && fname[1] == '\0')
                || (fname[0] == '.' && fname[1] == '.' && fname[2] == '\0')) {
                continue;
            }
            tname = dir + "/" + fname;
            struct stat st;
            int status = stat(tname.c_str(), &st);
            if (status == 0) {
                bool regular = ((st.st_mode & S_IFMT) == S_IFREG);
                if (((st.st_mode & S_IFMT) == S_IFDIR)) {
                    dirs.push(tname);
                } else if (regular) {
                    if (stat(tname.c_str(), &st) == 0) {
                        tname = tname.substr(path.length() + 1);
                        file f(tname);
                        f.size = st.st_size;
                        f.last_save_time = 0xffffffff & st.st_mtim.tv_sec;
                        auto h = std::hash<std::string>{}(tname);
                        auto p = std::make_pair(h, f);
                        files.emplace(p);
                        logger::debug(fmt::format("track file {}", tname));
                    }
                }
            } else {
                logger::error(fmt::format("stat of file {} failed: {}", tname, strerror(errno)));
            }
        }
        closedir(dirp);
    }
}

void StaticFileServer::start_thread() {}

bool StaticFileServer::operator()(Request& req, Response& resp) const THROWS
{
    const static std::string cc = "public, max-age=31536000";
    auto mth = req.getMethod();
    if (mth != HTTP_GET && mth != HTTP_HEAD) {
        return true;
    }

    auto url = req.getBaseURL();
    if (prefix != "") {
        url = url.substr(prefix.length());
    }
    auto hash = std::hash<std::string>{}(url);
    const auto& f = files.find(hash);
    if (f == files.end()) {
        return true;
    } else {
        std::string tf;
        time_t st = f->second.get_save_time();
        auto t = gmtime(&st);
        utils::format_time(t, tf);
        resp.addHeader(utils::CommonHeader::ContentType, f->second.mime);
        resp.addHeader(utils::CommonHeader::Etag, f->second.etag);
        resp.addHeader(utils::CommonHeader::LastModified, tf);
        if (mth == HTTP_GET) {
            if (req.getHeader("if-none-match", tf)) {
                if (0
                    == std::char_traits<char>::compare(
                        tf.c_str(),
                        f->second.etag,
                        std::min(tf.length(), static_cast<size_t>(ETAG_LENGTH)))) {
                    resp.status(HTTP_STATUS_NOT_MODIFIED);
                    resp.addHeaderIfNotExists(utils::CommonHeader::CacheControl, cc);
                    return false;
                }
            }
            char* buf = new char[f->second.size];
            std::string fname = path + '/' + url;
            int rd = open(fname.c_str(), O_RDONLY);
            if (rd > 0) {
                auto nread = read(rd, buf, f->second.size);
                if (nread == f->second.size) {
                    resp.setBody(buf, f->second.size);
                    resp.addHeaderIfNotExists(utils::CommonHeader::CacheControl, cc);
                }
                close(rd);
            }
        }
        resp.addHeader(utils::CommonHeader::ContentLength, std::to_string(f->second.size));
    }
    return false;
}

StaticFileServer::file::file(const std::string& fname) : mime(default_mime)
{
    last_save_time = size = 0;
    for (uint8_t i = 0; i < ETAG_LENGTH + 1; i++) {
        etag[i] = 0;
    };
    auto dot = fname.find_last_of('.');
    if (dot != std::string::npos && dot != fname.length() - 1) {
        std::string tmp(fname, dot + 1);

        mime = search_mime(tmp.c_str());
    }
    calc_etag(fname);
}

void StaticFileServer::file::calc_etag(const std::string& fname)
{
    MD5_CTX ctx;
    MD5_Init(&ctx);
    unsigned char md5[MD5_DIGEST_LENGTH] = {0};
    std::string misc = fmt::format(
        "{}-{}-{}-{}", next_rand(), next_rand(), next_rand(), reinterpret_cast<void*>(this));
    misc.append(fname);

    MD5_Update(&ctx, misc.c_str(), misc.length());
    MD5_Final(md5, &ctx);
    std::string out;

    char buf[4] = {0};
    for (int i = 0; i < 16; i++) {
        snprintf(buf, 4, "%02x", md5[i]);
        out.append(buf);
    }
    memcpy(etag, out.c_str(), ETAG_LENGTH);
}