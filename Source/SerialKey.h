#pragma once
#include <JuceHeader.h>

// =============================================================================
//  Serienoegle (2.10, brugeren 2026-11-27): "Genbrug serial-key (som kun åbnes med én bestemt kode), som egentlig bare
//  fungerer som password i denne demo for, at pluginnet ikke kopieres ud på nettet. Når serialkey er skrevet i pluginnet
//  på pågældende brugeres computer, skal han aldrig skrive den igen. [...] (den skal ikke lægges i mappen med pluginnet)"
// =============================================================================
// Offline, ingen server. Pluginnet kender ikke selve noeglen, kun en saltet SHA-256 af den (SerialKeyHash.h), saa den
// ikke kan laeses ud af pluginfilen. Noeglen er 20 tilfaeldige tegn fra 32 (100 bit), saa den ikke kan gaettes ud fra
// hashen. Naar den rigtige noegle er skrevet, gemmes den krypteret med denne computers id (SMBIOS-UUID, JUCE's
// getUniqueDeviceID) i brugerens programdata. Ved start dekrypteres og tjekkes den igen: kopieres filen til en anden
// computer, giver den ingen mening dér, og noeglen skal skrives igen. Samme fil og noegle for alle versioner fra 2.10.
//
// Laver noeglen: LicenseTool serial-new (skriver SerialKeyHash.h og gemmer noeglen i SoundSo/keys, aldrig i repoet).
class PitchBendSerial
{
public:
    explicit PitchBendSerial (juce::File stateFile = defaultStateFile());

    static juce::File   defaultStateFile();          // %APPDATA%\SoundSo\Pitch Bend Texture\serial.dat (Mac: Application Support)
    static juce::String normalise (const juce::String& typed);   // store bogstaver, uden mellemrum og bindestreger
    static juce::String format (const juce::String& normalised); // PBT-XXXXX-XXXXX-XXXXX-XXXXX
    static juce::String hashFor (const juce::String& salt, const juce::String& typed);
    static bool         keyMatches (const juce::String& typed);  // mod SerialKeyHash.h

    bool isUnlocked() const                    { return unlocked; }
    bool tryUnlock (const juce::String& typed);  // rigtig noegle: gemmes og laast op
    void forget();                             // glem noeglen paa denne computer (test)
    bool reloadIfChanged();                    // en anden instans har laast op: sand hvis tilstanden blev genlaest
    juce::File getStateFile() const            { return stateFile; }

private:
    void load();
    static juce::BlowFish machineCipher();

    juce::File stateFile;
    juce::Time lastModified;
    bool unlocked = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchBendSerial)
};
