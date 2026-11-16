#pragma once
#include <JuceHeader.h>

// =============================================================================
//  Licens (2.08, brugeren 2026-11-16): "Pluginnet skal kun kunne bruges, hvis man har købt serial key gennem
//  hjemmesiden ved at benytte et login. Pluginnet skal åbnes med en key eller login på ens konto fra hjemmesiden
//  inde i pluginnet. Al salg skal foregå med maksimal sikkerhed [...]"
// =============================================================================
// Bygger paa JUCE's OnlineUnlockStatus. Pluginnet sender login (e-mail + adgangskode) eller en licensnoegle plus
// computerens maskine-id'er til licensserveren. Serveren svarer med en noeglefil: en lille XML (produkt, e-mail,
// maskine-id'er), krypteret med serverens PRIVATE RSA-noegle (2048 bit, offentlig eksponent 65537). Pluginnet kender kun
// den OFFENTLIGE noegle (LicensePublicKey.h) og kan derfor tjekke en noeglefil, men ikke lave en. Noeglefilen gemmes i
// brugerens programdata og virker uden internet. Uden gyldig noeglefil koerer pluginnet som Bypass (toer lyd, samme
// latens), og editoren viser laas-op-skaermen.
//
// Serverens svar (XML, som JUCE forventer):
//   <MESSAGE message="..."><KEY>...noeglefil...</KEY></MESSAGE>     laast op
//   <ERROR error="..."/>                                              afvist (forkert login, for mange computere ...)
class PitchBendUnlock : public juce::OnlineUnlockStatus
{
public:
    static constexpr const char* productID = "PitchBendTexture";   // samme for alle versioner (2.08 og frem)
    static constexpr int maxComputers = 3;

    PitchBendUnlock (juce::File stateFile = defaultStateFile(),
                     juce::String serverURL = defaultServerURL(),
                     juce::String publicKey = defaultPublicKey());

    static juce::File   defaultStateFile();     // %APPDATA%\SoundSo\Pitch Bend Texture\licence.dat (Mac: Application Support)
    static juce::String defaultServerURL();     // DS_LICENSE_URL (udvikling: den lokale test-server)
    static juce::String defaultPublicKey();     // DS_LICENSE_PUBLIC_KEY fra LicensePublicKey.h
    static juce::String shopURL();              // DS_SHOP_URL: hjemmesidens shop (koeb / glemt adgangskode)

    // ---- OnlineUnlockStatus
    juce::String getProductID() override                               { return productID; }
    bool doesProductIDMatch (const juce::String& id) override          { return id == productID; }
    juce::RSAKey getPublicKey() override                               { return juce::RSAKey (publicKey); }
    void saveState (const juce::String&) override;
    juce::String getState() override;
    juce::String getWebsiteName() override                             { return "SoundSo"; }
    juce::URL getServerAuthenticationURL() override                    { return juce::URL (serverURL + "/api/unlock"); }
    // email tom = password er en licensnoegle. Blokerer: kald IKKE paa beskedtraaden.
    juce::String readReplyFromWebserver (const juce::String& email, const juce::String& password) override;

    // Serverens svar -> noeglefilen anvendes og gemmes (paa beskedtraaden). Samme som JUCE's eget svar-tjek.
    UnlockResult applyServerReply (const juce::String& reply);
    // Deaktiver denne computer paa serveren (blokerer); ved succes glemmes noeglefilen her.
    juce::String requestDeactivation();                          // blokerer, ikke paa beskedtraaden: serverens svar
    UnlockResult applyDeactivationReply (const juce::String& reply);
    void forget();                                               // glem noeglefilen paa denne computer
    bool reloadIfChanged();                                      // en anden instans har laast op / deaktiveret
    juce::String currentKeyFile();                               // noeglefilen, som den er gemt
    juce::File getStateFile() const                              { return stateFile; }

    // POST som formular (application/x-www-form-urlencoded); "" hvis serveren ikke svarer. Blokerer.
    static juce::String postForm (const juce::String& url, const juce::StringPairArray& fields, int timeoutMs = 10000);

private:
    juce::File stateFile;
    juce::String serverURL, publicKey;
    juce::Time lastModified;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchBendUnlock)
};
