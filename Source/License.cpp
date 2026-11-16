#include "License.h"
#include "LicensePublicKey.h"

// Udvikling: den lokale test-server (LicenseTool serve) og den lokale hjemmeside (website/serve.ps1). Naar pluginnet
// skal saelges, saettes de til hjemmesidens rigtige adresser - altid https (CMakeLists: DS_LICENSE_URL / DS_SHOP_URL).
#ifndef DS_LICENSE_URL
 #define DS_LICENSE_URL "http://127.0.0.1:8766"
#endif
#ifndef DS_SHOP_URL
 #define DS_SHOP_URL "http://127.0.0.1:8765/#shop"
#endif

PitchBendUnlock::PitchBendUnlock (juce::File f, juce::String url, juce::String key)
    : stateFile (std::move (f)), serverURL (url.trimCharactersAtEnd ("/")), publicKey (std::move (key))
{
}

juce::File PitchBendUnlock::defaultStateFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    dir = dir.getChildFile ("Application Support");
   #endif
    return dir.getChildFile ("SoundSo").getChildFile ("Pitch Bend Texture").getChildFile ("licence.dat");
}

juce::String PitchBendUnlock::defaultServerURL() { return DS_LICENSE_URL; }
juce::String PitchBendUnlock::defaultPublicKey() { return DS_LICENSE_PUBLIC_KEY; }
juce::String PitchBendUnlock::shopURL()          { return DS_SHOP_URL; }

void PitchBendUnlock::saveState (const juce::String& s)
{
    stateFile.getParentDirectory().createDirectory();
    if (s.isEmpty()) stateFile.deleteFile();
    else             stateFile.replaceWithText (s);
    lastModified = stateFile.getLastModificationTime();
}

juce::String PitchBendUnlock::getState()
{
    lastModified = stateFile.getLastModificationTime();
    return stateFile.existsAsFile() ? stateFile.loadFileAsString().trim() : juce::String();
}

juce::String PitchBendUnlock::postForm (const juce::String& url, const juce::StringPairArray& fields, int timeoutMs)
{
    juce::URL u (url);
    for (auto& k : fields.getAllKeys())
        u = u.withParameter (k, fields[k]);

    if (auto in = u.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                           .withConnectionTimeoutMs (timeoutMs)))
        return in->readEntireStreamAsString();
    return {};
}

juce::String PitchBendUnlock::readReplyFromWebserver (const juce::String& email, const juce::String& password)
{
    juce::StringPairArray f;
    f.set ("product", productID);
    f.set ("os", juce::SystemStats::getOperatingSystemName());
    f.set ("mach", getLocalMachineIDs().joinIntoString (","));
    if (email.trim().isEmpty())
        f.set ("key", password.trim().toUpperCase());          // licensnoegle
    else
    {
        f.set ("email", email.trim());
        f.set ("pw", password);
    }
    return postForm (getServerAuthenticationURL().toString (false), f);
}

// Som JUCE's OnlineUnlockStatus::handleXmlReply (privat), men paa beskedtraaden, og noeglefilen gemmes med det samme
PitchBendUnlock::UnlockResult PitchBendUnlock::applyServerReply (const juce::String& reply)
{
    UnlockResult r;
    r.succeeded = false;
    auto xml = juce::parseXML (reply);
    if (xml == nullptr)
    {
        r.errorMessage = reply.isEmpty() ? "Couldn't reach the SoundSo licence server. Check your internet connection and try again."
                                         : getMessageForUnexpectedReply();
        return r;
    }
    if (auto* keyNode = xml->getChildByName ("KEY"))
    {
        const auto keyText = keyNode->getAllSubText().trim();
        if (keyText.length() > 10)
        {
            const auto res = applyKeyFile (keyText);
            if (res.failed())
            {
                r.errorMessage = "The licence from the server could not be used on this computer (" + res.getErrorMessage() + ")";
                return r;
            }
            save();
            r.succeeded = (bool) isUnlocked();
        }
    }
    if (xml->hasTagName ("MESSAGE")) r.informativeMessage = xml->getStringAttribute ("message").trim();
    if (xml->hasTagName ("ERROR"))   r.errorMessage       = xml->getStringAttribute ("error").trim();
    r.urlToLaunch = xml->getStringAttribute ("url").trim();
    if (r.errorMessage.isEmpty() && r.informativeMessage.isEmpty() && ! r.succeeded)
        r.errorMessage = getMessageForUnexpectedReply();
    return r;
}

juce::String PitchBendUnlock::currentKeyFile()
{
    juce::MemoryBlock mb;
    mb.fromBase64Encoding (getState());
    if (mb.isEmpty()) return {};
    auto tree = juce::ValueTree::readFromGZIPData (mb.getData(), mb.getSize());
    return tree.getProperty ("key").toString();                // JUCE's OnlineUnlockStatus gemmer noeglefilen under "key"
}

juce::String PitchBendUnlock::requestDeactivation()
{
    juce::StringPairArray f;
    f.set ("product", productID);
    f.set ("email", getUserEmail());
    f.set ("mach", getLocalMachineIDs().joinIntoString (","));
    f.set ("keyfile", currentKeyFile());                        // beviser, at det er denne computers licens
    return postForm (serverURL + "/api/deactivate", f);
}

PitchBendUnlock::UnlockResult PitchBendUnlock::applyDeactivationReply (const juce::String& reply)
{
    UnlockResult r;
    r.succeeded = false;
    auto xml = juce::parseXML (reply);
    if (xml == nullptr)
    {
        r.errorMessage = "Couldn't reach the SoundSo licence server, so this computer is still active. Try again later.";
        return r;
    }
    if (xml->hasTagName ("ERROR"))
    {
        r.errorMessage = xml->getStringAttribute ("error").trim();
        return r;
    }
    r.informativeMessage = xml->getStringAttribute ("message").trim();
    r.succeeded = true;
    forget();
    return r;
}

void PitchBendUnlock::forget()
{
    saveState ({});
    load();                                                     // tom tilstand: laast
}

bool PitchBendUnlock::reloadIfChanged()
{
    const auto t = stateFile.getLastModificationTime();
    if (t == lastModified) return false;
    load();                                                     // kalder getState, som saetter lastModified
    return true;
}
