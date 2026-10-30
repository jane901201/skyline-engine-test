#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

template<class TResource>
class TResourceBank
{
public:
    using ResourcePtr = std::unique_ptr<TResource>;

    [[nodiscard]]
    TResource* get(std::string_view key)
    {
        validateKey(key);

        const auto it = m_resourceMap.find(key);
        return it != m_resourceMap.end()
            ? it->second.get()
            : nullptr;
    }

    [[nodiscard]]
    const TResource* get(std::string_view key) const
    {
        validateKey(key);

        const auto it = m_resourceMap.find(key);
        return it != m_resourceMap.end()
            ? it->second.get()
            : nullptr;
    }

    void registerResource(
        std::string key,
        ResourcePtr resource)
    {
        validateKey(key);

        if (!resource)
        {
            throw std::invalid_argument(
                "ResourceBank: A resource instance is required.");
        }

        const auto [it, inserted] = m_resourceMap.try_emplace(
            std::move(key),
            std::move(resource));

        if (!inserted)
        {
            throw std::logic_error(
                "ResourceBank: The resource key is already registered.");
        }
    }

    [[nodiscard]]
    bool contains(std::string_view key) const
    {
        if (key.empty())
            return false;

        return m_resourceMap.find(key) != m_resourceMap.end();
    }

    bool remove(std::string_view key)
    {
        if (key.empty())
            return false;

        return m_resourceMap.erase(std::string{key}) > 0;
    }

    void clear() noexcept
    {
        m_resourceMap.clear();
    }

    [[nodiscard]]
    std::size_t size() const noexcept
    {
        return m_resourceMap.size();
    }

private:
    static void validateKey(std::string_view key)
    {
        if (key.empty())
        {
            throw std::invalid_argument(
                "ResourceBank: A resource key is required.");
        }
    }

private:
    std::map<std::string, ResourcePtr, std::less<>> m_resourceMap;
};