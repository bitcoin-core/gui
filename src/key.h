// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KEY_H
#define BITCOIN_KEY_H

#include <pubkey.h>
#include <serialize.h>
#include <support/allocators/secure.h>
#include <uint256.h>

#include <stdexcept>
#include <vector>


/**
 * CPrivKey is a serialized private key, with all parameters included
 * (SIZE bytes)
 */
typedef std::vector<unsigned char, secure_allocator<unsigned char> > CPrivKey;

/** Size of ECDH shared secrets. */
constexpr static size_t ECDH_SECRET_SIZE = CSHA256::OUTPUT_SIZE;

// Used to represent ECDH shared secret (ECDH_SECRET_SIZE bytes)
using ECDHSecret = std::array<std::byte, ECDH_SECRET_SIZE>;

/** An encapsulated private key. */
class CKey
{
public:
    /**
     * secp256k1:
     */
    static const unsigned int SIZE            = 279;
    static const unsigned int COMPRESSED_SIZE = 214;
    /**
     * see www.keylength.com
     * script supports up to 75 for single byte push
     */
    static_assert(
        SIZE >= COMPRESSED_SIZE,
        "COMPRESSED_SIZE is larger than SIZE");

private:
    /** Internal data container for private key material. */
    using KeyType = std::array<unsigned char, 32>;

    //! Whether the public key corresponding to this private key is (to be) compressed.
    bool fCompressed{false};

    //! The actual byte data. nullptr for invalid keys.
    secure_unique_ptr<KeyType> keydata;

    //! Check whether the 32-byte array pointed to by vch is valid keydata.
    bool static Check(const unsigned char* vch);

    void MakeKeyData()
    {
        if (!keydata) keydata = make_secure_unique<KeyType>();
    }

    void ClearKeyData()
    {
        keydata.reset();
    }

public:
    CKey() noexcept = default;
    CKey(CKey&&) noexcept = default;
    CKey& operator=(CKey&&) noexcept = default;

    CKey& operator=(const CKey& other)
    {
        if (other.keydata) {
            MakeKeyData();
            *keydata = *other.keydata;
        } else {
            ClearKeyData();
        }
        fCompressed = other.fCompressed;
        return *this;
    }

    CKey(const CKey& other) { *this = other; }

    friend bool operator==(const CKey& a, const CKey& b)
    {
        return a.fCompressed == b.fCompressed &&
            a.size() == b.size() &&
            memcmp(a.data(), b.data(), a.size()) == 0;
    }

    //! Initialize using begin and end iterators to byte data.
    template <typename T>
    void Set(const T pbegin, const T pend, bool fCompressedIn)
    {
        if (size_t(pend - pbegin) != std::tuple_size_v<KeyType>) {
            ClearKeyData();
        } else if (Check(&pbegin[0])) {
            MakeKeyData();
            memcpy(keydata->data(), (unsigned char*)&pbegin[0], keydata->size());
            fCompressed = fCompressedIn;
        } else {
            ClearKeyData();
        }
    }

    //! Simple read-only vector-like interface.
    unsigned int size() const { return keydata ? keydata->size() : 0; }
    const std::byte* data() const { return keydata ? reinterpret_cast<const std::byte*>(keydata->data()) : nullptr; }
    const unsigned char* begin() const { return keydata ? keydata->data() : nullptr; }
    const unsigned char* end() const { return begin() + size(); }

    //! Check whether this private key is valid.
    bool IsValid() const { return !!keydata; }

    //! Check whether the public key corresponding to this private key is (to be) compressed.
    bool IsCompressed() const { return fCompressed; }

    //! Generate a new private key using a cryptographic PRNG.
    void MakeNewKey(bool fCompressed);

    //! Negate private key
    bool Negate();

    /**
     * Convert the private key to a CPrivKey (serialized OpenSSL private key data).
     * This is expensive.
     */
    CPrivKey GetPrivKey() const;

    /**
     * Compute the public key from a private key.
     * This is expensive.
     */
    CPubKey GetPubKey() const;

    /**
     * Create a DER-serialized signature.
     * The test_case parameter tweaks the deterministic nonce.
     */
    bool Sign(const uint256& hash, std::vector<unsigned char>& vchSig, bool grind = true, uint32_t test_case = 0) const;

    /**
     * Create a compact signature (65 bytes), which allows reconstructing the used public key.
     * The format is one header byte, followed by two times 32 bytes for the serialized r and s values.
     * The header byte: 0x1B = first key with even y, 0x1C = first key with odd y,
     *                  0x1D = second key with even y, 0x1E = second key with odd y,
     *                  add 0x04 for compressed keys.
     */
    bool SignCompact(const uint256& hash, std::vector<unsigned char>& vchSig) const;

    /**
     * Create a BIP-340 Schnorr signature, for the xonly-pubkey corresponding to *this,
     * optionally tweaked by *merkle_root. Additional nonce entropy is provided through
     * aux.
     *
     * merkle_root is used to optionally perform tweaking of the private key, as specified
     * in BIP341:
     * - If merkle_root == nullptr: no tweaking is done, sign with key directly (this is
     *                              used for signatures in BIP342 script).
     * - If merkle_root->IsNull():  sign with key + H_TapTweak(pubkey) (this is used for
     *                              key path spending when no scripts are present).
     * - Otherwise:                 sign with key + H_TapTweak(pubkey || *merkle_root)
     *                              (this is used for key path spending, with specific
     *                              Merkle root of the script tree).
     */
    bool SignSchnorr(const uint256& hash, Span<unsigned char> sig, const uint256* merkle_root, const uint256& aux) const;

    //! Derive BIP32 child key.
    [[nodiscard]] bool Derive(CKey& keyChild, ChainCode &ccChild, unsigned int nChild, const ChainCode& cc) const;

    /**
     * Verify thoroughly whether a private key and a public key match.
     * This is done using a different mechanism than just regenerating it.
     */
    bool VerifyPubKey(const CPubKey& vchPubKey) const;

    //! Load private key and check that public key matches.
    bool Load(const CPrivKey& privkey, const CPubKey& vchPubKey, bool fSkipCheck);

    /** Create an ellswift-encoded public key for this key, with specified entropy.
     *
     *  entropy must be a 32-byte span with additional entropy to use in the encoding. Every
     *  public key has ~2^256 different encodings, and this function will deterministically pick
     *  one of them, based on entropy. Note that even without truly random entropy, the
     *  resulting encoding will be indistinguishable from uniform to any adversary who does not
     *  know the private key (because the private key itself is always used as entropy as well).
     */
    EllSwiftPubKey EllSwiftCreate(Span<const std::byte> entropy) const;

    /** Compute a BIP324-style ECDH shared secret.
     *
     *  - their_ellswift: EllSwiftPubKey that was received from the other side.
     *  - our_ellswift: EllSwiftPubKey that was sent to the other side (must have been generated
     *                  from *this using EllSwiftCreate()).
     *  - initiating: whether we are the initiating party (true) or responding party (false).
     */
    ECDHSecret ComputeBIP324ECDHSecret(const EllSwiftPubKey& their_ellswift,
                                       const EllSwiftPubKey& our_ellswift,
                                       bool initiating) const;
};

struct CExtKey {
    unsigned char nDepth;
    unsigned char vchFingerprint[4];
    unsigned int nChild;
    ChainCode chaincode;
    CKey key;

    friend bool operator==(const CExtKey& a, const CExtKey& b)
    {
        return a.nDepth == b.nDepth &&
            memcmp(a.vchFingerprint, b.vchFingerprint, sizeof(vchFingerprint)) == 0 &&
            a.nChild == b.nChild &&
            a.chaincode == b.chaincode &&
            a.key == b.key;
    }

    void Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const;
    void Decode(const unsigned char code[BIP32_EXTKEY_SIZE]);
    [[nodiscard]] bool Derive(CExtKey& out, unsigned int nChild) const;
    CExtPubKey Neuter() const;
    void SetSeed(Span<const std::byte> seed);
};

/** Initialize the elliptic curve support. May not be called twice without calling ECC_Stop first. */
void ECC_Start();

/** Deinitialize the elliptic curve support. No-op if ECC_Start wasn't called first. */
void ECC_Stop();

/** Check that required EC support is available at runtime. */
bool ECC_InitSanityCheck();

#endif // BITCOIN_KEY_H
