// LicenseTool (2.08): noegler og den lokale test-licensserver.
//
//   LicenseTool genkeys [--header <LicensePublicKey.h>] [--force]   nyt RSA-noeglepar (2048 bit, eksponent 65537)
//   LicenseTool serve [port]                                         test-serveren paa 127.0.0.1 (standard 8766)
//   LicenseTool add <email> <password>                               testkonto + licens -> licensnoeglen
//   LicenseTool unlock-dev                                           laas pluginnet op paa denne computer (udvikling)
//   LicenseTool lock-dev                                             glem licensen paa denne computer (se laas-op-skaermen)
//   LicenseTool machine                                              denne computers maskine-id'er
//   LicenseTool serial-new [--force]                                 2.10: ny serienoegle -> Source/SerialKeyHash.h + SoundSo/keys
//   LicenseTool serial-unlock <noegle>                               2.10: skriv noeglen som i pluginnet (tjekkes mod hashen)
//   LicenseTool serial-lock                                          2.10: glem serienoeglen paa denne computer
//
// Den PRIVATE noegle ligger kun i brugerens programdata (SoundSo/keys), aldrig i repoet. Den offentlige skrives ind i
// Source/LicensePublicKey.h, som pluginnet bygges med. Mistes den private noegle, kan der ikke laves nye licenser til
// de plugins, der er bygget med den offentlige: tag en sikker kopi (fx i en password manager).
#include <JuceHeader.h>
#include "license_server.h"
#include "../../Source/License.h"
#include "../../Source/SerialKey.h"
#include <random>

static juce::File soundSoDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    dir = dir.getChildFile ("Application Support");
   #endif
    return dir.getChildFile ("SoundSo");
}
static juce::File privateKeyFile() { return soundSoDir().getChildFile ("keys").getChildFile ("licence-private.key"); }
static juce::File publicKeyFile()  { return soundSoDir().getChildFile ("keys").getChildFile ("licence-public.key"); }
static juce::File serverDataFile() { return soundSoDir().getChildFile ("test-server").getChildFile ("licences.json"); }

static bool loadKeys (juce::RSAKey& priv, juce::RSAKey& pub)
{
    if (! privateKeyFile().existsAsFile() || ! publicKeyFile().existsAsFile())
    {
        std::printf ("FEJL: ingen noegler i %s - koer 'LicenseTool genkeys' foerst\n", privateKeyFile().getParentDirectory().getFullPathName().toRawUTF8());
        return false;
    }
    priv = juce::RSAKey (privateKeyFile().loadFileAsString().trim());
    pub  = juce::RSAKey (publicKeyFile().loadFileAsString().trim());
    return priv.isValid() && pub.isValid();
}

static int genkeys (const juce::StringArray& args)
{
    if (privateKeyFile().existsAsFile() && ! args.contains ("--force"))
    {
        std::printf ("Der findes allerede en privat noegle (%s). Brug --force for at erstatte den.\n"
                     "OBS: licenser lavet med den gamle noegle virker ikke i plugins bygget med den nye.\n",
                     privateKeyFile().getFullPathName().toRawUTF8());
        return 1;
    }
    std::printf ("Laver et RSA-noeglepar paa 2048 bit (kan tage et minut) ...\n");
    juce::RSAKey pub, priv;
    LicenseServerCore::makeKeyPair (pub, priv, 2048);
    privateKeyFile().getParentDirectory().createDirectory();
    privateKeyFile().replaceWithText (priv.toString());
    publicKeyFile().replaceWithText (pub.toString());
    std::printf ("Privat noegle: %s (tag en sikker kopi, del den aldrig)\n", privateKeyFile().getFullPathName().toRawUTF8());
    std::printf ("Offentlig noegle: %s\n", publicKeyFile().getFullPathName().toRawUTF8());

    const int h = args.indexOf ("--header");
    if (h >= 0 && h + 1 < args.size())
    {
        juce::String s;
        s << "#pragma once\n"
          << "// Licensens OFFENTLIGE RSA-noegle (2048 bit, eksponent 65537), lavet af 'LicenseTool genkeys' "
          << juce::Time::getCurrentTime().formatted ("%Y-%m-%d") << ".\n"
          << "// Pluginnet tjekker noeglefiler med den. Den PRIVATE noegle ligger kun hos licensserveren (og i udviklerens\n"
          << "// programdata, SoundSo/keys) - aldrig i repoet. Ny noegle = nyt build; gamle licenser virker saa ikke i det.\n"
          << "#define DS_LICENSE_PUBLIC_KEY \"" << pub.toString() << "\"\n";
        juce::File (args[h + 1]).replaceWithText (s);
        std::printf ("Skrevet: %s\n", args[h + 1].toRawUTF8());
    }
    return 0;
}

static int serve (const juce::StringArray& args)
{
    juce::RSAKey priv, pub;
    if (! loadKeys (priv, pub)) return 1;
    const int port = args.size() > 1 ? args[1].getIntValue() : 8766;
    LicenseServerCore core (serverDataFile(), priv, pub);
    if (! core.start (port))
    {
        std::printf ("FEJL: kunne ikke lytte paa port %d\n", port);
        return 1;
    }
    std::printf ("Test-licensserver paa http://127.0.0.1:%d (data: %s). Stop med Ctrl+C.\n", core.getPort(),
                 serverDataFile().getFullPathName().toRawUTF8());
    for (;;) juce::Thread::sleep (1000);
}

static int add (const juce::StringArray& args)
{
    if (args.size() < 3) { std::printf ("brug: LicenseTool add <email> <password>\n"); return 2; }
    juce::RSAKey priv, pub;
    if (! loadKeys (priv, pub)) return 1;
    LicenseServerCore core (serverDataFile(), priv, pub);
    std::printf ("Licens til %s: %s\n", args[1].toRawUTF8(), core.addLicence (args[1], args[2]).toRawUTF8());
    return 0;
}

static int unlockDev()
{
    juce::RSAKey priv, pub;
    if (! loadKeys (priv, pub)) return 1;
    PitchBendUnlock lic;                                         // pluginnets egen klasse og offentlige noegle
    lic.load();
    const auto keyFile = juce::KeyGeneration::generateKeyFile (PitchBendUnlock::productID, "developer@soundso.local", "developer",
                                                               lic.getLocalMachineIDs().joinIntoString (","), priv);
    const auto r = lic.applyKeyFile (keyFile);
    if (r.failed())
    {
        std::printf ("FEJL: %s (passer den private noegle til Source/LicensePublicKey.h? Byg LicenseTool igen efter genkeys)\n",
                     r.getErrorMessage().toRawUTF8());
        return 1;
    }
    lic.save();
    std::printf ("Laast op paa denne computer: %s\n", lic.getStateFile().getFullPathName().toRawUTF8());
    return 0;
}

static int lockDev()
{
    PitchBendUnlock lic;
    lic.forget();
    std::printf ("Licensen paa denne computer er glemt (%s)\n", lic.getStateFile().getFullPathName().toRawUTF8());
    return 0;
}

// 2.10 serienoegle (SerialKey.h). Noeglen: 20 tegn fra 32 (uden 0 / O / 1 / I), dvs. 100 bit, fra std::random_device
// (paa Windows / MSVC styresystemets kryptografiske tilfaeldighed).
static juce::File serialKeyFile() { return soundSoDir().getChildFile ("keys").getChildFile ("serial-key.txt"); }

static int serialNew (const juce::StringArray& args)
{
    if (serialKeyFile().existsAsFile() && ! args.contains ("--force"))
    {
        std::printf ("Der findes allerede en serienoegle (%s). Brug --force for at lave en ny.\n"
                     "OBS: en ny noegle virker ikke i plugins bygget med den gamle, og omvendt.\n",
                     serialKeyFile().getFullPathName().toRawUTF8());
        return 1;
    }
    const auto header = juce::File::getCurrentWorkingDirectory().getChildFile ("Source").getChildFile ("SerialKeyHash.h");
    if (! header.getParentDirectory().isDirectory())
    {
        std::printf ("FEJL: koer fra repoets rod (ingen Source-mappe)\n");
        return 1;
    }
    const char* alphabet = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
    std::random_device rd;
    juce::String body, salt;
    for (int i = 0; i < 20; ++i) body << alphabet[rd() % 32];
    for (int i = 0; i < 16; ++i) salt << juce::String::toHexString ((int) (rd() % 16));
    const auto key  = PitchBendSerial::format (body);
    const auto hash = PitchBendSerial::hashFor (salt, key);
    const auto date = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");

    serialKeyFile().getParentDirectory().createDirectory();
    serialKeyFile().replaceWithText ("Pitch Bend Texture serienoegle (2.10 og frem), lavet " + date + ":\r\n" + key + "\r\n");
    juce::String s;
    s << "#pragma once\n"
      << "// Serienoeglens saltede SHA-256 (2.10, 'LicenseTool serial-new' " << date << ").\n"
      << "// Selve noeglen staar IKKE her: kun i udviklerens programdata (SoundSo/keys/serial-key.txt).\n"
      << "#define DS_SERIAL_SALT \"" << salt << "\"\n"
      << "#define DS_SERIAL_HASH \"" << hash << "\"\n";
    header.replaceWithText (s);
    std::printf ("Serienoegle: %s\nGemt i: %s\nSkrevet: %s (byg pluginnet og LicenseTool igen)\n", key.toRawUTF8(),
                 serialKeyFile().getFullPathName().toRawUTF8(), header.getFullPathName().toRawUTF8());
    return 0;
}

static int serialUnlock (const juce::StringArray& args)
{
    if (args.size() < 2)
    {
        std::printf ("brug: LicenseTool serial-unlock <noegle>\n");
        return 2;
    }
    PitchBendSerial s;
    if (! s.tryUnlock (args[1]))
    {
        std::printf ("FEJL: forkert noegle (eller LicenseTool er bygget med en anden SerialKeyHash.h)\n");
        return 1;
    }
    PitchBendSerial again;                                       // ny instans: laeses og dekrypteres fra filen
    std::printf ("Laast op paa denne computer (%s), genlaest: %s\n", s.getStateFile().getFullPathName().toRawUTF8(),
                 again.isUnlocked() ? "OK" : "FEJL");
    return again.isUnlocked() ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);
    const auto cmd = args[0];
    if (cmd == "genkeys")    return genkeys (args);
    if (cmd == "serve")      return serve (args);
    if (cmd == "add")        return add (args);
    if (cmd == "unlock-dev") return unlockDev();
    if (cmd == "lock-dev")   return lockDev();
    if (cmd == "serial-new")    return serialNew (args);
    if (cmd == "serial-unlock") return serialUnlock (args);
    if (cmd == "serial-lock")
    {
        PitchBendSerial s;
        s.forget();
        std::printf ("Serienoeglen er glemt paa denne computer (%s)\n", s.getStateFile().getFullPathName().toRawUTF8());
        return 0;
    }
    if (cmd == "machine")
    {
        PitchBendUnlock lic;
        std::printf ("%s\n", lic.getLocalMachineIDs().joinIntoString ("\n").toRawUTF8());
        return 0;
    }
    std::printf ("brug: LicenseTool genkeys [--header <fil>] [--force] | serve [port] | add <email> <password> | "
                 "unlock-dev | lock-dev | machine | serial-new [--force] | serial-unlock <noegle> | serial-lock\n");
    return 2;
}
