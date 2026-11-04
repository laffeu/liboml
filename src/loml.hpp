#pragma once

#include "liboml.h"

#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>
#include <initializer_list>

namespace loml
{
    template<typename T>
    std::unique_ptr<T> convert(char *value);

    template<>
    inline std::unique_ptr<std::string> convert<std::string>(char *value)
    {
        if (!value)
            return nullptr;

        return std::make_unique<std::string>(value);
    }

    template<>
    inline std::unique_ptr<int> convert<int>(char *value)
    {
        if (!value)
            return nullptr;

        char *end = nullptr;
        long result = std::strtol(value, &end, 10);

        if (value == end || *end != '\0')
            return nullptr;

        return std::make_unique<int>(static_cast<int>(result));
    }

    template<>
    inline std::unique_ptr<float> convert<float>(char *value)
    {
        if (!value)
            return nullptr;

        char *end = nullptr;
        float result = std::strtof(value, &end);

        if (value == end || *end != '\0')
            return nullptr;

        return std::make_unique<float>(result);
    }

    template<>
    inline std::unique_ptr<double> convert<double>(char *value)
    {
        if (!value)
            return nullptr;

        char *end = nullptr;
        double result = std::strtod(value, &end);

        if (value == end || *end != '\0')
            return nullptr;

        return std::make_unique<double>(result);
    }

    template<>
    inline std::unique_ptr<bool> convert<bool>(char *value)
    {
        if (!value)
            return nullptr;

        if (std::strcmp(value, "true") == 0)
            return std::make_unique<bool>(true);

        if (std::strcmp(value, "false") == 0)
            return std::make_unique<bool>(false);

        return nullptr;
    }

    class Table
    {
    private:
        table *table_;

        ::section *resolve(
            std::initializer_list<char *> path) const
        {
            if (!table_)
                return nullptr;

            if (path.size() == 0)
                return const_cast<::section *>(oml_section(table_, nullptr));

            auto it = path.begin();
            ::section *sec = const_cast<::section *>(oml_section(table_, *it));
            ++it;

            for (; it != path.end() && sec; ++it)
                sec = const_cast<::section *>(oml_nested(table_, sec, *it));

            return sec;
        }

    public:
        explicit Table(const char *filename)
            : table_(oml_get(filename))
        {
        }

        ~Table()
        {
            oml_free(table_);
        }

        Table(const Table &) = delete;
        Table &operator=(const Table &) = delete;

        Table(Table &&other) noexcept
            : table_(other.table_)
        {
            other.table_ = nullptr;
        }

        Table &operator=(Table &&other) noexcept
        {
            if (this != &other)
            {
                oml_free(table_);
                table_ = other.table_;
                other.table_ = nullptr;
            }
            return *this;
        }

        const ::section *section(char *section_name = nullptr) const
        {
            if (!table_)
                return nullptr;

            return oml_section(table_, section_name);
        }

        const ::section *nested(::section *parent, char *name) const
        {
            if (!table_ || !parent || !name)
                return nullptr;

            return oml_nested(table_, parent, name);
        }

        template<typename T = std::string>
        std::unique_ptr<T> get(
            char *section_name,
            char *key
        ) const
        {
            if (!table_)
                return nullptr;

            ::section *sec = const_cast<::section *>(oml_section(table_, section_name));
            return get<T>(sec, key);
        }

        template<typename T = std::string>
        std::unique_ptr<T> get(
            ::section *sec,
            char *key
        ) const
        {
            if (!table_ || !sec || !key)
                return nullptr;

            if constexpr (std::is_same_v<T, std::string>)
            {
                const entry *e = oml_value(sec, key);

                if (!e || !e->value)
                    return nullptr;

                return std::make_unique<std::string>(e->value);
            }
            else if constexpr (std::is_same_v<T, std::vector<std::string>>)
            {
                const entry *e = oml_value(sec, key);

                if (!e || !e->array)
                    return nullptr;

                auto result = std::make_unique<std::vector<std::string>>();

                for (std::size_t i = 0; e->array[i]; ++i)
                    result->emplace_back(e->array[i]);

                return result;
            }
            else
            {
                auto value = get<std::string>(sec, key);

                if (!value)
                    return nullptr;

                return convert<T>(const_cast<char *>(value->c_str()));
            }
        }

        template<typename T = std::string>
        std::unique_ptr<T> get(
            std::initializer_list<char *> path,
            char *key
        ) const
        {
            ::section *sec = resolve(path);
            return get<T>(sec, key);
        }
    };
}