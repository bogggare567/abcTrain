#include <juce_core/juce_core.h>
#include "../shared/VersionChannel.h"

class VersionChannelTest : public juce::UnitTest
{
public:
    VersionChannelTest() : juce::UnitTest ("VersionChannel", "Shared") {}

    void runTest() override
    {
        beginTest ("an exact vX.Y.Z tag is the stable channel");
        {
            expect (VersionChannel::detect ("v1.0.0") == VersionChannel::Channel::stable);
            expect (VersionChannel::detect ("1.0.0") == VersionChannel::Channel::stable);
            expect (VersionChannel::detect ("v0.1.0") == VersionChannel::Channel::stable);
        }

        beginTest ("a -beta suffix is the beta channel, regardless of case");
        {
            expect (VersionChannel::detect ("v1.0.0-beta1") == VersionChannel::Channel::beta);
            expect (VersionChannel::detect ("v1.0.0-BETA2") == VersionChannel::Channel::beta);
            expect (VersionChannel::detect ("v2.0.0-beta") == VersionChannel::Channel::beta);
        }

        beginTest ("N-commits-past-a-tag, a dirty tree, or no reachable tag are all the dev channel");
        {
            expect (VersionChannel::detect ("v1.0.0-5-g1a2b3c4") == VersionChannel::Channel::dev);
            expect (VersionChannel::detect ("v1.0.0-dirty") == VersionChannel::Channel::dev);
            expect (VersionChannel::detect ("v1.0.0-5-g1a2b3c4-dirty") == VersionChannel::Channel::dev);
            expect (VersionChannel::detect ("0.0.0-dev+sha1234567") == VersionChannel::Channel::dev);
        }

        beginTest ("malformed input is the dev channel, not a crash or a guess");
        {
            expect (VersionChannel::detect ("") == VersionChannel::Channel::dev);
            expect (VersionChannel::detect ("not-a-version") == VersionChannel::Channel::dev);
            expect (VersionChannel::detect ("v1.0") == VersionChannel::Channel::dev);
        }

        beginTest ("toString round-trips each channel to a lowercase name");
        {
            expectEquals (VersionChannel::toString (VersionChannel::Channel::stable), juce::String ("stable"));
            expectEquals (VersionChannel::toString (VersionChannel::Channel::beta), juce::String ("beta"));
            expectEquals (VersionChannel::toString (VersionChannel::Channel::dev), juce::String ("dev"));
        }
    }
};

static VersionChannelTest versionChannelTest;
