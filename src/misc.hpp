#pragma once

#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>
#include <ostream>
#include <iostream>

#define _ auto

using str = std::string;
using std::cout;

template<class t>
using vec = std::vector<t>;

class t_loc {
    str _filename;
    int _line;
    int _column;
public:
    bool operator==(const t_loc& x) const {
        return (_filename == x._filename
                and _line == x._line and _column == x._column);
    }
    bool operator!=(const t_loc& x) const {
        return !(*this == x);
    }
    t_loc(const str& filename_ = "", int line_ = 1, int column_ = 0)
        : _filename(filename_)
        , _line(line_)
        , _column(column_) {
    }
    int line() const {
        return _line;
    }
    int column() const {
        return _column;
    }
    const str& filename() const {
        return _filename;
    }
    void inc(bool newline) {
        if (newline) {
            _line++;
            _column = 0;
        } else {
            _column++;
        }
    }
    bool is_valid() const {
        return _filename != "";
    }
};

class t_compile_error : public std::runtime_error {
    t_loc _loc;
public:
    t_compile_error(const str& n_str, t_loc n_loc = t_loc())
        : std::runtime_error(n_str), _loc(n_loc) {
    }
    const t_loc& loc() const {
        return _loc;
    }
};

template <class t>
auto has(const vec<t>& c, const t& e) {
    return std::find(c.begin(), c.end(), e) != c.end();
}

void constrain(bool, const str&, const t_loc&);
str read_file_into_string(std::ifstream&);
void print_bytes(const str&, std::ostream&);
