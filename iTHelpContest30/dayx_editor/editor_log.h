#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <utility>

enum class LogLevel
{
    Info,
    Warning,
    Error
};

struct LogEntry
{
    LogLevel level = LogLevel::Info;
    std::string text;
};

/// 編輯器的輸出紀錄，顯示在 Output Log 分頁。只保留最近的 1000 行。
class EditorLog
{
  public:
    void Info(std::string text)
    {
        Add(LogLevel::Info, std::move(text));
    }
    void Warning(std::string text)
    {
        Add(LogLevel::Warning, std::move(text));
    }
    void Error(std::string text)
    {
        Add(LogLevel::Error, std::move(text));
    }
    void Clear()
    {
        m_entries.clear();
        ++m_version;
    }

    [[nodiscard]] const std::deque<LogEntry>& Entries() const noexcept
    {
        return m_entries;
    }
    /// 每次內容變動就加一，面板用它判斷是否要捲到最底
    [[nodiscard]] std::uint64_t Version() const noexcept
    {
        return m_version;
    }

  private:
    static constexpr std::size_t kMaxEntries = 1000;

    void Add(LogLevel level, std::string text)
    {
        m_entries.push_back({level, std::move(text)});
        if (m_entries.size() > kMaxEntries)
            m_entries.pop_front();
        ++m_version;
    }

    std::deque<LogEntry> m_entries;
    std::uint64_t m_version = 0;
};
