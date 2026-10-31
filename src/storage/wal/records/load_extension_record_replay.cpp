#include "extension/extension_manager.h"
#include "storage/wal/wal_replayer.h"

using namespace riag::storage;

namespace riag {
namespace storage {

void WALReplayer::replayLoadExtensionRecord(const WALRecord& walRecord) const {
    const auto& loadExtensionRecord = walRecord.constCast<LoadExtensionRecord>();
    extension::ExtensionManager::Get(clientContext)
        ->loadExtension(loadExtensionRecord.path, &clientContext);
}

} // namespace storage
} // namespace riag
