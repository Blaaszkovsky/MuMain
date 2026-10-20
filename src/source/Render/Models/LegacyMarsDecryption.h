#pragma once
#include <cryptopp/mars.h>

namespace Render::Models
{
// Asset compatibility variant: only the MARS key-word mask changes.
// Round processing/S-box stay in Crypto++; see docs/bmd-third-party-notices.md.
class LegacyMarsDecryption final : public CryptoPP::MARS::Decryption
{
protected:
    void UncheckedSetKey(const CryptoPP::byte* key, unsigned length,
                         const CryptoPP::NameValuePairs& parameters) override;
};
} // namespace Render::Models
