#include "LiveLink.h"
#include "shared/updates/UpdateChecker.h"

#if JUCE_WINDOWS
 #include <winsock2.h>
 #include <ws2tcpip.h>
#else
 #include <netdb.h>
 #include <sys/socket.h>
#endif

namespace LiveLink
{
    std::atomic<bool> networkAllowed { true };

    namespace
    {
        bool isPrivate (const juce::IPAddress& a)
        {
            const auto b0 = a.address[0], b1 = a.address[1];
            return b0 == 10 || (b0 == 192 && b1 == 168) || (b0 == 172 && b1 >= 16 && b1 <= 31);
        }

        bool isLinkLocal (const juce::IPAddress& a)
        {
            return a.address[0] == 169 && a.address[1] == 254;
        }

        bool isLoopback (const juce::IPAddress& a)
        {
            return a.address[0] == 127;
        }

        // The system's own resolver - no third party is asked anything.
        bool resolves (const char* name)
        {
           #if JUCE_WINDOWS
            static const bool started = []
            {
                WSADATA data;
                return WSAStartup (MAKEWORD (2, 2), &data) == 0;
            }();
            juce::ignoreUnused (started);
           #endif

            addrinfo hints {};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            addrinfo* found = nullptr;

            const auto ok = getaddrinfo (name, "443", &hints, &found) == 0 && found != nullptr;

            if (found != nullptr)
                freeaddrinfo (found);

            return ok;
        }
    }

    LanInfo pickLanAddress (const juce::Array<juce::IPAddress>& addresses)
    {
        LanInfo info;
        juce::IPAddress fallback, linkLocal;

        for (const auto& a : addresses)
        {
            if (a.isIPv6 || a.isNull() || isLoopback (a))
                continue;

            if (isPrivate (a))
            {
                info.address = a;
                return info;
            }

            if (isLinkLocal (a))
                linkLocal = a;
            else if (fallback.isNull())
                fallback = a;   // a public address straight on the machine - rare, but it works
        }

        if (! fallback.isNull())
        {
            info.address = fallback;
            return info;
        }

        info.address = linkLocal;
        info.linkLocal = ! linkLocal.isNull();
        return info;
    }

    LanInfo scanLan()
    {
        return pickLanAddress (juce::IPAddress::getAllAddresses (false));
    }

    State readHealth (const juce::String& body, const juce::String& currentVersion)
    {
        const auto json = juce::JSON::parse (body);

        if (! json.isObject() || ! (bool) json.getProperty ("ok", false))
            return State::serverError;

        const auto minimum = json.getProperty ("minApp", "").toString();

        if (minimum.isNotEmpty() && UpdateChecker::isNewerVersion (minimum, currentVersion))
            return State::appTooOld;

        return State::online;
    }

    Result check (const juce::String& currentVersion)
    {
        Result result;
        result.lan = scanLan();

        if (! networkAllowed.load())
        {
            result.state = State::unknown;
            return result;
        }

        if (! result.lan.any())
        {
            result.state = State::noNetwork;
            return result;
        }

        if (! resolves (host))
        {
            result.state = State::noInternet;
            return result;
        }

        auto status = 0;
        auto stream = juce::URL (healthUrl).createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs (5000)
                .withStatusCode (&status));

        result.httpStatus = status;

        if (stream == nullptr || status == 0)
        {
            result.state = State::serverDown;
            return result;
        }

        if (status != 200)
        {
            result.state = State::serverError;
            return result;
        }

        result.state = readHealth (stream->readEntireStreamAsString(), currentVersion);
        return result;
    }
}
