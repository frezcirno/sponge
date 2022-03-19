#pragma once

#include <memory>
#include <string>

class StringBuffer {
  private:
    std::shared_ptr<std::string> _storage{};
    std::string_view _view{};

    StringBuffer(const std::shared_ptr<std::string> &storage, const std::string_view &view)
        : _storage{storage}, _view{view} {}

  public:
    StringBuffer() = default;

    StringBuffer(std::string &&str)
        : _storage{std::make_shared<std::string>(std::move(str))}, _view{_storage->c_str(), _storage->size()} {}

    size_t size() const { return _view.size(); }

    std::string copy() const { return std::string(_view); }

    const std::string_view &view() const { return _view; }

    void remove_prefix(size_t n) { _view.remove_prefix(n); }

    void remove_suffix(size_t n) { _view.remove_suffix(n); }

    StringBuffer substr(size_t begin, size_t n) const { return {_storage, _view.substr(begin, n)}; }
};
