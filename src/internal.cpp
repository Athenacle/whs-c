#include "config.h"

#include "whs-internal.h"
#include "fmt/format.h"

using namespace whs::utils;

// mutex functions
#ifdef UNIX_HAVE_PTHREAD_MUTEX
using mutex_raw_type = pthread_mutex_t;
whs::utils::mutex::mutex()
{
    auto m = new pthread_mutex_t;
    pthread_mutex_init(m, nullptr);
    data = m;
}

whs::utils::mutex::~mutex()
{
    auto m = reinterpret_cast<pthread_mutex_t *>(data);
    pthread_mutex_destroy(m);
    delete m;
}

void whs::utils::mutex::lock()
{
    auto m = reinterpret_cast<pthread_mutex_t *>(data);
    pthread_mutex_lock(m);
}

void whs::utils::mutex::unlock()
{
    auto m = reinterpret_cast<pthread_mutex_t *>(data);
    pthread_mutex_unlock(m);
}
#else
#include <mutex>
using mutex_raw_type = std::mutex;
whs::utils::mutex::mutex()
{
    auto m = new mutex_raw_type;
    data = m;
}

whs::utils::mutex::~mutex()
{
    auto m = reinterpret_cast<mutex_raw_type*>(data);
    delete m;
}

void whs::utils::mutex::lock()
{
    auto m = reinterpret_cast<mutex_raw_type*>(data);
    m->lock();
}

void whs::utils::mutex::unlock()
{
    auto m = reinterpret_cast<mutex_raw_type*>(data);
    m->unlock();
}
#endif

// mutex functions end

// regex
#ifdef HAVE_LIBPCRE2
#include <pcre2.h>

void regex::init_pcre2()
{
    pcre2_pattern_info(re, PCRE2_INFO_NAMECOUNT, &namecount);
    pcre2_pattern_info(re, PCRE2_INFO_NAMETABLE, &name_table);
    pcre2_pattern_info(re, PCRE2_INFO_NAMEENTRYSIZE, &name_entry_size);
    pcre2_pattern_info(re, PCRE2_INFO_ALLOPTIONS, &ob);
    pcre2_pattern_info(re, PCRE2_INFO_NEWLINE, &nl);
}

bool regex::execute(const char *test, std::vector<regex_group> &group) const
{
    auto status = false;
    auto sub = reinterpret_cast<PCRE2_SPTR>(test);
    auto subl = std::char_traits<char>::length(test);
    auto match_data = pcre2_match_data_create_from_pattern(re, nullptr);
    auto rc = pcre2_match(re, sub, subl, 0, 0, match_data, nullptr);
    if (rc > 0) {
        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(match_data);
        auto utf8 = (ob & PCRE2_UTF) != 0;
        auto crlf =
            nl == PCRE2_NEWLINE_ANY || nl == PCRE2_NEWLINE_CRLF || nl == PCRE2_NEWLINE_ANYCRLF;

        regex_group first;
        for (int i = 0; i < rc; i++) {
            PCRE2_SPTR substring_start = sub + ov[2 * i];
            PCRE2_SIZE substring_length = ov[2 * i + 1] - ov[2 * i];
            std::string t((char *)substring_start, (int)substring_length);
            first.matches.emplace_back(std::move(t));
        }
        if (namecount != 0) {
            auto tabptr = name_table;
            for (PCRE2_SIZE i = 0; i < namecount; i++) {
                int n = (tabptr[0] << 8) | tabptr[1];
                std::string k(reinterpret_cast<const char *>(tabptr) + 2);
                std::string v(reinterpret_cast<const char *>(sub + ov[2 * n]),
                              static_cast<unsigned>((ov[2 * n + 1] - ov[2 * n])));
                tabptr += name_entry_size;
                first.named_matches.emplace(std::move(k), std::move(v));
            }
        }
        group.emplace_back(std::move(first));

        status = true;
        for (;;) {
            regex_group current;
            uint32_t options = 0;
            PCRE2_SIZE start_offset = ov[1];

            if (ov[0] == ov[1]) {
                if (ov[0] == subl)
                    break;
                options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
            } else {
                PCRE2_SIZE startchar = pcre2_get_startchar(match_data);
                if (start_offset <= startchar) {
                    if (startchar >= subl)
                        break;
                    start_offset = startchar + 1;
                    if (utf8) {
                        for (; start_offset < subl; start_offset++)
                            if ((sub[start_offset] & 0xc0) != 0x80)
                                break;
                    }
                }
            }

            rc = pcre2_match(re, sub, subl, start_offset, options, match_data, nullptr);

            if (rc == PCRE2_ERROR_NOMATCH) {
                if (options == 0)
                    break;
                ov[1] = start_offset + 1;
                if (crlf && start_offset < subl - 1 && sub[start_offset] == '\r'
                    && sub[start_offset + 1] == '\n')
                    ov[1] += 1;
                else if (utf8) {
                    while (ov[1] < subl) {
                        if ((sub[ov[1]] & 0xc0) != 0x80)
                            break;
                        ov[1] += 1;
                    }
                }
                continue;
            }

            if (rc < 0) {
                status = false;
                break;
            }
            for (int i = 0; i < rc; i++) {
                PCRE2_SPTR substring_start = sub + ov[2 * i];
                size_t substring_length = ov[2 * i + 1] - ov[2 * i];
                std::string m(reinterpret_cast<const char *>(substring_start), substring_length);
                current.matches.emplace_back(std::move(m));
            }
            if (namecount != 0) {
                PCRE2_SPTR tabptr = name_table;
                auto &ngroups = current.named_matches;
                for (uint32_t i = 0; i < namecount; i++) {
                    int n = (tabptr[0] << 8) | tabptr[1];
                    std::string k(reinterpret_cast<const char *>(tabptr) + 2);
                    std::string v(reinterpret_cast<const char *>(sub + ov[2 * n]),
                                  static_cast<unsigned>((ov[2 * n + 1] - ov[2 * n])));
                    tabptr += name_entry_size;
                    ngroups.emplace(std::move(k), std::move(v));
                }
            }
            group.emplace_back(std::move(current));
        }
    }
    pcre2_match_data_free(match_data);
    return status;
}

regex *regex::compile(const char *pattern)
{
    auto ret = new regex;
    return compile(pattern, *ret) ? ret : nullptr;
}

bool regex::compile(char const *pattern, whs::utils::regex &re)
{
    int errnumber;
    PCRE2_SIZE offset;

    auto ret = pcre2_compile(reinterpret_cast<PCRE2_SPTR8>(pattern),
                             PCRE2_ZERO_TERMINATED,
                             0,
                             &errnumber,
                             &offset,
                             NULL);
    if (ret) {
        re.re = ret;
        re.init_pcre2();
        return true;
    } else {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errnumber, buffer, sizeof(buffer));
        logger::error(fmt::format("whs-core: PCRE2 compilation failed at pattern {} offset {}: {}",
                                  pattern,
                                  static_cast<int>(offset),
                                  buffer));
        return false;
    }
}

void regex::swap(regex &other)
{
    std::swap(other.re, re);
    std::swap(other.ob, ob);
    std::swap(other.nl, nl);
    std::swap(other.name_table, name_table);
    std::swap(other.namecount, other.namecount);
    std::swap(other.name_entry_size, name_entry_size);
}

regex::regex()
{
#ifdef HAVE_BZERO
    bzero(this, sizeof(*this));
#else
    memset(this, 0, sizeof(*this));
#endif
}

regex::regex(regex &&other)
{
    re = other.re;
    other.re = nullptr;
}

regex::~regex()
{
    pcre2_code_free(re);
}

bool regex::match(const char *test) const
{
    auto match_data = pcre2_match_data_create_from_pattern(re, NULL);
    auto len = std::char_traits<char>::length(test);
    auto rc = pcre2_match(re, reinterpret_cast<PCRE2_SPTR8>(test), len, 0, 0, match_data, NULL);
    bool ret = rc == 1;
    pcre2_match_data_free(match_data);
    return ret;
}
#endif