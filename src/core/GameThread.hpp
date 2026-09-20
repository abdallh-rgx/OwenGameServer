#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

// ============================================================
// GameThread scheduler
// ------------------------------------------------------------
// Unreal Engine is NOT thread-safe: ProcessEvent, UFunction calls,
// RPCs (Client* functions), world travel and any UObject mutation
// must happen on the game thread. Calling them from a background
// std::thread corrupts engine state and crashes the game later,
// deep inside libUnreal.so on the GameThread (raised SIGSEGV via
// tgkill -> "signal 11, code SI_TKILL" tombstone with NO
// gameserver frames in the backtrace).
//
// Usage:
//   Sarah::RunOnGameThread([](){ ...engine calls...; });
//
// The queue is drained ONLY from Dobby hook bodies that the engine
// itself invokes on the game thread (Misc::TickFlush, GetNetMode,
// Player::ClientOnPawnDied) - see their call sites for
// Sarah::DrainGameThreadQueue().
// ============================================================

namespace Sarah {

inline std::mutex g_GameThreadQueueMutex;
inline std::vector<std::function<void()>> g_GameThreadQueue;

/* Cheap "has work" flag so the per-frame pump costs one relaxed
 * atomic load when the queue is empty. */
inline std::atomic<bool> g_GameThreadHasWork{false};

/* Schedule a task for execution on the game thread. Thread-safe. */
inline void RunOnGameThread(std::function<void()> fn)
{
    if (!fn)
        return;

    {
        std::lock_guard<std::mutex> Lock(g_GameThreadQueueMutex);
        g_GameThreadQueue.push_back(std::move(fn));
    }
    g_GameThreadHasWork.store(true, std::memory_order_release);
}

/* MUST be called only from the game thread (from inside one of the
 * installed hooks). Executes every queued task, oldest first.
 * Exceptions from tasks are swallowed - a broken task must never
 * propagate into engine code. */
inline void DrainGameThreadQueue()
{
    if (!g_GameThreadHasWork.load(std::memory_order_acquire))
        return;

    std::vector<std::function<void()>> Local;
    {
        std::lock_guard<std::mutex> Lock(g_GameThreadQueueMutex);
        Local.swap(g_GameThreadQueue);
        g_GameThreadHasWork.store(false, std::memory_order_release);
    }

    for (auto& Fn : Local)
    {
        if (!Fn)
            continue;
        try
        {
            Fn();
        }
        catch (...)
        {
            // Never let a task exception escape into engine code.
        }
    }
}

} // namespace Sarah
