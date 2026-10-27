#ifndef CONTROLTABLEHOST_H
#define CONTROLTABLEHOST_H

#include "databus.h"
#include "rcqapi.h"

#include <mutex>
#include <unordered_map>
#include <vector>

/// Host-owned merge of per-tracker control exports into the bus `control_table` slot.
class ControlTableHost {
public:
    static constexpr const char* kHostOwnerPath = "__host__";

    explicit ControlTableHost(IDataBus* bus) : bus_(bus) {}

    std::unordered_map<std::string, std::shared_ptr<void>>* table()
    {
        if (!bus_)
            return nullptr;
        std::lock_guard<std::mutex> busLock(PluginBusLocks::streamBusMutex());
        auto& slot = bus_->getData("control_table");
        return std::any_cast<std::unordered_map<std::string, std::shared_ptr<void>>>(&slot);
    }

    const std::unordered_map<std::string, std::string>& keySources() const { return keySources_; }

    /// Merge entries from a tracker-local map; records key ownership.
    void merge(const std::string& trackerPath,
               const std::string& trackerDisplayName,
               const std::unordered_map<std::string, std::shared_ptr<void>>& entries,
               std::vector<std::string>& outAdded)
    {
        if (trackerPath.empty() || !bus_)
            return;

        const std::string sourceLabel = trackerDisplayName.empty()
            ? pluginMetaFallbackName(trackerPath) : trackerDisplayName;

        std::lock_guard<std::mutex> hostLock(mu_);
        std::lock_guard<std::mutex> busLock(PluginBusLocks::streamBusMutex());
        auto& slot = bus_->getData("control_table");
        auto* ct = std::any_cast<std::unordered_map<std::string, std::shared_ptr<void>>>(&slot);
        if (!ct)
            return;

        for (const auto& [key, value] : entries) {
            if (key.empty() || !value)
                continue;
            if (keyOwners_.count(key) && keyOwners_.at(key) == kHostOwnerPath)
                continue;
            const bool isNew = ct->find(key) == ct->end();
            (*ct)[key] = value;
            keyOwners_[key] = trackerPath;
            keySources_[key] = sourceLabel;
            if (isNew)
                outAdded.push_back(key);
        }
    }

    /// Merge application-owned controllers (survive tracker rebuild / clearAll).
    void mergeHost(const std::string& hostPath,
                   const std::unordered_map<std::string, std::shared_ptr<void>>& entries,
                   std::vector<std::string>& outAdded)
    {
        if (hostPath.empty() || !bus_)
            return;

        std::lock_guard<std::mutex> hostLock(mu_);
        std::lock_guard<std::mutex> busLock(PluginBusLocks::streamBusMutex());
        auto& slot = bus_->getData("control_table");
        auto* ct = std::any_cast<std::unordered_map<std::string, std::shared_ptr<void>>>(&slot);
        if (!ct)
            return;

        for (const auto& [key, value] : entries) {
            if (key.empty() || !value)
                continue;
            hostEntries_[key] = value;
            const bool isNew = ct->find(key) == ct->end();
            (*ct)[key] = value;
            keyOwners_[key] = hostPath;
            keySources_[key] = "Application";
            if (isNew)
                outAdded.push_back(key);
        }
    }

    void mergeTracker(const std::string& trackerPath, const std::string& trackerDisplayName,
                      ITracker* tracker, std::vector<std::string>& outAdded)
    {
        if (!tracker)
            return;
        auto* local = tracker->getTable();
        if (!local)
            return;
        merge(trackerPath, trackerDisplayName, *local, outAdded);
    }

    void removeTracker(const std::string& trackerPath, std::vector<std::string>& outRemoved)
    {
        if (trackerPath.empty() || !bus_)
            return;

        std::lock_guard<std::mutex> hostLock(mu_);
        std::lock_guard<std::mutex> busLock(PluginBusLocks::streamBusMutex());
        auto& slot = bus_->getData("control_table");
        auto* ct = std::any_cast<std::unordered_map<std::string, std::shared_ptr<void>>>(&slot);
        if (!ct)
            return;

        for (auto it = keyOwners_.begin(); it != keyOwners_.end(); ) {
            if (it->second == trackerPath) {
                outRemoved.push_back(it->first);
                keySources_.erase(it->first);
                ct->erase(it->first);
                it = keyOwners_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void clearAll(std::vector<std::string>& outRemoved)
    {
        if (!bus_)
            return;

        std::lock_guard<std::mutex> hostLock(mu_);
        std::lock_guard<std::mutex> busLock(PluginBusLocks::streamBusMutex());
        auto& slot = bus_->getData("control_table");
        auto* ct = std::any_cast<std::unordered_map<std::string, std::shared_ptr<void>>>(&slot);
        if (!ct)
            return;

        for (const auto& [key, _] : *ct) {
            if (keyOwners_.count(key) && keyOwners_[key] == kHostOwnerPath)
                continue;
            outRemoved.push_back(key);
        }

        const auto savedHost = hostEntries_;
        const auto savedHostSources = keySources_;
        ct->clear();
        keyOwners_.clear();
        keySources_.clear();

        for (const auto& [key, value] : savedHost) {
            if (key.empty() || !value)
                continue;
            (*ct)[key] = value;
            keyOwners_[key] = kHostOwnerPath;
            if (auto sit = savedHostSources.find(key); sit != savedHostSources.end())
                keySources_[key] = sit->second;
            else
                keySources_[key] = "Application";
        }
    }

private:
    static std::string pluginMetaFallbackName(const std::string& dllPath)
    {
        const auto slash = dllPath.find_last_of("/\\");
        const std::string base = (slash == std::string::npos) ? dllPath : dllPath.substr(slash + 1);
        const auto dot = base.find_last_of('.');
        return (dot == std::string::npos) ? base : base.substr(0, dot);
    }

    IDataBus* bus_ = nullptr;
    std::mutex mu_;
    std::unordered_map<std::string, std::string> keyOwners_;
    std::unordered_map<std::string, std::string> keySources_;
    std::unordered_map<std::string, std::shared_ptr<void>> hostEntries_;
};

#endif // CONTROLTABLEHOST_H
