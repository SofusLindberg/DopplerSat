#include "license_server.h"
#include <random>

namespace
{
constexpr const char* kProduct = "PitchBendTexture";
constexpr int kMaxComputers = 3;

// std::random_device er kryptografisk tilfaeldig paa Windows (RtlGenRandom) og Mac (/dev/urandom)
juce::uint32 secureRandom()
{
    static std::random_device rd;
    return (juce::uint32) rd();
}

juce::String randomHex (int bytes)
{
    juce::String s;
    for (int i = 0; i < bytes; ++i)
        s << juce::String::toHexString ((int) (secureRandom() & 0xff)).paddedLeft ('0', 2);
    return s;
}

juce::String randomLicenceKey()
{
    static const char* alphabet = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";   // ingen 0/O og 1/I/L (let at laese og skrive)
    juce::String k ("PBT");
    for (int g = 0; g < 4; ++g)
    {
        k << "-";
        for (int i = 0; i < 5; ++i) k << alphabet[secureRandom() % 32];
    }
    return k;                                                             // 4 x 5 tegn = 100 bit
}

juce::BigInteger randomPrime (int bits)
{
    for (;;)
    {
        juce::BigInteger p;
        for (int i = 0; i < bits; i += 32)
        {
            const auto v = secureRandom();
            for (int b = 0; b < 32 && i + b < bits; ++b)
                if (v & (1u << b)) p.setBit (i + b);
        }
        p.setBit (bits - 1);
        p.setBit (bits - 2);                                              // n = p q faar hele laengden
        p.setBit (0);
        for (int tries = 0; tries < 20000; ++tries, p += juce::BigInteger (2))
            if (juce::Primes::isProbablyPrime (p, 40))
                return p;
    }
}

juce::StringArray idsFrom (const juce::String& s)
{
    auto a = juce::StringArray::fromTokens (s, ",;", "");
    a.trim();
    a.removeEmptyStrings();
    return a;
}

bool overlaps (const juce::StringArray& a, const juce::var& known)
{
    if (auto* arr = known.getArray())
        for (auto& v : *arr)
            if (a.contains (v.toString(), true)) return true;
    return false;
}
}

//==============================================================================
void LicenseServerCore::makeKeyPair (juce::RSAKey& pub, juce::RSAKey& priv, int bits)
{
    const juce::BigInteger e (65537);
    for (;;)
    {
        const auto p = randomPrime (bits / 2), q = randomPrime (bits / 2);
        if (p == q) continue;
        const auto n = p * q;
        const auto m = (p - juce::BigInteger (1)) * (q - juce::BigInteger (1));
        if (e.findGreatestCommonDivisor (m) != juce::BigInteger (1)) continue;
        auto d = e;
        d.inverseModulo (m);
        pub  = juce::RSAKey (e.toString (16) + "," + n.toString (16));
        priv = juce::RSAKey (d.toString (16) + "," + n.toString (16));
        return;
    }
}

std::unique_ptr<juce::XmlElement> LicenseServerCore::readKeyFile (const juce::String& keyFile, const juce::RSAKey& key)
{
    const auto hex = keyFile.fromLastOccurrenceOf ("#", false, false).removeCharacters (" \t\r\n");
    juce::BigInteger val;
    val.parseString (hex, 16);
    if (val.isZero() || ! key.isValid()) return nullptr;
    key.applyToValue (val);
    const auto mb = val.toMemoryBlock();
    if (! juce::CharPointer_UTF8::isValidString (static_cast<const char*> (mb.getData()), (int) mb.getSize())) return nullptr;
    return juce::parseXML (mb.toString());
}

//==============================================================================
LicenseServerCore::LicenseServerCore (juce::File f, juce::RSAKey priv, juce::RSAKey pub)
    : dataFile (std::move (f)), privateKey (priv), publicKey (pub)
{
    if (dataFile.existsAsFile()) data = juce::JSON::parse (dataFile);
    if (! data.isObject())
    {
        data = new juce::DynamicObject();
        data.getDynamicObject()->setProperty ("accounts", juce::Array<juce::var>());
    }
}

LicenseServerCore::~LicenseServerCore() { stop(); }

void LicenseServerCore::save() const
{
    dataFile.getParentDirectory().createDirectory();
    dataFile.replaceWithText (juce::JSON::toString (data));
}

juce::String LicenseServerCore::hashPassword (const juce::String& salt, const juce::String& password)
{
    // Kun test-serveren: saltet SHA-256, 20000 gange. Den rigtige server bruger platformens login (fx Supabase Auth).
    auto h = juce::SHA256 ((salt + password).toUTF8()).toHexString();
    for (int i = 0; i < 20000; ++i)
        h = juce::SHA256 ((h + salt + password).toUTF8()).toHexString();
    return h;
}

juce::String LicenseServerCore::error (const juce::String& message)
{
    juce::XmlElement e ("ERROR");
    e.setAttribute ("error", message);
    return e.toString (juce::XmlElement::TextFormat().withoutHeader().singleLine());
}

juce::DynamicObject* LicenseServerCore::findAccount (const juce::String& email) const
{
    for (auto& a : *data["accounts"].getArray())
        if (a["email"].toString().equalsIgnoreCase (email.trim()))
            return a.getDynamicObject();
    return nullptr;
}

void LicenseServerCore::findLicence (const juce::String& key, juce::DynamicObject*& account, juce::DynamicObject*& licence) const
{
    account = nullptr;
    licence = nullptr;
    for (auto& a : *data["accounts"].getArray())
        for (auto& l : *a["licences"].getArray())
            if (l["key"].toString() == key)
            {
                account = a.getDynamicObject();
                licence = l.getDynamicObject();
                return;
            }
}

juce::String LicenseServerCore::addLicence (const juce::String& email, const juce::String& password)
{
    const juce::ScopedLock sl (lock);
    auto* acc = findAccount (email);
    if (acc == nullptr)
    {
        juce::var a (new juce::DynamicObject());
        acc = a.getDynamicObject();
        const auto salt = randomHex (16);
        acc->setProperty ("email", email.trim());
        acc->setProperty ("salt", salt);
        acc->setProperty ("hash", hashPassword (salt, password));
        acc->setProperty ("licences", juce::Array<juce::var>());
        data["accounts"].getArray()->add (a);
    }
    juce::var l (new juce::DynamicObject());
    const auto key = randomLicenceKey();
    l.getDynamicObject()->setProperty ("key", key);
    l.getDynamicObject()->setProperty ("product", kProduct);
    l.getDynamicObject()->setProperty ("created", juce::Time::getCurrentTime().toISO8601 (true));
    l.getDynamicObject()->setProperty ("activations", juce::Array<juce::var>());
    acc->getProperty ("licences").getArray()->add (l);
    save();
    return key;
}

int LicenseServerCore::activationCount (const juce::String& key) const
{
    const juce::ScopedLock sl (lock);
    juce::DynamicObject *acc, *lic;
    findLicence (key, acc, lic);
    return lic != nullptr ? lic->getProperty ("activations").getArray()->size() : -1;
}

juce::String LicenseServerCore::handle (const juce::String& path, const juce::StringPairArray& form)
{
    const juce::ScopedLock sl (lock);
    if (path.startsWith ("/api/unlock"))     return unlock (form);
    if (path.startsWith ("/api/deactivate")) return deactivate (form);
    if (path.startsWith ("/api/ping"))       return "<MESSAGE message=\"ok\"/>";
    return error ("Unknown request.");
}

juce::String LicenseServerCore::unlock (const juce::StringPairArray& form)
{
    if (form["product"] != kProduct) return error ("Unknown product.");
    const auto ids = idsFrom (form["mach"]);
    if (ids.isEmpty()) return error ("Your computer could not be identified. Please contact SoundSo support.");

    juce::DynamicObject *acc = nullptr, *lic = nullptr;
    if (form["key"].trim().isNotEmpty())
    {
        findLicence (form["key"].trim().toUpperCase(), acc, lic);
        if (lic == nullptr || lic->getProperty ("product") != kProduct)
            return error ("This licence key is not valid. You can find your key in your account on the SoundSo website.");
    }
    else
    {
        acc = findAccount (form["email"]);
        if (acc == nullptr || hashPassword (acc->getProperty ("salt"), form["pw"]) != acc->getProperty ("hash").toString())
            return error ("Wrong email or password.");
        for (auto& l : *acc->getProperty ("licences").getArray())
            if (l["product"] == kProduct) { lic = l.getDynamicObject(); break; }
        if (lic == nullptr)
            return error ("This account has no licence for Pitch Bend Texture yet. You can buy one on the SoundSo website.");
    }

    // Samme computer (et af id'erne kendes) = samme plads; ellers en ny plads, hoejst 3
    auto* acts = lic->getProperty ("activations").getArray();
    int found = -1;
    for (int i = 0; i < acts->size() && found < 0; ++i)
        if (overlaps (ids, (*acts)[i]["ids"])) found = i;
    if (found < 0 && acts->size() >= kMaxComputers)
        return error ("This licence is already active on " + juce::String (kMaxComputers) + " computers. Deactivate one of them "
                      "first (in the plugin: Licence > Deactivate this computer).");

    juce::Array<juce::var> idVars;
    for (auto& s : ids) idVars.add (s);
    juce::var act (new juce::DynamicObject());
    act.getDynamicObject()->setProperty ("ids", idVars);
    act.getDynamicObject()->setProperty ("os", form["os"]);
    act.getDynamicObject()->setProperty ("activated", juce::Time::getCurrentTime().toISO8601 (true));
    if (found >= 0) acts->set (found, act);
    else            acts->add (act);
    save();

    const auto email = acc->getProperty ("email").toString();
    auto user = email.upToFirstOccurrenceOf ("@", false, false).replaceCharacters (" ,;", "___");
    if (user.isEmpty()) user = "customer";
    const auto keyFile = juce::KeyGeneration::generateKeyFile (kProduct, email, user, ids.joinIntoString (","), privateKey);

    juce::XmlElement msg ("MESSAGE");
    msg.setAttribute ("message", "Unlocked on this computer (" + juce::String (acts->size()) + " of "
                                 + juce::String (kMaxComputers) + " computers in use).");
    msg.createNewChildElement ("KEY")->addTextElement (keyFile);
    return msg.toString (juce::XmlElement::TextFormat().withoutHeader());
}

juce::String LicenseServerCore::deactivate (const juce::StringPairArray& form)
{
    if (form["product"] != kProduct) return error ("Unknown product.");
    auto xml = readKeyFile (form["keyfile"], publicKey);
    if (xml == nullptr || xml->getStringAttribute ("app") != kProduct)
        return error ("The licence on this computer could not be read. Please contact SoundSo support.");
    const auto email = xml->getStringAttribute ("email");
    if (! email.equalsIgnoreCase (form["email"].trim()))
        return error ("The licence on this computer belongs to another account.");
    const auto keyIds = idsFrom (xml->getStringAttribute ("mach"));
    const auto reqIds = idsFrom (form["mach"]);
    bool sameComputer = false;
    for (auto& id : reqIds) sameComputer = sameComputer || keyIds.contains (id, true);
    if (! sameComputer) return error ("The licence on this computer belongs to another computer.");

    if (auto* acc = findAccount (email))
        for (auto& l : *acc->getProperty ("licences").getArray())
        {
            if (l["product"] != kProduct) continue;
            auto* acts = l["activations"].getArray();
            for (int i = 0; i < acts->size(); ++i)
                if (overlaps (keyIds, (*acts)[i]["ids"]))
                {
                    acts->remove (i);
                    save();
                    juce::XmlElement msg ("MESSAGE");
                    msg.setAttribute ("message", "This computer is deactivated (" + juce::String (acts->size()) + " of "
                                                 + juce::String (kMaxComputers) + " computers in use).");
                    return msg.toString (juce::XmlElement::TextFormat().withoutHeader().singleLine());
                }
        }
    return error ("This computer is not active on your licence.");
}

//==============================================================================
struct LicenseServerCore::HttpThread : public juce::Thread
{
    explicit HttpThread (LicenseServerCore& o) : juce::Thread ("Licence test server"), owner (o) {}

    void run() override
    {
        while (! threadShouldExit())
        {
            std::unique_ptr<juce::StreamingSocket> c (listener.waitForNextConnection());
            if (c == nullptr)
            {
                if (threadShouldExit()) break;
                juce::Thread::sleep (5);
                continue;
            }
            serve (*c);
        }
    }

    void serve (juce::StreamingSocket& c)
    {
        juce::MemoryBlock buf;
        char tmp[4096];
        int headerEnd = -1, contentLength = 0;
        juce::String text;
        const auto deadline = juce::Time::getMillisecondCounter() + 5000;
        while (juce::Time::getMillisecondCounter() < deadline)
        {
            const int ready = c.waitUntilReady (true, 200);
            if (ready < 0) break;
            if (ready == 0) continue;
            const int n = c.read (tmp, (int) sizeof (tmp), false);
            if (n <= 0) break;
            buf.append (tmp, (size_t) n);
            text = buf.toString();
            if (headerEnd < 0 && (headerEnd = text.indexOf ("\r\n\r\n")) >= 0)
                for (auto& line : juce::StringArray::fromLines (text.substring (0, headerEnd)))
                    if (line.startsWithIgnoreCase ("Content-Length:"))
                        contentLength = line.fromFirstOccurrenceOf (":", false, false).trim().getIntValue();
            if (headerEnd >= 0 && (int) buf.getSize() >= headerEnd + 4 + contentLength) break;
        }
        if (headerEnd < 0) { c.close(); return; }

        const auto requestLine = text.upToFirstOccurrenceOf ("\r\n", false, false);
        const auto path = requestLine.fromFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf (" ", false, false);
        juce::StringPairArray form;
        for (auto& pair : juce::StringArray::fromTokens (text.substring (headerEnd + 4), "&", ""))
        {
            auto decode = [] (juce::String s) { return juce::URL::removeEscapeChars (s.replaceCharacter ('+', ' ')); };
            if (pair.containsChar ('='))
                form.set (decode (pair.upToFirstOccurrenceOf ("=", false, false)), decode (pair.fromFirstOccurrenceOf ("=", false, false)));
        }
        const auto reply = owner.handle (path, form);
        const juce::String response = "HTTP/1.1 200 OK\r\nContent-Type: text/xml; charset=utf-8\r\nContent-Length: "
                                    + juce::String ((int) reply.getNumBytesAsUTF8()) + "\r\nConnection: close\r\n\r\n" + reply;
        c.write (response.toRawUTF8(), (int) response.getNumBytesAsUTF8());
        c.close();
    }

    LicenseServerCore& owner;
    juce::StreamingSocket listener;
};

bool LicenseServerCore::start (int p)
{
    stop();
    http = std::make_unique<HttpThread> (*this);
    if (! http->listener.createListener (p, "127.0.0.1"))
    {
        http.reset();
        return false;
    }
    port = http->listener.getBoundPort();
    http->startThread();
    return true;
}

void LicenseServerCore::stop()
{
    if (http == nullptr) return;
    http->signalThreadShouldExit();
    http->listener.close();
    http->stopThread (3000);
    http.reset();
}
