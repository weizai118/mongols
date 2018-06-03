#ifndef UTIL_HPP
#define UTIL_HPP



#include <ctime>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>


namespace mycpp {
    inline std::string md5(const std::string& str);

    inline std::string random_string(const std::string& s);

    inline void read_file(const std::string& path, std::string& out);

    inline bool is_file(const std::string& s);

    inline bool is_dir(const std::string& s);


    inline std::string http_time(time_t *t);

    inline time_t parse_http_time(u_char* value, size_t len);

    inline std::string trim(const std::string& s);

    inline void parse_param(const std::string& data, std::unordered_map<std::string, std::string>& result, char c, char cc);

    inline void split(const std::string& s, char delim, std::vector<std::string>& v);

    class random {
    public:

        random() : now(time(0)) {
            srand(this->now);
        }

        template<class T>
        T create(T left, T right) {
            return static_cast<T> (this->create()*(right - left) + left);
        }
    private:
        time_t now;

        double create() {
            return static_cast<double> (rand() / (RAND_MAX + 1.0));
        }
    };

    static std::string INTEGER = R"(^[+-]?[1-9]+[0-9]*$)"
            , NUMBER = R"(^[+-]?[1-9]+[0-9]*\.?[0-9]*$)"
            , EMAIL = R"(^[0-9a-zA-Z]+(([-_\.])?[0-9a-zA-Z]+)?\@[0-9a-zA-Z]+[-_]?[0-9a-zA-Z]+(\.[0-9a-zA-Z]+)+$)"
            , URL = R"(^(http[s]?|ftp)://[0-9a-zA-Z\._-]([0-9a-zA-Z]+/?)+\??.*$)";
}

#endif /* UTIL_HPP */

