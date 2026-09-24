#pragma once

#include <juce_core/juce_core.h>
#include <atomic>

// Is there a way to the Live server, and if not, why not - in the terms a
// person at a seminar can act on.
//
// "Something went wrong" helps nobody holding a laptop in a hall with forty
// people waiting. The three failures that actually happen have three
// different fixes: no network at all (turn on Wi-Fi); a network with no way
// out (a captive portal, a VPN, a venue that blocks things); and the
// internet fine but the server not answering (wait, or run the seminar
// locally). They can be told apart without asking any third party: whether
// this machine has an address; whether the system's own resolver can find
// soundkorb.ru; whether soundkorb.ru answers.
//
// Runs only when the player opens the Live tab (and on "Check again") - the
// offline rule in CLAUDE.md: the app talks to the Live server only when the
// player uses Live. Tools and tests switch it off (networkAllowed).
namespace LiveLink
{
    enum class State
    {
        unknown,        // not checked yet
        checking,
        online,         // the server answered and accepts this version
        noNetwork,      // no address other than loopback: Wi-Fi off, cable out
        noInternet,     // an address, but soundkorb.ru does not resolve
        serverDown,     // it resolves, nothing answers
        serverError,    // it answered with an error status
        appTooOld       // it answered, and says this version is no longer supported
    };

    struct LanInfo
    {
        juce::IPAddress address;     // best address for phones to reach, or null
        bool linkLocal = false;      // 169.254.x.x: no router handed one out
        bool any() const noexcept { return ! address.isNull(); }
    };

    // The address a phone on the same Wi-Fi would use: a private IPv4
    // (192.168/16, 10/8, 172.16/12) over anything else, and 169.254 only
    // as a last resort - flagged, because phones rarely reach it.
    LanInfo pickLanAddress (const juce::Array<juce::IPAddress>&);
    LanInfo scanLan();

    struct Result
    {
        State state = State::unknown;
        int httpStatus = 0;
        LanInfo lan;
    };

    // What the server's /api/abctrain/health says, given this build's
    // version: online, or appTooOld when it names a newer minimum.
    State readHealth (const juce::String& body, const juce::String& currentVersion);

    // Blocking - a worker thread only. Resolves and fetches with short
    // timeouts; the whole check takes at most a few seconds.
    Result check (const juce::String& currentVersion);

    constexpr const char* host = "soundkorb.ru";
    constexpr const char* healthUrl = "https://soundkorb.ru/api/abctrain/health";

    // False in tools/EditorSnapshots, ClickMap and the tests: a screenshot
    // must not depend on the network of whatever machine renders it.
    extern std::atomic<bool> networkAllowed;
}
