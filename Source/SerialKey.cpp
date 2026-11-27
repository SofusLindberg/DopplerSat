#include "SerialKey.h"
#include "SerialKeyHash.h"

PitchBendSerial::PitchBendSerial (juce::File f) : stateFile (std::move (f))
{
    load();
}

juce::File PitchBendSerial::defaultStateFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    dir = dir.getChildFile ("Application Support");
   #endif
    return dir.getChildFile ("SoundSo").getChildFile ("Pitch Bend Texture").getChildFile ("serial.dat");
}

juce::String PitchBendSerial::normalise (const juce::String& typed)
{
    return typed.toUpperCase().retainCharacters ("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
}

juce::String PitchBendSerial::format (const juce::String& n)
{
    juce::String s ("PBT");
    const auto body = n.startsWith ("PBT") ? n.substring (3) : n;
    for (int i = 0; i < body.length(); i += 5)
        s << "-" << body.substring (i, i + 5);
    return s;
}

juce::String PitchBendSerial::hashFor (const juce::String& salt, const juce::String& typed)
{
    return juce::SHA256 ((salt + ":" + normalise (typed)).toUTF8()).toHexString();
}

bool PitchBendSerial::keyMatches (const juce::String& typed)
{
    const juce::String want (DS_SERIAL_HASH);
    if (want.length() != 64 || normalise (typed).length() < 20)
        return false;
    // Samme tid uanset hvor tidligt tegnene er forskellige (ikke afgoerende her, men gratis)
    const auto got = hashFor (DS_SERIAL_SALT, typed);
    int diff = 0;
    for (int i = 0; i < 64; ++i)
        diff |= (int) (got[i] ^ want[i]);
    return diff == 0;
}

// Noeglen til at kryptere den gemte serienoegle: afledt af computerens id, saa filen kun virker paa denne computer
juce::BlowFish PitchBendSerial::machineCipher()
{
    auto id = juce::SystemStats::getUniqueDeviceID();
    if (id.isEmpty())
        id = juce::SystemStats::getComputerName();               // reserve: id'et kunne ikke laeses
    const juce::SHA256 k (("PitchBendTexture-serial|" + id).toUTF8());
    const auto raw = k.getRawData();
    return juce::BlowFish (raw.getData(), (int) raw.getSize());
}

void PitchBendSerial::load()
{
    unlocked = false;
    lastModified = stateFile.getLastModificationTime();
    if (! stateFile.existsAsFile())
        return;

    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (stateFile.loadFileAsString().trim()) || mb.isEmpty())
        return;
    const int n = machineCipher().decrypt (mb.getData(), mb.getSize());
    if (n <= 0)                                                  // forkert computer: udfyldningen passer ikke
        return;
    mb.setSize ((size_t) n);
    unlocked = keyMatches (mb.toString());
}

bool PitchBendSerial::tryUnlock (const juce::String& typed)
{
    if (! keyMatches (typed))
        return false;

    juce::MemoryBlock mb;
    mb.append (normalise (typed).toRawUTF8(), (size_t) normalise (typed).getNumBytesAsUTF8());
    machineCipher().encrypt (mb);
    stateFile.getParentDirectory().createDirectory();
    if (! stateFile.replaceWithText (mb.toBase64Encoding()))
        return false;
    load();
    return unlocked;
}

void PitchBendSerial::forget()
{
    stateFile.deleteFile();
    load();
}

bool PitchBendSerial::reloadIfChanged()
{
    if (stateFile.getLastModificationTime() == lastModified)
        return false;
    load();
    return true;
}
