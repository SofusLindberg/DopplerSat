// LicenseTest (2.08): hele licensforloebet mod en lokal test-server med et midlertidigt testnoeglepar og egne filer
// (roerer hverken brugerens licens eller test-serverens data). Pluginnets egen PitchBendUnlock bruges.
#include <JuceHeader.h>
#include "license_server.h"
#include "../../Source/License.h"

static int failures = 0;
static void check (const char* what, bool ok, const juce::String& detail = {})
{
    std::printf ("  %-62s %s%s\n", what, ok ? "OK" : "FEJL", detail.isEmpty() ? "" : ("  (" + detail + ")").toRawUTF8());
    if (! ok) ++failures;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("pbt-licence-test-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
    dir.createDirectory();

    std::printf ("Testnoeglepar (1024 bit) ...\n");
    juce::RSAKey pub, priv;
    LicenseServerCore::makeKeyPair (pub, priv, 1024);

    LicenseServerCore server (dir.getChildFile ("licences.json"), priv, pub);
    if (! server.start (0)) { std::printf ("FEJL: test-serveren kunne ikke starte\n"); return 1; }
    const juce::String url = "http://127.0.0.1:" + juce::String (server.getPort());
    const auto key = server.addLicence ("buyer@example.com", "correct horse");
    std::printf ("Test-server paa %s, licens %s\n", url.toRawUTF8(), key.toRawUTF8());

    auto unlockWith = [&] (PitchBendUnlock& lic, const juce::String& email, const juce::String& pw)
    {
        return lic.applyServerReply (lic.readReplyFromWebserver (email, pw));
    };
    const auto state = dir.getChildFile ("licence.dat");

    {
        PitchBendUnlock lic (state, url, pub.toString());
        lic.load();
        check ("uden licens: laast", ! (bool) lic.isUnlocked());

        auto r = unlockWith (lic, "buyer@example.com", "wrong");
        check ("forkert adgangskode: afvist og stadig laast", ! r.succeeded && ! (bool) lic.isUnlocked(), r.errorMessage);
        r = unlockWith (lic, "nobody@example.com", "correct horse");
        check ("ukendt e-mail: afvist", ! r.succeeded && ! (bool) lic.isUnlocked(), r.errorMessage);
        r = unlockWith (lic, "buyer@example.com", "correct horse");
        check ("rigtigt login: laast op", r.succeeded && (bool) lic.isUnlocked(), r.informativeMessage);
        check ("licensen gemt i filen", state.existsAsFile());
        check ("serveren taeller 1 computer", server.activationCount (key) == 1);
    }
    {
        PitchBendUnlock lic (state, url, pub.toString());
        lic.load();
        check ("efter genstart (offline): stadig laast op", (bool) lic.isUnlocked());
        check ("e-mail kendt efter genstart", lic.getUserEmail() == "buyer@example.com", lic.getUserEmail());
        const auto r = unlockWith (lic, "buyer@example.com", "correct horse");
        check ("samme computer igen: stadig 1 computer", r.succeeded && server.activationCount (key) == 1);
    }

    // Licensnoegle paa en anden "installation" (samme computer -> samme plads)
    {
        const auto state2 = dir.getChildFile ("licence2.dat");
        PitchBendUnlock lic (state2, url, pub.toString());
        lic.load();
        auto r = unlockWith (lic, "", "PBT-AAAAA-BBBBB-CCCCC-DDDDD");
        check ("forkert licensnoegle: afvist", ! r.succeeded && ! (bool) lic.isUnlocked(), r.errorMessage);
        r = unlockWith (lic, "", key.toLowerCase());
        check ("rigtig licensnoegle (smaa bogstaver): laast op", r.succeeded && (bool) lic.isUnlocked(), r.informativeMessage);
    }

    // Hoejst 3 computere (andre computere efterlignes med falske maskine-id'er)
    auto fakeUnlock = [&] (const juce::String& mach)
    {
        juce::StringPairArray f;
        f.set ("product", PitchBendUnlock::productID);
        f.set ("key", key);
        f.set ("mach", mach);
        return PitchBendUnlock::postForm (url + "/api/unlock", f);
    };
    check ("computer 2: laast op", fakeUnlock ("WFAKE00002,WFAKE00002B").contains ("<KEY>"));
    check ("computer 3: laast op", fakeUnlock ("WFAKE00003").contains ("<KEY>"));
    const auto fourth = fakeUnlock ("WFAKE00004");
    check ("computer 4: afvist (allerede 3 computere)", fourth.contains ("<ERROR") && server.activationCount (key) == 3,
           juce::parseXML (fourth) != nullptr ? juce::parseXML (fourth)->getStringAttribute ("error") : fourth);
    check ("computer 2 igen (et kendt id): stadig 3", fakeUnlock ("WFAKE00002B,WNEWID").contains ("<KEY>") && server.activationCount (key) == 3);

    // Deaktiver denne computer fra pluginnet -> pladsen er fri
    {
        PitchBendUnlock lic (state, url, pub.toString());
        lic.load();
        const auto r = lic.applyDeactivationReply (lic.requestDeactivation());
        check ("deaktiver denne computer: OK og laast", r.succeeded && ! (bool) lic.isUnlocked(), r.succeeded ? r.informativeMessage : r.errorMessage);
        check ("licensfilen slettet", ! state.existsAsFile());
        check ("serveren taeller 2 computere", server.activationCount (key) == 2);
        check ("nu kan computer 4 komme ind", fakeUnlock ("WFAKE00004").contains ("<KEY>"));
    }

    // Snyd: aendret noeglefil, noeglefil til en anden computer, andet produkt, falsk noegle
    {
        const auto reply = fakeUnlock ("WFAKE00004");
        auto keyFile = juce::parseXML (reply)->getChildByName ("KEY")->getAllSubText().trim();
        PitchBendUnlock lic (dir.getChildFile ("licence3.dat"), url, pub.toString());
        lic.load();
        check ("noeglefil til en anden computer: virker ikke her", lic.applyKeyFile (keyFile).failed() && ! (bool) lic.isUnlocked());

        const auto mine = juce::KeyGeneration::generateKeyFile (PitchBendUnlock::productID, "buyer@example.com", "buyer",
                                                                lic.getLocalMachineIDs().joinIntoString (","), priv);
        auto tampered = mine;
        const int pos = tampered.lastIndexOfChar ('#') + 20;
        tampered = tampered.substring (0, pos) + (tampered[pos] == 'a' ? "b" : "a") + tampered.substring (pos + 1);
        check ("aendret noeglefil (et tegn): afvist", lic.applyKeyFile (tampered).failed() && ! (bool) lic.isUnlocked());

        const auto other = juce::KeyGeneration::generateKeyFile ("OtherProduct", "buyer@example.com", "buyer",
                                                                 lic.getLocalMachineIDs().joinIntoString (","), priv);
        check ("noeglefil til et andet produkt: afvist", lic.applyKeyFile (other).failed() && ! (bool) lic.isUnlocked());

        juce::RSAKey fakePub, fakePriv;
        LicenseServerCore::makeKeyPair (fakePub, fakePriv, 512);
        const auto forged = juce::KeyGeneration::generateKeyFile (PitchBendUnlock::productID, "pirate@example.com", "pirate",
                                                                  lic.getLocalMachineIDs().joinIntoString (","), fakePriv);
        check ("noeglefil signeret med en anden noegle: afvist", lic.applyKeyFile (forged).failed() && ! (bool) lic.isUnlocked());

        check ("rigtig noeglefil til denne computer: virker", lic.applyKeyFile (mine).wasOk() && (bool) lic.isUnlocked());
    }

    // Serveren svarer ikke
    {
        PitchBendUnlock lic (dir.getChildFile ("licence4.dat"), "http://127.0.0.1:1", pub.toString());
        lic.load();
        const auto r = lic.applyServerReply (lic.readReplyFromWebserver ("buyer@example.com", "correct horse"));
        check ("ingen server: forklarende fejl, stadig laast", ! r.succeeded && r.errorMessage.isNotEmpty() && ! (bool) lic.isUnlocked(), r.errorMessage);
    }

    server.stop();
    dir.deleteRecursively();
    std::printf (failures == 0 ? "LICENS OK\n" : "LICENS FEJL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
