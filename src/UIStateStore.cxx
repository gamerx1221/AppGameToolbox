#include <AppGameToolbox/UIStateStore.hpp>

#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

namespace appgametoolbox {

bool UIStateStore::set(std::string key, UIStateValue value) {
    const auto existing = m_values.find(key);
    if (existing != m_values.end() && existing->second == value) return false;
    if (std::find(m_dirtyKeys.begin(), m_dirtyKeys.end(), key) == m_dirtyKeys.end()) m_dirtyKeys.push_back(key);
    auto inserted = m_values.insert_or_assign(std::move(key), std::move(value));
    for (const auto& subscriber : m_subscribers) subscriber.second(inserted.first->first, inserted.first->second);
    return true;
}

bool UIStateStore::set(std::string key, bool value) { return set(std::move(key), UIStateValue(value)); }
bool UIStateStore::set(std::string key, std::int64_t value) { return set(std::move(key), UIStateValue(value)); }
bool UIStateStore::set(std::string key, double value) { return set(std::move(key), UIStateValue(value)); }
bool UIStateStore::set(std::string key, std::string value) { return set(std::move(key), UIStateValue(std::move(value))); }

bool UIStateStore::setCollection(std::string key, UIStateCollection value) {
    const auto existing = m_collections.find(key);
    if (existing != m_collections.end() && existing->second == value) return false;
    if (std::find(m_dirtyKeys.begin(), m_dirtyKeys.end(), key) == m_dirtyKeys.end()) m_dirtyKeys.push_back(key);
    auto inserted = m_collections.insert_or_assign(std::move(key), std::move(value));
    for (const auto& subscriber : m_collectionSubscribers) subscriber.second(inserted.first->first, inserted.first->second);
    return true;
}

const UIStateValue* UIStateStore::value(const std::string& key) const {
    const auto found = m_values.find(key);
    return found == m_values.end() ? nullptr : &found->second;
}

const UIStateCollection* UIStateStore::collection(const std::string& key) const {
    const auto found = m_collections.find(key);
    return found == m_collections.end() ? nullptr : &found->second;
}

std::vector<std::string> UIStateStore::consumeDirtyKeys() {
    std::vector<std::string> result = std::move(m_dirtyKeys);
    m_dirtyKeys.clear();
    return result;
}

void UIStateStore::clearDirty() { m_dirtyKeys.clear(); }

UIStateStore::SubscriptionId UIStateStore::subscribe(ChangeCallback callback) {
    const SubscriptionId subscription = m_nextSubscription++;
    m_subscribers.emplace(subscription, std::move(callback));
    return subscription;
}

UIStateStore::SubscriptionId UIStateStore::subscribeCollections(CollectionChangeCallback callback) {
    const SubscriptionId subscription = m_nextSubscription++;
    m_collectionSubscribers.emplace(subscription, std::move(callback));
    return subscription;
}

void UIStateStore::unsubscribe(SubscriptionId subscription) {
    m_subscribers.erase(subscription);
    m_collectionSubscribers.erase(subscription);
}

std::string UIStateStore::toString(const UIStateValue& value) {
    if (const auto* boolean = std::get_if<bool>(&value)) return *boolean ? "true" : "false";
    if (const auto* integer = std::get_if<std::int64_t>(&value)) return std::to_string(*integer);
    if (const auto* number = std::get_if<double>(&value)) {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::setprecision(std::numeric_limits<double>::max_digits10) << *number;
        return stream.str();
    }
    return std::get<std::string>(value);
}

} // namespace appgametoolbox
