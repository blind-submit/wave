#ifndef DCF_HPP__
#define DCF_HPP__

#include "EzPC/FSS/src/fss.h"

namespace osuCrypto
{
    namespace details
    {
        block keyGenHelper(block key, block keyRcon)
        {
            keyRcon = _mm_shuffle_epi32(keyRcon, _MM_SHUFFLE(3, 3, 3, 3));
            key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
            key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
            key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
            return _mm_xor_si128(key, keyRcon);
        };

        template<>
        void AES<NI>::setKey(const block& userKey)
        {

            mRoundKey[0] = userKey;
            mRoundKey[1] = keyGenHelper(mRoundKey[0], _mm_aeskeygenassist_si128(mRoundKey[0], 0x01));
            mRoundKey[2] = keyGenHelper(mRoundKey[1], _mm_aeskeygenassist_si128(mRoundKey[1], 0x02));
            mRoundKey[3] = keyGenHelper(mRoundKey[2], _mm_aeskeygenassist_si128(mRoundKey[2], 0x04));
            mRoundKey[4] = keyGenHelper(mRoundKey[3], _mm_aeskeygenassist_si128(mRoundKey[3], 0x08));
            mRoundKey[5] = keyGenHelper(mRoundKey[4], _mm_aeskeygenassist_si128(mRoundKey[4], 0x10));
            mRoundKey[6] = keyGenHelper(mRoundKey[5], _mm_aeskeygenassist_si128(mRoundKey[5], 0x20));
            mRoundKey[7] = keyGenHelper(mRoundKey[6], _mm_aeskeygenassist_si128(mRoundKey[6], 0x40));
            mRoundKey[8] = keyGenHelper(mRoundKey[7], _mm_aeskeygenassist_si128(mRoundKey[7], 0x80));
            mRoundKey[9] = keyGenHelper(mRoundKey[8], _mm_aeskeygenassist_si128(mRoundKey[8], 0x1B));
            mRoundKey[10] = keyGenHelper(mRoundKey[9], _mm_aeskeygenassist_si128(mRoundKey[9], 0x36));
        }
    }
    void PRNG::SetSeed(const block& seed, u64 bufferSize)
    {
        mAes.setKey(seed);
        mBlockIdx = 0;

        if (mBuffer.size() == 0)
        {
            mBuffer.resize(bufferSize);
            mBufferByteCapacity = (sizeof(block) * bufferSize);
        }

        refillBuffer();
    }
}

template <std::size_t bits,
          typename PeerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_make_dcf(PeerT & peer0, PeerT & peer1, ExecutorT work_executor, dpf::modint<bits> x, dpf::modint<bits> y, CompletionToken && token)
{
    #include <asio/yield.hpp>
    return ::asio::async_compose<
        CompletionToken, void(::asio::error_code,  // error status
                              std::size_t,         // bytes_written0
                              std::size_t)>(       // bytes_written1
            [
                &peer0,
                &peer1,
                work_executor,
                x,y,
                keys = std::make_shared<std::pair<DCFKeyPack, DCFKeyPack>>(),
                bytes_written0 = std::size_t(0),
                bytes_written1 = std::size_t(0),
                coro = ::asio::coroutine()
            ]
            (
                auto & self,
                const ::asio::error_code & error = {},
                std::size_t bytes_just_written = 0
            )
            mutable
            {
                reenter (coro)
                {
                    yield dpf::asio::async_post(work_executor, [keys,x,y,&self]()
                    {
                        *keys = keyGenDCF(bits, bits, GroupElement(x.reduced_value(), bits), GroupElement(y.reduced_value(), bits));
                    }, std::move(self));

                    yield ::asio::async_write(peer0, std::array<asio::const_buffer, 6>{
                        asio::buffer(&keys->first.Bin, sizeof(int)),
                        asio::buffer(&keys->first.Bout, sizeof(int)),
                        asio::buffer(&keys->first.groupSize, sizeof(int)),
                        asio::buffer(keys->first.k, (keys->first.Bin + 1) * sizeof(osuCrypto::block)),
                        asio::buffer(keys->first.g, keys->first.groupSize * sizeof(GroupElement)),
                        asio::buffer(keys->first.v, keys->first.Bin * keys->first.groupSize * sizeof(GroupElement))
                    }, std::move(self));

                    bytes_written0 += bytes_just_written;

                    if (error) asio::detail::throw_error(error, "async_write");

                    yield ::asio::async_write(peer1, std::array<asio::const_buffer, 6>{
                        asio::buffer(&keys->second.Bin, sizeof(int)),
                        asio::buffer(&keys->second.Bout, sizeof(int)),
                        asio::buffer(&keys->second.groupSize, sizeof(int)),
                        asio::buffer(keys->second.k, (keys->second.Bin + 1) * sizeof(osuCrypto::block)),
                        asio::buffer(keys->second.g, keys->second.groupSize * sizeof(GroupElement)),
                        asio::buffer(keys->second.v, keys->second.Bin * keys->second.groupSize * sizeof(GroupElement))
                    }, std::move(self));

                    bytes_written1 += bytes_just_written;

                    if (error) asio::detail::throw_error(error, "async_write");

                    self.complete(error, bytes_written0, bytes_written1);
                }
            },
        token, peer0, peer1, work_executor);
    #include <asio/unyield.hpp>
}

template <std::size_t bits,
          typename DealerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_read_dcf(DealerT & dealer, ExecutorT work_executor, CompletionToken && token)
{
    #include <asio/yield.hpp>
    return ::asio::async_compose<
        CompletionToken, void(::asio::error_code,  // error status
                              DCFKeyPack,          // DCF key
                              std::size_t)>(       // bytes_read
            [
                &dealer,
                work_executor,
                key = std::make_shared<DCFKeyPack>(),
                bytes_read = std::size_t(0),
                coro = ::asio::coroutine()
            ]
            (
                auto & self,
                const ::asio::error_code & error = {},
                std::size_t bytes_just_read = 0
            )
            mutable
            {
                reenter (coro)
                {
                    yield ::asio::async_read(dealer, std::array<asio::mutable_buffer, 3>
                    {
                        asio::buffer(&key->Bin, sizeof(int)),
                        asio::buffer(&key->Bout, sizeof(int)),
                        asio::buffer(&key->groupSize, sizeof(int))
                    }, std::move(self));

                    bytes_read += bytes_just_read;

                    if (error) asio::detail::throw_error(error, "async_read");

                    yield dpf::asio::async_post(work_executor, [key]()
                    {
                        key->k = new block[key->Bin + 1];
                        key->g = new GroupElement[key->groupSize];
                        key->v = new GroupElement[key->Bin * key->groupSize];
                    }, std::move(self));

                    yield ::asio::async_read(dealer, std::array<asio::mutable_buffer, 3>
                    {
                        asio::buffer(key->k, (key->Bin + 1) * sizeof(osuCrypto::block)),
                        asio::buffer(key->g, key->groupSize * sizeof(GroupElement)),
                        asio::buffer(key->v, key->Bin * key->groupSize * sizeof(GroupElement))
                    }, std::move(self));

                    bytes_read += bytes_just_read;

                    if (error) asio::detail::throw_error(error, "async_read");

                    self.complete(error, *key, bytes_read);
                }
            },
        token, dealer, work_executor);
    #include <asio/unyield.hpp>
}

#endif  // DCF_HPP__