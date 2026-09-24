#include <juce_core/juce_core.h>
#include "../Source/LiveLink.h"
#include "../Source/LiveAccount.h"

// The parts of the Live connection check that do not need a network: which
// address a local room shows the phones, and what the server's health
// answer means for this build.
class LiveLinkTest : public juce::UnitTest
{
public:
    LiveLinkTest() : juce::UnitTest ("LiveLink", "Live") {}

    void runTest() override
    {
        using LiveLink::State;

        beginTest ("a local room shows a private address, never loopback, 169.254 only as a flagged last resort");
        {
            const auto pick = [] (std::initializer_list<const char*> list)
            {
                juce::Array<juce::IPAddress> addresses;
                for (const auto* a : list)
                    addresses.add (juce::IPAddress (a));
                return LiveLink::pickLanAddress (addresses);
            };

            auto lan = pick ({ "127.0.0.1", "169.254.3.9", "192.168.1.24" });
            expectEquals (lan.address.toString(), juce::String ("192.168.1.24"));
            expect (! lan.linkLocal);

            lan = pick ({ "10.0.0.5" });
            expectEquals (lan.address.toString(), juce::String ("10.0.0.5"));

            lan = pick ({ "172.20.1.2", "85.1.2.3" });
            expectEquals (lan.address.toString(), juce::String ("172.20.1.2"));

            lan = pick ({ "127.0.0.1", "169.254.3.9" });
            expect (lan.any() && lan.linkLocal, "169.254 is all there is: offered, and flagged");

            lan = pick ({ "127.0.0.1" });
            expect (! lan.any(), "loopback alone is no network");
        }

        beginTest ("the health answer: ok, too old for the server, or not an answer at all");
        {
            expect (LiveLink::readHealth (R"({"ok":true,"minApp":"1.0.0"})", "1.6.0") == State::online);
            expect (LiveLink::readHealth (R"({"ok":true})", "1.6.0") == State::online);
            expect (LiveLink::readHealth (R"({"ok":true,"minApp":"2.0.0"})", "1.6.0") == State::appTooOld);
            expect (LiveLink::readHealth (R"({"ok":false})", "1.6.0") == State::serverError);
            expect (LiveLink::readHealth ("<html>502 Bad Gateway</html>", "1.6.0") == State::serverError);
        }
        beginTest ("the version the app sends when linking is one the server accepts");
        {
            // The server's rule (site: abctrainAccounts.js APP_VERSION_RE).
            const auto accepted = [] (const juce::String& v)
            {
                const auto core = v.initialSectionContainingOnly ("0123456789.");
                const auto parts = juce::StringArray::fromTokens (core, ".", "");
                const auto suffix = v.substring (core.length());
                return parts.size() == 3 && ! core.endsWith (".")
                    && (suffix.isEmpty() || ((suffix[0] == '-' || suffix[0] == '+') && suffix.length() <= 21
                                             && suffix.substring (1).containsOnly ("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-")));
            };

            expectEquals (LiveAccount::appVersion ("v2.0.0"), juce::String ("2.0.0"));
            expectEquals (LiveAccount::appVersion ("v1.8.0-beta.1"), juce::String ("1.8.0-beta.1"));
            expectEquals (LiveAccount::appVersion ("0.0.0-dev+sha2178654"), juce::String ("0.0.0-dev.sha2178654"));

            for (const auto* v : { "v2.0.0", "v1.8.0-beta.1-8-g2178654-dirty", "2.1.3", "v2.0.0-12-gdeadbee", "garbage" })
                expect (accepted (LiveAccount::appVersion (v)), juce::String (v) + " -> " + LiveAccount::appVersion (v));

            expect (LiveAccount::platform().length() <= 20);
        }
    }
};

static LiveLinkTest liveLinkTest;
