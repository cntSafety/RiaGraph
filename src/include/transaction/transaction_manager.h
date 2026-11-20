#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "common/constants.h"
#include "common/uniq_lock.h"
#include "storage/checkpointer.h"
#include "storage/wal/wal.h"
#include "transaction/transaction.h"

namespace riag {
namespace main {
class ClientContext;
} // namespace main

namespace testing {
class DBTest;
class FlakyBufferManager;
class FlakyCheckpointer;
} // namespace testing

namespace transaction {

class TransactionManager {
    friend class testing::DBTest;
    friend class testing::FlakyBufferManager;
    friend class testing::FlakyCheckpointer;

    using init_checkpointer_func_t =
        std::function<std::unique_ptr<storage::Checkpointer>(main::ClientContext&)>;
    static std::unique_ptr<storage::Checkpointer> initCheckpointer(
        main::ClientContext& clientContext);

public:
    // Timestamp starts from 1. 0 is reserved for the dummy system transaction.
    explicit TransactionManager(storage::WAL& wal)
        : wal{wal}, lastTransactionID{Transaction::START_TRANSACTION_ID}, lastTimestamp{1} {
        initCheckpointerFunc = initCheckpointer;
    }

    Transaction* beginTransaction(main::ClientContext& clientContext, TransactionType type);

    void commit(main::ClientContext& clientContext, Transaction* transaction);
    void rollback(main::ClientContext& clientContext, common::transaction_t transactionID);

    // Namespace DDL must observe every committed membership and prevent new writers
    // until its policy/definition changes are published. Read snapshots remain free.
    void claimNamespaceDDL(main::ClientContext& context, const Transaction& transaction);

    void checkpoint(main::ClientContext& clientContext);

    // Minimum start timestamp over all currently open transactions (the snapshot horizon
    // that checkpoint retention must preserve). Returns INVALID_TRANSACTION when no
    // transaction is open. Brief lock acquisition.
    common::transaction_t snapshotHorizon();
    // A conservative, nonblocking horizon for namespace membership reclamation.
    // It may lag registry changes; active writers keep it finite. Reading it
    // under storage locks must never acquire the public transaction mutex.
    common::transaction_t namespaceReclaimHorizon() const {
        return cachedHorizon.load(std::memory_order_acquire);
    }
    // Start timestamps of all currently open transactions. Brief lock acquisition.
    std::vector<common::transaction_t> snapshotOpenStartTSs();
    // Pin behind the snapshot horizon: the minimum open start timestamp, the transaction
    // holding it, and the open-transaction count (which transaction pins WAL growth).
    // INVALID_TRANSACTION/0 when no transaction is open. Brief lock acquisition.
    struct HorizonPinInfo {
        common::transaction_t horizon = common::INVALID_TRANSACTION;
        common::transaction_t pinningTxnID = common::INVALID_TRANSACTION;
        size_t openTxnCount = 0;
    };
    HorizonPinInfo snapshotHorizonInfo();

    static TransactionManager* Get(const main::ClientContext& context);

private:
    bool hasNoActiveTransactions() const;
    void checkpointNoLock(main::ClientContext& clientContext);
    // Try to checkpoint without blocking. Returns immediately if another checkpoint is in
    // progress. Used by auto-checkpoint after commit.
    void tryCheckpoint(main::ClientContext& clientContext);

    // This function locks the mutex to stop new transactions and waits until all transactions
    // (both read and write) leave the system. Used as a fallback.
    common::UniqLock stopNewTransactionsAndWaitUntilAllTransactionsLeave();

    // This function locks the mutex to stop new write transactions and waits until all active
    // write transactions leave the system. Read transactions are allowed to continue.
    common::UniqLock stopNewWriteTransactionsAndWaitUntilAllWriteTransactionsLeave();

    bool hasActiveWriteTransactionNoLock() const {
        return activeWriteTransactionCount.load(std::memory_order_acquire) > 0;
    }

    // Note: Used by DBTest::createDB only.
    void setCheckPointWaitTimeoutForTransactionsToLeaveInMicros(uint64_t waitTimeInMicros) {
        checkpointWaitTimeoutInMicros = waitTimeInMicros;
    }

    void clearTransactionNoLock(common::transaction_t transactionID);

    // An in-memory database never checkpoints, so the storage of a dropped table is reclaimed
    // here instead, once no open transaction can still see it. Call under
    // mtxForSerializingPublicFunctionCalls.
    void reclaimDroppedTablesIfIdleNoLock(main::ClientContext& clientContext);

    // Registry maintenance, all under mtxForSerializingPublicFunctionCalls. The registry
    // pins retention only for the lifetime of each transaction; it imposes no timeout,
    // deadline, or eviction — a long-open reader pins the horizon until it closes.
    void registerTransactionNoLock(common::transaction_t transactionID,
        common::transaction_t startTS);
    void recomputeHorizonNoLock();

private:
    storage::WAL& wal;
    std::vector<std::unique_ptr<Transaction>> activeTransactions;
    // Start timestamp of every open transaction, keyed by transaction ID. Checkpoint
    // retention keeps version data committed at or after the minimum (the horizon).
    std::unordered_map<common::transaction_t, common::transaction_t> openTxnStartTS;
    // Cached minimum over openTxnStartTS; INVALID_TRANSACTION when no transaction is open.
    // Open-transaction counts are small, so a full rescan on erase is trivial.
    std::atomic<common::transaction_t> cachedHorizon{common::INVALID_TRANSACTION};
    common::transaction_t lastTransactionID;
    // Atomic so checkpointNoLock() can snapshot it without taking
    // mtxForSerializingPublicFunctionCalls while it holds the write gate (see the lock order
    // below).
    std::atomic<common::transaction_t> lastTimestamp{1};
    uint64_t nextWALCommitSequenceToPublish = 1;
    std::condition_variable cvForPublishingCommit;
    std::condition_variable cvForCommittingWriteTransaction;
    // This mutex serializes begin/commit/rollback calls to protect activeTransactions.
    // Lock order: mtxForStartingNewTransactions (the write gate), then this mutex. Never wait
    // for the write gate while holding this mutex: a checkpoint holds the gate while it drains
    // active writers, and they leave through commit()/rollback(), which need this mutex.
    std::mutex mtxForSerializingPublicFunctionCalls;
    std::mutex mtxForStartingNewTransactions;
    // Prevents concurrent checkpoints. Separate from mtxForSerializingPublicFunctionCalls so
    // that active writers can commit/rollback while the checkpoint is draining them.
    std::mutex mtxForCheckpoint;
    // Atomic counter tracking active write/recovery transactions so the checkpoint drain loop
    // can poll without holding mtxForSerializingPublicFunctionCalls.
    std::atomic<uint32_t> activeWriteTransactionCount{0};
    std::atomic<uint32_t> committingWriteTransactionCount{0};
    common::transaction_t namespaceDDLTransactionID = 0;
    // An in-memory transaction dropped a table whose storage is not reclaimed yet.
    std::atomic<bool> hasUnreclaimedDroppedTables{false};
    uint64_t checkpointWaitTimeoutInMicros = common::DEFAULT_CHECKPOINT_WAIT_TIMEOUT_IN_MICROS;

    init_checkpointer_func_t initCheckpointerFunc;
};
} // namespace transaction
} // namespace riag
