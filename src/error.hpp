#pragma once

#include <exception>
#include <sstream>
#include <string>
#include <utility>

class TomasuloError : public std::exception {
  public:
    std::string content{};

    template <class First, class... Rest> TomasuloError(First&& first, Rest&&... rest) {
        std::ostringstream sout{};
        sout << first;
        ((sout << ' ' << rest), ...);
        content = sout.str();
    }

    TomasuloError() = default;

    virtual const char* what() const noexcept override {
        return content.c_str();
    }
};