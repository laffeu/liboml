#pragma once

#include "liboml.h"

#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>

namespace loml
{
    template<typename T>
    std::unique_ptr<T> convert(const char *value);

    template<>
    inline std::unique_ptr<std::string> convert<std::string>(const char *value)
    {
        if (!value)
            return nullptr;

        return std::make_unique<std::string>(value);
    }

    template<>
    inline std::unique_ptr<int> convert<int>(const char *value)
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
    inline std::unique_ptr<float> convert<float>(const char *value)
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
    inline std::unique_ptr<double> convert<double>(const char *value)
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
    inline std::unique_ptr<bool> convert<bool>(const char *value)
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

        bool valid() const
        {
            return table_ != nullptr;
        }

        template<typename T = std::string>
        std::unique_ptr<T> get(
            const char *section_name,
            const char *key
        ) const
        {
            if (!table_)
                return nullptr;

            if constexpr (std::is_same_v<T, std::string>)
            {
                const section *sec = oml_bysec(table_, section_name);
                const entry *e = oml_bykey(sec, key);

                if (!e || !e->value)
                    return nullptr;

                return std::make_unique<std::string>(e->value);
            }
            else if constexpr (std::is_same_v<T, std::vector<std::string>>)
            {
                const section *sec = oml_bysec(table_, section_name);
                const entry *e = oml_bykey(sec, key);

                if (!e || !e->array)
                    return nullptr;

                auto result = std::make_unique<std::vector<std::string>>();

                for (std::size_t i = 0; e->array[i]; ++i)
                    result->emplace_back(e->array[i]);

                return result;
            }
            else
            {
                auto value = get<std::string>(section_name, key);

                if (!value)
                    return nullptr;

                return convert<T>(value->c_str());
            }
        }
    };
}