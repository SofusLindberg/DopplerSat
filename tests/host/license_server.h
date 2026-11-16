#pragma once
#include <JuceHeader.h>

// =============================================================================
//  Lokal test-licensserver (2.08, brugeren 2026-11-16). KUN til udvikling og test paa denne computer: lytter paa
//  127.0.0.1, gemmer konti i en JSON-fil og signerer noeglefiler med den private noegle. Den rigtige server (paa
//  nettet, https) skal goere det samme, men med platformens sikre login (fx Supabase Auth) i stedet for vores
//  adgangskode-hashes, og den private noegle i serverens hemmelige indstillinger.
//
//  POST /api/unlock      product, mach, os + (email, pw) eller key  ->  <MESSAGE><KEY>..</KEY></MESSAGE> / <ERROR>
//  POST /api/deactivate  product, mach, email, keyfile              ->  <MESSAGE> / <ERROR>
//  GET  /api/ping                                                    ->  <MESSAGE message="ok"/>
// =============================================================================
class LicenseServerCore
{
public:
    LicenseServerCore (juce::File dataFile, juce::RSAKey privateKey, juce::RSAKey publicKey);
    ~LicenseServerCore();

    // Opret konto (hvis den ikke findes) med en ny licens; giver licensnoeglen. Adgangskoden gemmes saltet og hashet.
    juce::String addLicence (const juce::String& email, const juce::String& password);
    int activationCount (const juce::String& licenceKey) const;

    // Et kald (uden HTTP), fx /api/unlock
    juce::String handle (const juce::String& path, const juce::StringPairArray& form);

    // HTTP paa 127.0.0.1 i en baggrundstraad
    bool start (int port);
    void stop();
    int getPort() const { return port; }

    // RSA-noeglepar med offentlig eksponent 65537 (JUCE's createKeyPair kan vaelge 3, og uden padding kan en lille
    // eksponent i princippet give falske noeglefiler). bits: 2048 til rigtige noegler, mindre til hurtige tests.
    static void makeKeyPair (juce::RSAKey& publicKey, juce::RSAKey& privateKey, int bits);
    // Noeglefilens XML (som JUCE's KeyFileUtils::decryptXML); nullptr hvis den ikke kan laeses med noeglen
    static std::unique_ptr<juce::XmlElement> readKeyFile (const juce::String& keyFile, const juce::RSAKey& publicKey);

private:
    juce::String unlock (const juce::StringPairArray& form);
    juce::String deactivate (const juce::StringPairArray& form);
    juce::DynamicObject* findAccount (const juce::String& email) const;
    void findLicence (const juce::String& key, juce::DynamicObject*& account, juce::DynamicObject*& licence) const;
    void save() const;
    static juce::String hashPassword (const juce::String& salt, const juce::String& password);
    static juce::String error (const juce::String& message);

    juce::File dataFile;
    juce::RSAKey privateKey, publicKey;
    juce::var data;                         // { "accounts": [ { email, salt, hash, licences: [ { key, product, activations: [ { ids, os } ] } ] } ] }
    juce::CriticalSection lock;
    juce::Random rng;

    struct HttpThread;
    std::unique_ptr<HttpThread> http;
    int port = 0;
};
