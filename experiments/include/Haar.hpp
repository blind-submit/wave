#ifndef HAAR_HPP__
#define HAAR_HPP__

template <std::size_t bits,
          typename PeerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_make_beaver_Haar(PeerT & peer0, PeerT & peer1, ExecutorT work_executor, CompletionToken && token)
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
                bvr0 = std::make_shared<std::array<dpf::modint<bits>, 3>>(),
                bvr1 = std::make_shared<std::array<dpf::modint<bits>, 3>>(),
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
                        std::tie((*bvr0)[0], (*bvr1)[0]) = dpf::additively_share(dpf::uniform_sample<dpf::modint<bits>>());
                        std::tie((*bvr0)[1], (*bvr1)[1]) = dpf::additively_share(dpf::uniform_sample<dpf::modint<bits>>());
                        std::tie((*bvr0)[2], (*bvr1)[2]) = dpf::additively_share((*bvr0)[0]*(*bvr1)[1]+(*bvr0)[1]*(*bvr1)[0]);
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
          typename DealerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_read_beaver_Haar_inner(DealerT & dealer, ExecutorT work_executor, CompletionToken && token)
{
    #include <asio/yield.hpp>
    return ::asio::async_compose<
        CompletionToken, void(::asio::error_code,  // error status
                              std::array<dpf::modint<bits>, 3>, // beaver triple
                              std::size_t)>(       // bytes_read
            [
                &dealer,
                work_executor,
                bvr = std::make_shared<std::array<dpf::modint<bits>, 3>>(),
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

struct beaver_Haar
{
  public:
    beaver_Haar()
      : sign{0},
        inner_product{0},
        sign_blind{0},
        inner_product_blind{0},
        correction{0},
        blinded_sign{0},
        blinded_inner_product{0},
        blinded_sign2{0},
        blinded_inner_product2{0},
        y_{0},
        beaver_state_(beaver_status::notstarted),
        ready_{false} { }
    beaver_Haar(beaver_Haar &&) = default;
    beaver_Haar(const beaver_Haar &) = default;

    auto get_blinded_operands()
    {
        if (HEDLEY_UNLIKELY(beaver_state_ != beaver_status::notstarted))
        {
            throw std::runtime_error("invalid state transition");
        }
        beaver_state_ = beaver_status::blinding;
        blinded_sign = sign + sign_blind;
        blinded_inner_product = inner_product + inner_product_blind;
        return std::make_pair(blinded_sign, blinded_inner_product);
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
        y_ = (sign * (inner_product + blinded_inner_product2) - blinded_sign2 * blinded_inner_product + correction);

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

    dpf::modint<L> sign = 0;
    dpf::modint<L> inner_product = 0;

//   private:
    dpf::modint<L> sign_blind;
    dpf::modint<L> inner_product_blind;
    dpf::modint<L> correction;

    dpf::modint<L> blinded_sign;
    dpf::modint<L> blinded_inner_product;

    dpf::modint<L> blinded_sign2;
    dpf::modint<L> blinded_inner_product2;

    dpf::modint<L> y_;

    enum class beaver_status : psnip_uint8_t { ready = 0, waiting = 1, blinding, notstarted = 2 };
    beaver_status beaver_state_;
    bool ready_;
};

template <std::size_t bits,
          typename DealerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_read_beaver_Haar(DealerT & dealer, ExecutorT work_executor, CompletionToken && token)
{
    #include <asio/yield.hpp>
    return ::asio::async_compose<
        CompletionToken, void(::asio::error_code,  // error status
                              beaver_Haar,         // beaver triple
                              std::size_t)>(       // bytes_read
            [
                &dealer,
                work_executor,
                bvr = std::make_shared<beaver_Haar>(),
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
                    yield ::asio::async_read(dealer, std::array<asio::mutable_buffer, 3>{
                        asio::buffer(&bvr->sign_blind, sizeof(bvr->sign_blind)),
                        asio::buffer(&bvr->inner_product_blind, sizeof(bvr->inner_product_blind)),
                        asio::buffer(&bvr->correction, sizeof(bvr->correction))
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
          typename DealerT,
          typename PeerT,
          typename ExecutorT>
struct mult1x1_coro : asio::coroutine
{
#include <asio/yield.hpp>
  public:
    mult1x1_coro(DealerT & dealer, PeerT & peer,
        ExecutorT work_executor, std::shared_ptr<beaver_Haar> bvr)
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
                std::array<asio::const_buffer, 2>{
                    asio::buffer(&bvr_->blinded_sign, sizeof(bvr_->blinded_sign)),
                    asio::buffer(&bvr_->blinded_inner_product, sizeof(bvr_->blinded_inner_product)) },
                std::move(self));
            bytes_written_ += bytes_just_transmitted;
            if (error) asio::detail::throw_error(error, "async_write(blinded)");

            yield ::asio::async_read(peer_,
                std::array<asio::mutable_buffer, 2>{
                    asio::buffer(&bvr_->blinded_sign2, sizeof(bvr_->blinded_sign2)),
                    asio::buffer(&bvr_->blinded_inner_product2, sizeof(bvr_->blinded_inner_product2)) },
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
    std::shared_ptr<beaver_Haar> bvr_;
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
auto async_mult(DealerT & dealer, PeerT & peer, ExecutorT work_executor, std::shared_ptr<beaver_Haar> bvr, CompletionToken && token)
{
    return asio::async_compose<CompletionToken,
        void(asio::error_code,
            output_type output,
            std::size_t,
            std::size_t)>(mult1x1_coro<bits, DealerT, PeerT, ExecutorT>{dealer, peer, work_executor, std::move(bvr)},
        token, dealer, peer, work_executor);
}

template <std::size_t bits,
          typename PeerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_make_preprocess_Haar(PeerT & peer0, PeerT & peer1, ExecutorT work_executor, std::size_t count, CompletionToken && token)
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
                bytes_written0 = std::size_t(0),
                bytes_written1 = std::size_t(0),
                args = dpf::make_dpfargs(dpf::wildcard<dpf::modint<bits>>),
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
                        yield dpf::asio::async_make_dpf(peer0, peer1, work_executor, args, std::move(self));
                        bytes_written0 += bytes_just_written0;
                        bytes_written1 += bytes_just_written1;
                        if (error) asio::detail::throw_error(error, "async_make_dpf");

                        yield async_make_beaver_Haar<64>(peer0, peer1, work_executor, std::move(self));
                        bytes_written0 += bytes_just_written0;
                        bytes_written1 += bytes_just_written1;
                        if (error) asio::detail::throw_error(error, "async_make_beaver_Haar");
                    }

                    self.complete(error, bytes_written0, bytes_written1);
                }
            },
        token, peer0, peer1, work_executor);
    #include <asio/unyield.hpp>
}

template <std::size_t bits,
          typename DealerT,
          typename ExecutorT>
struct read_preprocess_Haar_coro : asio::coroutine
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
    read_preprocess_Haar_coro(DealerT & dealer, asio::stream_file & outfile,
        ExecutorT work_executor, std::size_t count)
      : dealer_{dealer}, outfile_{outfile}, work_executor_{work_executor},
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
                    std::array<dpf::modint<bits>, 3> bvr,
                    std::size_t bytes_just_read)
    {
        bytes_read_ += bytes_just_read;
        bvr_ = std::make_unique<std::array<dpf::modint<bits>, 3>>(bvr);
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
                if (error) asio::detail::throw_error(error, "async_make_dpf_inner");

                yield asio::async_write(outfile_, asio::buffer(&*dpf_, sizeof(*dpf_)), std::move(self));
                bytes_written_ += bytes_just_written;
                if (error) asio::detail::throw_error(error, "async_write(dpf)");

                yield async_read_beaver_Haar_inner<64>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_make_beaver_Haar");

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
    std::unique_ptr<dpf_values> dpf_;
    std::unique_ptr<std::array<dpf::modint<bits>, 3>> bvr_;
#include <asio/unyield.hpp>
};

template <std::size_t bits,
          typename DealerT,
          typename OutfileT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_read_preprocess_Haar(DealerT & dealer, OutfileT & outfile, ExecutorT work_executor, std::size_t count, CompletionToken && token)
{
    return asio::async_compose<CompletionToken,
        void(asio::error_code,
             std::size_t,
             std::size_t)>(read_preprocess_Haar_coro<bits, DealerT, ExecutorT>{dealer, outfile, work_executor, count},
        token, dealer, outfile, work_executor);
}

template <std::size_t bits,
          typename DealerT,
          typename PeerT,
          typename ExecutorT>
struct online_Haar_coro : asio::coroutine
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
    online_Haar_coro(DealerT & dealer, PeerT & peer,
        ExecutorT work_executor, dpf::modint<L> input_share, std::size_t count)
      : dealer_{dealer}, peer_{peer}, work_executor_{work_executor},
        input_share_{input_share}, count_{count},
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
                    beaver_Haar bvr,
                    std::size_t bytes_just_read)
    {
        dealer_bytes_read_ += bytes_just_read;
        bvr_ = std::make_shared<beaver_Haar>(bvr);
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
//            for (size_t i = 0; i < (1ul<<J); ++i) eps[i]=i*twoj;
//            return eps;
//        }();

        reenter(*this)
        {
            while (count_--)
            {
                yield dpf::asio::async_read_dpf_inner<dpf_type>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_read_dpf_inner");

                yield async_read_beaver_Haar<64>(dealer_, work_executor_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_make_beaver_Haar");

                yield dpf::asio::async_assign_wildcard_input(peer_, work_executor_, *dpf_, input_share_, std::move(self));
                if (error) asio::detail::throw_error(error, "async_assign_wildcard_input");

                yield dpf::asio::async_post(work_executor_, [bvr = this->bvr_, dpf = this->dpf_]()mutable
                {
                    auto parities = grotto::segment_parities<n,J>(*dpf);
                    for (std::size_t i = 0; i < twoJ; ++i)
                    {
                        if (!parities[i]) continue;
                        bvr->inner_product += scaled_lut[i];
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
    dpf::modint<L> input_share_;
    std::size_t count_;
    std::size_t dealer_bytes_read_, peer_bytes_read_, bytes_written_;
    std::shared_ptr<dpf_type> dpf_;
    std::shared_ptr<beaver_Haar> bvr_;
#include <asio/unyield.hpp>
};

template <std::size_t bits,
          typename DealerT,
          typename PeerT,
          typename ExecutorT,
          typename CompletionToken>
[[nodiscard]]
HEDLEY_ALWAYS_INLINE
auto async_online_Haar(DealerT & dealer, PeerT & peer, ExecutorT work_executor, dpf::modint<L> input_share, std::size_t count, CompletionToken && token)
{
    return asio::async_compose<CompletionToken,
        void(asio::error_code,
             std::size_t,
             std::size_t,
             std::size_t)>(online_Haar_coro<bits, DealerT, PeerT, ExecutorT>{dealer, peer, work_executor, input_share, count},
              token, dealer, peer, work_executor);
}

#endif