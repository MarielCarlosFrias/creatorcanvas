#pragma once

#include "DocumentVersion.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace cc {

class DocumentVersionStore final
{
public:
    std::size_t size() const { return m_versions.size(); }
    void add(DocumentVersion version) { m_versions.push_back(std::move(version)); }
    const DocumentVersion& back() const { return m_versions.back(); }
    std::vector<DocumentVersion> all() const { return m_versions; }

    DocumentVersion* find(const QUuid& id)
    {
        const auto it = std::find_if(m_versions.begin(), m_versions.end(),
                                     [&id](const DocumentVersion& version) {
                                         return version.id == id;
                                     });
        return it == m_versions.end() ? nullptr : &*it;
    }

    bool remove(const QUuid& id)
    {
        const auto it = std::find_if(m_versions.begin(), m_versions.end(),
                                     [&id](const DocumentVersion& version) {
                                         return version.id == id;
                                     });
        if (it == m_versions.end())
            return false;
        m_versions.erase(it);
        return true;
    }

    void clear() { m_versions.clear(); }

private:
    std::vector<DocumentVersion> m_versions;
};

} // namespace cc
