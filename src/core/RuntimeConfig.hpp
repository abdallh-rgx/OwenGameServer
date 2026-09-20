#pragma once

// ============================================================
// OwenGameServer runtime configuration
// ------------------------------------------------------------
// The recurring GameThread crash needs to be BISECTED on the
// user's device. This config file allows disabling every
// suspicious subsystem independently, without rebuilding.
//
// The file is read once at startup from (first existing dir):
//   /storage/emulated/0/Android/data/com.epicgames.fortnite/files/OwenGameServer.cfg
//   /sdcard/Android/data/com.epicgames.fortnite/files/OwenGameServer.cfg
//   /data/data/com.epicgames.fortnite/files/OwenGameServer.cfg
//   /data/local/tmp/OwenGameServer.cfg
//
// Format: one "key=value" per line, '#' starts a comment.
// value: 1/true/yes/on enables. Missing file => all defaults.
//
// DEFAULTS keep the exact current behaviour (crash still
// reproduces, but now with full forensics).
// ============================================================

struct OwenConfig {
    // Pure observation control: no flag flips, no hooks, no travel.
    // If it STILL crashes in this mode, the cause is outside this lib.
    bool safe_mode = false;

    // Do not flip GIsEditor/GIsClient at startup.
    bool no_setclientoffonly = false;

    // Flip GIsEditor/GIsClient only right before map travel, letting the
    // frontend finish loading as an honest client first.
    bool late_flip = false;

    // Do not install the GetNetMode Dobby hook.
    bool no_getnetmode_hook = false;

    // GetNetMode hook installed, but returns the ORIGINAL (true) value
    // instead of always NM_DedicatedServer. The frontend then keeps its
    // real client netmode; after travel GIsClient=0 makes the true value
    // NM_DedicatedServer anyway.
    bool honest_netmode = false;

    // Do not install the TickFlush Dobby hook.
    bool no_tickflush_hook = false;

    // Do not install ANY Dobby native hooks.
    bool no_native_hooks = false;

    // Do not install any ExecFunction (UFunction::ExecFunction) hooks.
    bool no_exec_hooks = false;

    // Do not request the `open <map>` world travel.
    bool no_map_travel = false;

    // Do not hook raise()/liblog (forensics signal handlers stay active).
    bool no_fatal_api_hooks = false;
};

extern OwenConfig OwenCfg;

// Reads the config file (if present). Returns true when a file was parsed.
bool LoadOwenConfig(const char* logDirOrNull);
