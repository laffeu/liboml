#ifndef LOML_HPP
#define LOML_HPP
extern "C" {
#include "liboml.h"
}
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
namespace loml {

class table;
class section {
    friend class table;

    std::shared_ptr<::table> owner_;
    const ::section *section_;

    section(std::shared_ptr<::table> owner, const ::section *value)
        : owner_(std::move(owner)), section_(value) {}

    template<class T>
    std::optional<T> read(const char *key, bool (*getter)(const ::section *, const char *, T *)) const {
        T value{};
        if (*this && getter(section_, key, &value)) return value;
        return std::nullopt;
    }

public:
    explicit operator bool() const noexcept { return owner_ && section_; }

    section nested(const char *name) const {
        return {owner_, oml_nested(owner_.get(), section_, name)};
    }

    std::optional<std::string> string(const char *key) const {
        const auto *e = *this ? oml_value(section_, key) : nullptr;
        if (e && e->value) return std::string(e->value);
        return std::nullopt;
    }

    std::optional<std::vector<std::string>> array(const char *key) const {
        const auto *e = *this ? oml_value(section_, key) : nullptr;
        if (!e || !e->array) return std::nullopt;
        std::vector<std::string> items;
        for (auto p = e->array; *p; ++p)
            items.emplace_back(*p);
        return items;
    }

    std::optional<int> integer(const char *key) const {
        return read(key, oml_int);
    }

    std::optional<double> real(const char *key) const {
        return read(key, oml_double);
    }

    std::optional<bool> boolean(const char *key) const {
        return read(key, oml_bool);
    }
    template<class T = std::string>
    std::optional<T> get(const char *key) const {
        if constexpr (std::is_same_v<T, std::string>) return string(key);
        else if constexpr (std::is_same_v<T, int>) return integer(key);
        else if constexpr (std::is_same_v<T, double>) return real(key);
        else if constexpr (std::is_same_v<T, bool>) return boolean(key);
        else if constexpr (std::is_same_v<T, std::vector<std::string>>) return array(key);
        else static_assert(!std::is_same_v<T, T>, "Unsupported OML value type");
    }

};

class table {
    std::shared_ptr<::table> owner_;

public:
    explicit table(const std::string &path) : owner_(oml_get(path.c_str()), oml_free) {
        if (!owner_) throw std::runtime_error("Failed to load OML file: " + path);
    }

    loml::section root() const { return section(nullptr); }

    loml::section find(const char *name) const { return section(name); }

    loml::section section(const char *name) const {
        return {owner_, oml_section(owner_.get(), name)};
    }
};

}
#endif
