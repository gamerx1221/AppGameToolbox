#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace appgametoolbox {

using UIStateValue = std::variant<bool, std::int64_t, double, std::string>;
using UIStateItem = std::unordered_map<std::string, UIStateValue>;
using UIStateCollection = std::vector<UIStateItem>;

// Retained, platform-independent UI state. Notifications are synchronous and
// only emitted when a key's typed value actually changes.
class UIStateStore {
public:
    using ChangeCallback = std::function<void(const std::string&, const UIStateValue&)>;
    using CollectionChangeCallback = std::function<void(const std::string&, const UIStateCollection&)>;
    using SubscriptionId = std::uint64_t;

    bool set(std::string key, UIStateValue value);
    bool set(std::string key, bool value);
    bool set(std::string key, std::int64_t value);
    bool set(std::string key, double value);
    bool set(std::string key, std::string value);
    // Collection values are copied on set so hosts retain ownership of their data.
    bool setCollection(std::string key, UIStateCollection value);

    const UIStateValue* value(const std::string& key) const;
    const UIStateCollection* collection(const std::string& key) const;
    const std::unordered_map<std::string, UIStateValue>& values() const { return m_values; }
    bool isDirty() const { return !m_dirtyKeys.empty(); }
    const std::vector<std::string>& dirtyKeys() const { return m_dirtyKeys; }
    std::vector<std::string> consumeDirtyKeys();
    void clearDirty();

    SubscriptionId subscribe(ChangeCallback callback);
    SubscriptionId subscribeCollections(CollectionChangeCallback callback);
    void unsubscribe(SubscriptionId subscription);

    static std::string toString(const UIStateValue& value);

private:
    std::unordered_map<std::string, UIStateValue> m_values;
    std::unordered_map<std::string, UIStateCollection> m_collections;
    std::unordered_map<SubscriptionId, ChangeCallback> m_subscribers;
    std::unordered_map<SubscriptionId, CollectionChangeCallback> m_collectionSubscribers;
    std::vector<std::string> m_dirtyKeys;
    SubscriptionId m_nextSubscription = 1;
};

} // namespace appgametoolbox
