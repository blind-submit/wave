#ifndef BIOR_HPP__
#define BIOR_HPP__

#include "dcf.hpp"

output_type * scaled_lut2;

template <std::size_t bits,
          std::size_t j,
          std::size_t n,
          typename PeerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_make_beaver_bior(PeerT & peer0, PeerT & peer1, ExecutorT work_executor, CompletionToken && token)
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
                bvr0 = std::make_shared<std::array<dpf::modint<bits>, 8>>(),
                bvr1 = std::make_shared<std::array<dpf::modint<bits>, 8>>(),
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
                    yield dpf::asio::async_post(work_executor, [bvr0,bvr1,&self]()
                    {
                        auto U = dpf::uniform_sample<dpf::modint<bits>>();
                        std::tie((*bvr0)[0], (*bvr1)[0]) = dpf::additively_share(U);
                        auto A = dpf::uniform_sample<dpf::modint<bits>>();
                        std::tie((*bvr0)[1], (*bvr1)[1]) = dpf::additively_share(A);
                        auto B = dpf::uniform_sample<dpf::modint<bits>>();
                        std::tie((*bvr0)[2], (*bvr1)[2]) = dpf::additively_share(B);
                        auto X = dpf::uniform_sample<dpf::modint<bits>>();
                        std::tie((*bvr0)[3], (*bvr1)[3]) = dpf::additively_share(X);

                        std::tie((*bvr0)[4], (*bvr1)[4]) = dpf::additively_share(A*X-B);
                        std::tie((*bvr0)[5], (*bvr1)[5]) = dpf::additively_share(U*X);
                        std::tie((*bvr0)[6], (*bvr1)[6]) = dpf::additively_share(U*A);
                        std::tie((*bvr0)[7], (*bvr1)[7]) = dpf::additively_share(U*(A*X-B));
                    }, std::move(self));

                    yield ::asio::async_write(peer0, asio::buffer(*bvr0, sizeof(*bvr0)), std::move(self));

                    bytes_written0 += bytes_just_written;

                    if (error) asio::detail::throw_error(error, "async_write");

                    yield ::asio::async_write(peer1, asio::buffer(*bvr1, sizeof(*bvr1)), std::move(self));

                    bytes_written1 += bytes_just_written;

                    if (error) asio::detail::throw_error(error, "async_write");

                    self.complete(error, bytes_written0, bytes_written1);
                }
            },
        token, peer0, peer1, work_executor);
    #include <asio/unyield.hpp>
}

template <std::size_t bits,
          std::size_t j,
          std::size_t n,
          typename DealerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_read_beaver_bior_inner(DealerT & dealer, ExecutorT work_executor, CompletionToken && token)
{
    #include <asio/yield.hpp>
    return ::asio::async_compose<
        CompletionToken, void(::asio::error_code,  // error status
                              std::array<dpf::modint<bits>, 8>, // beaver triple
                              std::size_t)>(       // bytes_read
            [
                &dealer,
                work_executor,
                bvr = std::make_shared<std::array<dpf::modint<bits>, 8>>(),
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
                    yield ::asio::async_read(dealer, asio::buffer(*bvr, sizeof(*bvr)), std::move(self));

                    bytes_read += bytes_just_read;

                    if (error) asio::detail::throw_error(error, "async_read");

                    self.complete(error, *bvr, bytes_read);
                }
            },
        token, dealer, work_executor);
    #include <asio/unyield.hpp>
}

struct beaver
{
  public:
    beaver()
      : sign{0},
        inner_product0{0},
        inner_product1{0},
        coefficient{0},
        //
        sign_blind{0},
        inner_product0_blind{0},
        inner_product1_blind{0},
        coefficient_blind{0},
        //
        correction_coeff_inner0_minus_inner1{0},
        correction_sign_coeff{0},
        correction_sign_inner0{0},
        correction_sign_coeff_inner0_minus_inner1{0},
        //
        blinded_sign{0},
        blinded_inner_product0{0},
        blinded_inner_product1{0},
        blinded_coefficient{0},
        //
        blinded_sign2{0},
        blinded_inner_product02{0},
        blinded_inner_product12{0},
        blinded_coefficient2{0},
        //
        y_{0},
        //
        beaver_state_(beaver_status::notstarted),
        ready_{false} { }
    beaver(beaver &&) = default;
    beaver(const beaver &) = default;

    auto get_blinded_operands()
    {
        if (HEDLEY_UNLIKELY(beaver_state_ != beaver_status::notstarted))
        {
            throw std::runtime_error("invalid state transition");
        }
        beaver_state_ = beaver_status::blinding;
        blinded_sign = sign + sign_blind;
        blinded_inner_product0 = inner_product0 + inner_product0_blind;
        blinded_inner_product1 = inner_product1 + inner_product1_blind;
        blinded_coefficient = coefficient + coefficient_blind;
        return std::make_tuple(blinded_sign, blinded_inner_product0, blinded_inner_product1, blinded_coefficient);
    }

    auto do_evaluation()
    {
        beaver_status blinding = beaver_status::blinding;
        if (HEDLEY_UNLIKELY(beaver_state_ != beaver_status::blinding))
        {
            throw std::runtime_error("invalid state transition");
        }
        beaver_state_ = beaver_status::ready;
        ready_ = true;

        auto aa = blinded_inner_product0 + blinded_inner_product02;
        auto xx = blinded_coefficient + blinded_coefficient2;
        auto uu = blinded_sign + blinded_sign2;
        auto bb = blinded_inner_product1 + blinded_inner_product12;

        y_ =  party_ ? (aa*xx+bb) : 0;
        y_ = uu*(y_ + aa*coefficient_blind - xx*inner_product0_blind + correction_coeff_inner0_minus_inner1) - (aa*xx+bb)*sign_blind + aa*correction_sign_coeff + xx*correction_sign_inner0 - correction_sign_coeff_inner0_minus_inner1;

        return y_;
    }

    HEDLEY_INLINE
    auto operator()() const
    {
        if (HEDLEY_UNLIKELY(!ready_))
        {
            throw std::runtime_error("beaver not ready");
        }
        return y_;
    }

    dpf::modint<L> sign;
    dpf::modint<L> inner_product0;
    dpf::modint<L> inner_product1;
    dpf::modint<L> coefficient;

//   private:
    dpf::modint<L> sign_blind;
    dpf::modint<L> inner_product0_blind;
    dpf::modint<L> inner_product1_blind;
    dpf::modint<L> coefficient_blind;

    dpf::modint<L> correction_coeff_inner0_minus_inner1;
    dpf::modint<L> correction_sign_coeff;
    dpf::modint<L> correction_sign_inner0;
    dpf::modint<L> correction_sign_coeff_inner0_minus_inner1;

    dpf::modint<L> blinded_sign;
    dpf::modint<L> blinded_inner_product0;
    dpf::modint<L> blinded_inner_product1;
    dpf::modint<L> blinded_coefficient;

    dpf::modint<L> blinded_sign2;
    dpf::modint<L> blinded_inner_product02;
    dpf::modint<L> blinded_inner_product12;
    dpf::modint<L> blinded_coefficient2;

    dpf::modint<L> y_;

    bool party_;

    enum class beaver_status : psnip_uint8_t { ready = 0, waiting = 1, blinding, notstarted = 2 };
    beaver_status beaver_state_;
    bool ready_;
};

template <std::size_t bits,
          std::size_t j,
          std::size_t n,
          typename DealerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_read_beaver_bior(DealerT & dealer, ExecutorT work_executor, CompletionToken && token)
{
    #include <asio/yield.hpp>
    return ::asio::async_compose<
        CompletionToken, void(::asio::error_code,  // error status
                              beaver,              // beaver tuple
                              std::size_t)>(       // bytes_read
            [
                &dealer,
                work_executor,
                bvr = std::make_shared<beaver>(),
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
                    yield ::asio::async_read(dealer, std::array<asio::mutable_buffer, 8>{
                        asio::buffer(&bvr->sign_blind, sizeof(bvr->sign_blind)),
                        asio::buffer(&bvr->inner_product0_blind, sizeof(bvr->inner_product0_blind)),
                        asio::buffer(&bvr->inner_product1_blind, sizeof(bvr->inner_product1_blind)),
                        asio::buffer(&bvr->coefficient_blind, sizeof(bvr->coefficient_blind)),
                        asio::buffer(&bvr->correction_coeff_inner0_minus_inner1, sizeof(bvr->correction_coeff_inner0_minus_inner1)),
                        asio::buffer(&bvr->correction_sign_coeff, sizeof(bvr->correction_sign_coeff)),
                        asio::buffer(&bvr->correction_sign_inner0, sizeof(bvr->correction_sign_inner0)),
                        asio::buffer(&bvr->correction_sign_coeff_inner0_minus_inner1, sizeof(bvr->correction_sign_coeff_inner0_minus_inner1)),
                    }, std::move(self));

                    bytes_read += bytes_just_read;

                    if (error) asio::detail::throw_error(error, "async_read");

                    self.complete(error, *bvr, bytes_read);
                }
            },
        token, dealer, work_executor);
    #include <asio/unyield.hpp>
}

template <std::size_t bits,
          std::size_t j,
          std::size_t n,
          typename PeerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_make_preprocess_bior(PeerT & peer0, PeerT & peer1, ExecutorT work_executor, std::size_t count, CompletionToken && token)
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
                r = std::make_shared<dpf::modint<bits>>(),
                rshare = std::make_shared<std::pair<dpf::modint<bits>, dpf::modint<bits>>>(),
                rrshare = std::make_shared<std::pair<dpf::modint<bits>, dpf::modint<bits>>>(),
                bytes_written0 = std::size_t(0),
                bytes_written1 = std::size_t(0),
                count,
                iter = std::size_t(0),
                coro = ::asio::coroutine()
            ]
            (
                auto & self,
                const ::asio::error_code & error = {},
                std::size_t bytes_just_written0 = 0,
                std::size_t bytes_just_written1 = 0,
                std::size_t = 0
            )
            mutable
            {
                reenter (coro)
                {
                    while (iter++ < count)
                    {
                        yield dpf::asio::async_post(work_executor, [r,rshare,rrshare,&self]()
                        {
                            *r = dpf::uniform_sample<dpf::modint<bits>>();
                            *rshare = dpf::additively_share(*r);
                            *rrshare = dpf::additively_share((r->reduced_value() >> L-n) % (1ul << j));
                        }, std::move(self));

                        yield asio::async_write(peer0, std::array<asio::const_buffer, 2>
                        {
                            asio::buffer(&rshare->first, sizeof(rshare->first)),
                            asio::buffer(&rrshare->first, sizeof(rrshare->first))
                        }, std::move(self));
                        bytes_written0 += bytes_just_written0;
                        if (error) asio::detail::throw_error(error, "async_write(r0,rr0)");

                        yield asio::async_write(peer1, std::array<asio::const_buffer, 2>
                        {
                            asio::buffer(&rshare->second, sizeof(rshare->second)),
                            asio::buffer(&rrshare->second, sizeof(rrshare->second))
                        }, std::move(self));
                        bytes_written1 += bytes_just_written0;
                        if (error) asio::detail::throw_error(error, "async_write(r1,rr1)");

                        yield dpf::asio::async_make_dpf(peer0, peer1, work_executor,
                            dpf::make_dpfargs(dpf::wildcard_value<dpf::modint<bits>>(*r)), std::move(self));
                        bytes_written0 += bytes_just_written0;
                        bytes_written1 += bytes_just_written1;
                        if (error) asio::detail::throw_error(error, "async_make_dpf");

                        yield async_make_dcf(peer0, peer1, work_executor, dpf::modint<L-n>(*r), dpf::modint<L-n>(1), std::move(self));
                        bytes_written0 += bytes_just_written0;
                        bytes_written1 += bytes_just_written1;
                        if (error) asio::detail::throw_error(error, "async_make_dcf");

                        yield async_make_dcf(peer0, peer1, work_executor, dpf::modint<L-n+j>(*r), dpf::modint<L-n+j>(1ul<<j), std::move(self));
                        bytes_written0 += bytes_just_written0;
                        bytes_written1 += bytes_just_written1;
                        if (error) asio::detail::throw_error(error, "async_make_dcf");

                        yield async_make_beaver_bior<L,j,n>(peer0, peer1, work_executor, std::move(self));
                        bytes_written0 += bytes_just_written0;
                        bytes_written1 += bytes_just_written1;
                        if (error) asio::detail::throw_error(error, "async_make_beaver_bior");
                    }
                    self.complete(error, bytes_written0, bytes_written1);
                }
            },
        token, peer0, peer1, work_executor);
    #include <asio/unyield.hpp>
}

template <std::size_t bits,
          std::size_t j,
          std::size_t n,
          typename DealerT,
          typename ExecutorT>
struct read_preprocess_bior_coro : asio::coroutine
{
#include <asio/yield.hpp>
    using dpf_type = DEDUCE_DPF_TYPE_T(dpf::wildcard_value<dpf::modint<bits>>{});
    using correction_words_array = typename dpf_type::correction_words_array;
    using correction_advice_array = typename dpf_type::correction_advice_array;
    using interior_node = typename dpf_type::interior_node;
    using leaf_tuple = typename dpf_type::leaf_tuple;
    using beaver_tuple = typename dpf_type::beaver_tuple;
    using input_type = typename dpf_type::input_type;

    using dpf_priv_values = std::tuple<interior_node, leaf_tuple, beaver_tuple, input_type>;
    using dpf_values = std::tuple<correction_words_array, correction_advice_array, dpf_priv_values>;
  public:
    read_preprocess_bior_coro(DealerT & dealer, asio::stream_file & outfile,
        ExecutorT work_executor, std::size_t count)
      : dealer_{dealer}, outfile_{outfile}, work_executor_{work_executor},
        r_{std::make_shared<dpf::modint<L>>(0)}, rr_{std::make_shared<dpf::modint<L>>(0)},
        count_{count},
        bytes_read_{0}, bytes_written_{0} { }

    template <typename Self>
    void operator()(Self & self,
                    const asio::error_code & error,
                    std::size_t bytes_just_read,
                    dpf_values dpf)
    {
        bytes_read_ += bytes_just_read;
        dpf_ = std::make_unique<dpf_values>(dpf);
        (*this)(self, error);
    }

    template <typename Self>
    void operator()(Self & self,
                    const asio::error_code & error,
                    DCFKeyPack dcf,
                    std::size_t bytes_just_read)
    {
        bytes_read_ += bytes_just_read;
        if (dcf_lo_) dcf_hi_ = std::make_shared<DCFKeyPack>(dcf);
        else dcf_lo_ = std::make_shared<DCFKeyPack>(dcf);
        (*this)(self, error);
    }

    template <typename Self>
    void operator()(Self & self,
                    const asio::error_code & error,
                    std::array<dpf::modint<bits>, 8> bvr,
                    std::size_t bytes_just_read)
    {
        bytes_read_ += bytes_just_read;
        bvr_ = std::make_unique<std::array<dpf::modint<bits>, 8>>(bvr);
        (*this)(self, error);
    }

    template <typename Self>
    void operator()(Self & self,
                    const asio::error_code & error = {},
                    std::size_t bytes_just_written = 0)
    {
        reenter(*this)
        {
            while (count_--)
            {
                yield dpf::asio::async_read_dpf_inner<dpf_type>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_read_dpf_inner");

                yield asio::async_write(outfile_, asio::buffer(&*dpf_, sizeof(*dpf_)), std::move(self));
                bytes_written_ += bytes_just_written;
                if (error) asio::detail::throw_error(error, "async_write(dpf)");

                yield asio::async_read(dealer_, std::array<asio::mutable_buffer, 2>{
                    asio::buffer(&*r_, sizeof(*r_)),
                    asio::buffer(&*rr_, sizeof(*rr_))
                }, std::move(self));
                bytes_read_ += bytes_just_written;
                if (error) asio::detail::throw_error(error, "async_read(r,rr)");

                yield asio::async_write(outfile_, std::array<asio::const_buffer, 2>
                {
                    asio::buffer(&*r_, sizeof(*r_)),
                    asio::buffer(&*rr_, sizeof(*rr_))
                }, std::move(self));
                bytes_written_ += bytes_just_written;
                if (error) asio::detail::throw_error(error, "async_write(r,rr)");

                yield async_read_dcf<bits-n-j>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_read_dcf(lo)");

                yield asio::async_write(outfile_, std::array<asio::const_buffer, 6>
                {
                    asio::buffer(&dcf_lo_->Bin, sizeof(int)),
                    asio::buffer(&dcf_lo_->Bout, sizeof(int)),
                    asio::buffer(&dcf_lo_->groupSize, sizeof(int)),
                    asio::buffer(dcf_lo_->k, (dcf_lo_->Bin + 1) * sizeof(osuCrypto::block)),
                    asio::buffer(dcf_lo_->g, dcf_lo_->groupSize * sizeof(GroupElement)),
                    asio::buffer(dcf_lo_->v, dcf_lo_->Bin * dcf_lo_->groupSize * sizeof(GroupElement))
                }, std::move(self));
                bytes_written_ += bytes_just_written;
                if (error) asio::detail::throw_error(error, "async_write(dcf_lo)");

                yield async_read_dcf<bits-n>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_read_dcf(hi)");

                yield asio::async_write(outfile_, std::array<asio::const_buffer, 6>
                {
                    asio::buffer(&dcf_hi_->Bin, sizeof(int)),
                    asio::buffer(&dcf_hi_->Bout, sizeof(int)),
                    asio::buffer(&dcf_hi_->groupSize, sizeof(int)),
                    asio::buffer(dcf_hi_->k, (dcf_hi_->Bin + 1) * sizeof(osuCrypto::block)),
                    asio::buffer(dcf_hi_->g, dcf_hi_->groupSize * sizeof(GroupElement)),
                    asio::buffer(dcf_hi_->v, dcf_hi_->Bin * dcf_hi_->groupSize * sizeof(GroupElement))
                }, std::move(self));
                bytes_written_ += bytes_just_written;
                if (error) asio::detail::throw_error(error, "async_write(dcf_lo)");

                yield async_read_beaver_bior_inner<L,j,n>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_make_beaver_bior");

                yield asio::async_write(outfile_, asio::buffer(&*bvr_, sizeof(*bvr_)), std::move(self));
                bytes_written_ += bytes_just_written;
                if (error) asio::detail::throw_error(error, "async_write(beaver)");
            }

            self.complete(error, bytes_read_, bytes_written_);
        }
    }

  private:
    DealerT & dealer_;
    asio::stream_file & outfile_;
    ExecutorT work_executor_;
    std::size_t count_;
    std::size_t bytes_read_, bytes_written_;
    std::shared_ptr<dpf::modint<L>> r_, rr_;
    std::shared_ptr<dpf_values> dpf_;
    std::shared_ptr<DCFKeyPack> dcf_lo_;
    std::shared_ptr<DCFKeyPack> dcf_hi_;
    std::shared_ptr<std::array<dpf::modint<bits>, 8>> bvr_;
#include <asio/unyield.hpp>
};

template <std::size_t bits,
          std::size_t j,
          std::size_t n,
          typename DealerT,
          typename OutfileT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_read_preprocess_bior(DealerT & dealer, OutfileT & outfile, ExecutorT work_executor, std::size_t count, CompletionToken && token)
{
    return asio::async_compose<CompletionToken,
        void(asio::error_code,
             std::size_t,
             std::size_t)>(read_preprocess_bior_coro<bits, j, n, DealerT, ExecutorT>{dealer, outfile, work_executor, count},
        token, dealer, outfile, work_executor);
}

template <std::size_t bits,
          std::size_t j,
          std::size_t n,
          typename DealerT,
          typename PeerT,
          typename ExecutorT>
struct mult_coro : asio::coroutine
{
#include <asio/yield.hpp>
  public:
    mult_coro(DealerT & dealer, PeerT & peer,
        ExecutorT work_executor, std::shared_ptr<beaver> bvr)
      : dealer_{dealer}, peer_{peer}, work_executor_{work_executor},
        bvr_{std::move(bvr)},
        bytes_read_{0}, bytes_written_{0} { }

    template <typename Self>
    void operator()(Self & self,
                    const asio::error_code & error = {},
                    std::size_t bytes_just_transmitted = 0)
    {
        reenter(*this)
        {
            yield dpf::asio::async_post(work_executor_, [bvr_ = this->bvr_]()
            {
                bvr_->get_blinded_operands();
            }, std::move(self));

            yield ::asio::async_write(peer_,
                std::array<asio::const_buffer, 4>{
                    asio::buffer(&bvr_->blinded_sign, sizeof(bvr_->blinded_sign)),
                    asio::buffer(&bvr_->blinded_inner_product0, sizeof(bvr_->blinded_inner_product0)),
                    asio::buffer(&bvr_->blinded_inner_product1, sizeof(bvr_->blinded_inner_product1)),
                    asio::buffer(&bvr_->blinded_coefficient, sizeof(bvr_->blinded_coefficient)) },
                std::move(self));
            bytes_written_ += bytes_just_transmitted;
            if (error) asio::detail::throw_error(error, "async_write(blinded)");

            yield ::asio::async_read(peer_,
                std::array<asio::mutable_buffer, 4>{
                    asio::buffer(&bvr_->blinded_sign2, sizeof(bvr_->blinded_sign2)),
                    asio::buffer(&bvr_->blinded_inner_product02, sizeof(bvr_->blinded_inner_product02)),
                    asio::buffer(&bvr_->blinded_inner_product12, sizeof(bvr_->blinded_inner_product12)),
                    asio::buffer(&bvr_->blinded_coefficient2, sizeof(bvr_->blinded_coefficient2)) },
                std::move(self));
            bytes_read_ += bytes_just_transmitted;
            if (error) asio::detail::throw_error(error, "async_read(blinded)");

            yield dpf::asio::async_post(work_executor_, [bvr_ = this->bvr_]()
            {
                bvr_->do_evaluation();
            }, std::move(self));

            self.complete(error, (*bvr_)(), bytes_read_, bytes_written_);
        }
    }

  private:
    DealerT & dealer_;
    PeerT & peer_;
    ExecutorT work_executor_;
    std::shared_ptr<beaver> bvr_;
    std::size_t bytes_read_, bytes_written_;
#include <asio/unyield.hpp>
};

template <std::size_t bits,
          typename DealerT,
          typename PeerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_mult(DealerT & dealer, PeerT & peer, ExecutorT work_executor, std::shared_ptr<beaver> bvr, CompletionToken && token)
{
    return asio::async_compose<CompletionToken,
        void(asio::error_code,
            output_type output,
            std::size_t,
            std::size_t)>(mult_coro<bits, j, n, DealerT, PeerT, ExecutorT>{dealer, peer, work_executor, std::move(bvr)},
        token, dealer, peer, work_executor);
}

template <std::size_t bits,
          std::size_t j,
          std::size_t n,
          typename DealerT,
          typename PeerT,
          typename ExecutorT>
struct online_bior_coro : asio::coroutine
{
#include <asio/yield.hpp>
    using dpf_type = DEDUCE_DPF_TYPE_T(dpf::wildcard_value<dpf::modint<bits>>{});
    using correction_words_array = typename dpf_type::correction_words_array;
    using correction_advice_array = typename dpf_type::correction_advice_array;
    using interior_node = typename dpf_type::interior_node;
    using leaf_tuple = typename dpf_type::leaf_tuple;
    using beaver_tuple = typename dpf_type::beaver_tuple;
    using input_type = typename dpf_type::input_type;

    using dpf_priv_values = std::tuple<interior_node, leaf_tuple, beaver_tuple, input_type>;
    using dpf_values = std::tuple<correction_words_array, correction_advice_array, dpf_priv_values>;
  public:
    online_bior_coro(DealerT & dealer, PeerT & peer,
        ExecutorT work_executor, bool party, dpf::modint<L> input_share,
        std::size_t count)
      : dealer_{dealer}, peer_{peer}, work_executor_{work_executor},
        r_{std::make_shared<dpf::modint<L>>(0)}, rr_{std::make_shared<dpf::modint<L>>(0)},
        party_{party}, input_share_{input_share}, count_{count},
        dealer_bytes_read_{0}, peer_bytes_read_{0}, bytes_written_{0} { }

    template <typename Self>
    void operator()(Self & self,
                    const asio::error_code & error,
                    std::size_t bytes_just_read,
                    dpf_values dpf)
    {
        dealer_bytes_read_ += bytes_just_read;
        auto & [correction_words, correction_advice, priv] = dpf;
        auto & [root, leaves, beavers, offset_share] = priv;
        dpf_ = std::make_shared<dpf_type>(root, correction_words, correction_advice,
            leaves, beavers, offset_share);
        (*this)(self, error);
    }

    template <typename Self>
    void operator()(Self & self,
                    const asio::error_code & error,
                    DCFKeyPack dcf,
                    std::size_t bytes_just_read)
    {
        dealer_bytes_read_ += bytes_just_read;
        if (dcf_lo_) dcf_hi_ = std::make_unique<DCFKeyPack>(dcf);
        else dcf_lo_ = std::make_unique<DCFKeyPack>(dcf);
        (*this)(self, error);
    }

    template <typename Self>
    void operator()(Self & self,
                    const asio::error_code & error,
                    beaver bvr,
                    std::size_t bytes_just_read)
    {
        dealer_bytes_read_ += bytes_just_read;
        bvr_ = std::make_shared<beaver>(bvr);
        (*this)(self, error);
    }

    template <typename Self>
    void operator()(Self & self,
                    const asio::error_code & error,
                    dpf::modint<L> shifted_input,
                    std::size_t bytes_just_read,
                    std::size_t bytes_just_written)
    {
        peer_bytes_read_ += bytes_just_read;
        bytes_written_ += bytes_just_written;
        shifted_input_ = shifted_input;
        (*this)(self, error);
    }

    template <typename Self>
    void operator()(Self & self,
                    const asio::error_code & error = {},
                    std::size_t bytes_just_written = 0)
    {
//        static const std::array<dpf::modint<L>, 1ul<<J> endpoints = [&]()
//        {
//            std::array<dpf::modint<L>, 1ul<<J> eps;
//            for (int i = 0; i < (1ul<<J); ++i) eps[i]=i*twoj;
//            return eps;
//        }();

        reenter(*this)
        {
            while (count_--)
            {
                yield asio::async_read(dealer_, std::array<asio::mutable_buffer, 2>{
                    asio::buffer(&*r_, sizeof(*r_)),
                    asio::buffer(&*rr_, sizeof(*rr_))
                }, std::move(self));
                dealer_bytes_read_ += bytes_just_written;
                if (error) asio::detail::throw_error(error, "async_read(r,rr)");

                yield dpf::asio::async_read_dpf_inner<dpf_type>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_read_dpf_inner");

                yield async_read_dcf<bits-n-j>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_read_dcf(lo)");

                yield async_read_dcf<bits-n>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_read_dcf(hi)");

                yield async_read_beaver_bior<bits,j,n>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_make_beaver_Haar");

                yield dpf::asio::async_assign_wildcard_input(peer_, work_executor_, *dpf_, input_share_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_assign_wildcard_input");

                yield dpf::asio::async_post(work_executor_,
                [
                    // this,
                    party = this->party_,
                    shifted_input = this->shifted_input_.reduced_value(),
                    dcf_lo = this->dcf_lo_,
                    dcf_hi = this->dcf_hi_,
                    dpf = this->dpf_,
                    bvr = this->bvr_,
                    rr = this->rr_
                ]()
                {
                    /// compute [lsb_j(msb_n(a))
                    GroupElement carry{0};
                    GroupElement borrow{0};
                    evalDCF(party, &carry, GroupElement(shifted_input&(1ul<<(L-n)), L-n), *dcf_lo);
                    evalDCF(party, &borrow, GroupElement(shifted_input&(1ul<<(L-J)), L-J), *dcf_hi);
                    bvr->coefficient = *rr - ((shifted_input>>(L-n)&(1ul<<j)))-1+carry.value-borrow.value;
                    auto parities = grotto::segment_parities<n,J>(*dpf);
                    for (std::size_t i = 0; i < twoJ; ++i)
                    {
                        if (!parities[i]) continue;
                        bvr->inner_product0 += scaled_lut2[i];
                        bvr->inner_product1 += scaled_lut[i];
                        bvr->sign++;
                    }
                }, std::move(self));
                if (error) asio::detail::throw_error(error, "async_post");

                yield async_mult<bits>(dealer_, peer_, work_executor_, bvr_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_post");
            }

            self.complete(error, dealer_bytes_read_, peer_bytes_read_, bytes_written_);
        }
    }

  private:
    DealerT & dealer_;
    PeerT & peer_;
    ExecutorT work_executor_;
    bool party_;
    dpf::modint<L> input_share_;
    std::size_t count_;
    dpf::modint<L> shifted_input_;
    std::size_t dealer_bytes_read_, peer_bytes_read_, bytes_written_;
    std::shared_ptr<dpf::modint<L>> r_, rr_;
    std::shared_ptr<dpf_type> dpf_;
    std::shared_ptr<DCFKeyPack> dcf_lo_;
    std::shared_ptr<DCFKeyPack> dcf_hi_;
    std::shared_ptr<beaver> bvr_;
#include <asio/unyield.hpp>
};

template <std::size_t bits,
          std::size_t j,
          std::size_t n,
          typename DealerT,
          typename PeerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_online_bior(DealerT & dealer, PeerT & peer, ExecutorT work_executor, bool party, dpf::modint<L> input_share, std::size_t count, CompletionToken && token)
{
    return asio::async_compose<CompletionToken,
        void(asio::error_code,
             std::size_t,
             std::size_t,
             std::size_t)>(online_bior_coro<bits, j, n, DealerT, PeerT, ExecutorT>{dealer, peer, work_executor, party, input_share, count},
              token, dealer, peer, work_executor);
}

#endif  // BIOR_HPP__