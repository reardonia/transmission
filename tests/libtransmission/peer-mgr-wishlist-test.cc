// This file Copyright (C) 2021-2022 Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <cstddef> // size_t
#include <map>
#include <memory>
#include <set>

#define LIBTRANSMISSION_PEER_MODULE

#include <libtransmission/transmission.h>

#include <libtransmission/bitfield.h>
#include <libtransmission/peer-mgr-wishlist.h>

#include "gtest/gtest.h"

class PeerMgrWishlistTest : public ::testing::Test
{
protected:
    struct MockMediator final : public Wishlist::Mediator
    {
        mutable std::map<tr_block_index_t, size_t> active_request_count_;
        mutable std::map<tr_piece_index_t, size_t> missing_block_count_;
        mutable std::map<tr_piece_index_t, tr_block_span_t> block_span_;
        mutable std::map<tr_piece_index_t, tr_priority_t> piece_priority_;
        mutable std::map<tr_piece_index_t, size_t> piece_replication_;
        mutable std::set<tr_block_index_t> client_has_block_;
        mutable std::set<tr_piece_index_t> client_wants_piece_;
        tr_piece_index_t piece_count_ = 0;
        bool is_endgame_ = false;
        bool is_sequential_download_ = false;

        PeerMgrWishlistTest& parent_;

        explicit MockMediator(PeerMgrWishlistTest& parent)
            : parent_{ parent }
        {
        }

        [[nodiscard]] bool client_has_block(tr_block_index_t block) const override
        {
            return client_has_block_.count(block) != 0;
        }

        [[nodiscard]] bool client_wants_piece(tr_piece_index_t piece) const override
        {
            return client_wants_piece_.count(piece) != 0;
        }

        [[nodiscard]] bool is_endgame() const override
        {
            return is_endgame_;
        }

        [[nodiscard]] bool is_sequential_download() const override
        {
            return is_sequential_download_;
        }

        [[nodiscard]] size_t count_active_requests(tr_block_index_t block) const override
        {
            return active_request_count_[block];
        }

        [[nodiscard]] size_t count_missing_blocks(tr_piece_index_t piece) const override
        {
            return missing_block_count_[piece];
        }

        [[nodiscard]] size_t count_piece_replication(tr_piece_index_t piece) const override
        {
            return piece_replication_[piece];
        }

        [[nodiscard]] tr_block_span_t block_span(tr_piece_index_t piece) const override
        {
            return block_span_[piece];
        }

        [[nodiscard]] tr_piece_index_t piece_count() const override
        {
            return piece_count_;
        }

        [[nodiscard]] tr_priority_t priority(tr_piece_index_t piece) const final
        {
            return piece_priority_[piece];
        }

        [[nodiscard]] libtransmission::ObserverTag observe_peer_disconnect(
            libtransmission::SimpleObservable<tr_torrent*, tr_bitfield const&>::Observer observer) override
        {
            return parent_.peer_disconnect_.observe(std::move(observer));
        }

        [[nodiscard]] libtransmission::ObserverTag observe_got_bitfield(
            libtransmission::SimpleObservable<tr_torrent*, tr_bitfield const&>::Observer observer) override
        {
            return parent_.got_bitfield_.observe(std::move(observer));
        }

        [[nodiscard]] libtransmission::ObserverTag observe_got_block(
            libtransmission::SimpleObservable<tr_torrent*, tr_piece_index_t, tr_block_index_t>::Observer observer) override
        {
            return parent_.got_block_.observe(std::move(observer));
        }

        [[nodiscard]] libtransmission::ObserverTag observe_got_have(
            libtransmission::SimpleObservable<tr_torrent*, tr_piece_index_t>::Observer observer) override
        {
            return parent_.got_have_.observe(std::move(observer));
        }

        [[nodiscard]] libtransmission::ObserverTag observe_got_have_all(
            libtransmission::SimpleObservable<tr_torrent*>::Observer observer) override
        {
            return parent_.got_have_all_.observe(std::move(observer));
        }

        [[nodiscard]] libtransmission::ObserverTag observe_piece_completed(
            libtransmission::SimpleObservable<tr_torrent*, tr_piece_index_t>::Observer observer) override
        {
            return parent_.piece_completed_.observe(std::move(observer));
        }

        [[nodiscard]] libtransmission::ObserverTag observe_priority_changed(
            libtransmission::SimpleObservable<tr_torrent*, tr_file_index_t const*, tr_file_index_t, tr_priority_t>::Observer
                observer) override
        {
            return parent_.priority_changed_.observe(std::move(observer));
        }

        [[nodiscard]] libtransmission::ObserverTag observe_sequential_download_changed(
            libtransmission::SimpleObservable<tr_torrent*, bool>::Observer observer) override
        {
            return parent_.sequential_download_changed_.observe(std::move(observer));
        }
    };

    libtransmission::SimpleObservable<tr_torrent*, tr_bitfield const&> peer_disconnect_;
    libtransmission::SimpleObservable<tr_torrent*, tr_bitfield const&> got_bitfield_;
    libtransmission::SimpleObservable<tr_torrent*, tr_piece_index_t, tr_block_index_t> got_block_;
    libtransmission::SimpleObservable<tr_torrent*, tr_piece_index_t> got_have_;
    libtransmission::SimpleObservable<tr_torrent*> got_have_all_;
    libtransmission::SimpleObservable<tr_torrent*, tr_piece_index_t> piece_completed_;
    libtransmission::SimpleObservable<tr_torrent*, tr_file_index_t const*, tr_file_index_t, tr_priority_t> priority_changed_;
    libtransmission::SimpleObservable<tr_torrent*, bool> sequential_download_changed_;

    static auto constexpr PeerHasAllPieces = [](tr_piece_index_t)
    {
        return true;
    };
    static auto constexpr ClientHasNoActiveRequests = [](tr_block_index_t)
    {
        return false;
    };
};

TEST_F(PeerMgrWishlistTest, doesNotRequestPiecesThatAreNotWanted)
{
    auto mediator_ptr = std::make_unique<MockMediator>(*this);
    auto& mediator = *mediator_ptr;

    // setup: three pieces, all missing
    mediator.piece_count_ = 3;
    mediator.missing_block_count_[0] = 100;
    mediator.missing_block_count_[1] = 100;
    mediator.missing_block_count_[2] = 50;
    mediator.block_span_[0] = { 0, 100 };
    mediator.block_span_[1] = { 100, 200 };
    mediator.block_span_[2] = { 200, 250 };

    // but we only want the first piece
    mediator.client_wants_piece_.insert(0);

    // we should only get the first piece back
    auto wishlist = Wishlist{ std::move(mediator_ptr) };
    auto const spans = wishlist.next(1000, PeerHasAllPieces, ClientHasNoActiveRequests);
    ASSERT_EQ(1U, std::size(spans));
    EXPECT_EQ(mediator.block_span_[0].begin, spans[0].begin);
    EXPECT_EQ(mediator.block_span_[0].end, spans[0].end);
}

TEST_F(PeerMgrWishlistTest, onlyRequestBlocksThePeerHas)
{
    auto mediator = std::make_unique<MockMediator>(*this);

    // setup: three pieces, all missing
    mediator->piece_count_ = 3;
    mediator->missing_block_count_[0] = 100;
    mediator->missing_block_count_[1] = 100;
    mediator->missing_block_count_[2] = 50;
    mediator->block_span_[0] = { 0, 100 };
    mediator->block_span_[1] = { 100, 200 };
    mediator->block_span_[2] = { 200, 250 };

    // and we want all three pieces
    mediator->client_wants_piece_.insert(0);
    mediator->client_wants_piece_.insert(1);
    mediator->client_wants_piece_.insert(2);

    // but the peer only has the second piece, we don't want to
    // request blocks other than these
    static auto constexpr IsPieceOne = [](tr_piece_index_t p)
    {
        return p == 1U;
    };

    // even if we ask wishlist for more blocks than exist,
    // it should only return blocks [100..200)
    auto const spans = Wishlist{ std::move(mediator) }.next(1000, IsPieceOne, ClientHasNoActiveRequests);
    auto requested = tr_bitfield{ 250 };
    for (auto const& span : spans)
    {
        requested.set_span(span.begin, span.end);
    }
    EXPECT_EQ(100U, requested.count());
    EXPECT_EQ(0U, requested.count(0, 100));
    EXPECT_EQ(100U, requested.count(100, 200));
    EXPECT_EQ(0U, requested.count(200, 250));
}

TEST_F(PeerMgrWishlistTest, doesNotRequestSameBlockTwiceFromSamePeer)
{
    auto mediator = std::make_unique<MockMediator>(*this);

    // setup: three pieces, all missing
    mediator->piece_count_ = 3;
    mediator->missing_block_count_[0] = 100;
    mediator->missing_block_count_[1] = 100;
    mediator->missing_block_count_[2] = 50;
    mediator->block_span_[0] = { 0, 100 };
    mediator->block_span_[1] = { 100, 200 };
    mediator->block_span_[2] = { 200, 250 };

    // and we want all three pieces
    mediator->client_wants_piece_.insert(0);
    mediator->client_wants_piece_.insert(1);
    mediator->client_wants_piece_.insert(2);

    // but we've already requested blocks [0..10) from this peer,
    // so we don't want to send repeated requests
    static auto constexpr IsBetweenZeroToTen = [](tr_block_index_t b)
    {
        return b < 10U;
    };

    // even if we ask wishlist for more blocks than exist,
    // it should omit blocks [0..10) from the return set
    auto const spans = Wishlist{ std::move(mediator) }.next(1000, PeerHasAllPieces, IsBetweenZeroToTen);
    auto requested = tr_bitfield{ 250 };
    for (auto const& span : spans)
    {
        requested.set_span(span.begin, span.end);
    }
    EXPECT_EQ(240U, requested.count());
    EXPECT_EQ(0U, requested.count(0, 10));
    EXPECT_EQ(240U, requested.count(10, 250));
}

TEST_F(PeerMgrWishlistTest, doesNotRequestDupesWhenNotInEndgame)
{
    auto mediator = std::make_unique<MockMediator>(*this);

    // setup: three pieces, all missing
    mediator->piece_count_ = 3;
    mediator->missing_block_count_[0] = 100;
    mediator->missing_block_count_[1] = 100;
    mediator->missing_block_count_[2] = 50;
    mediator->block_span_[0] = { 0, 100 };
    mediator->block_span_[1] = { 100, 200 };
    mediator->block_span_[2] = { 200, 250 };

    // and we want all three pieces
    mediator->client_wants_piece_.insert(0);
    mediator->client_wants_piece_.insert(1);
    mediator->client_wants_piece_.insert(2);

    // but we've already requested blocks [0..10) from someone else,
    // and it is not endgame, so we don't want to send repeated requests
    for (tr_block_index_t block = 0; block < 10; ++block)
    {
        mediator->active_request_count_[block] = 1;
    }

    // even if we ask wishlist for more blocks than exist,
    // it should omit blocks [0..10) from the return set
    auto const spans = Wishlist{ std::move(mediator) }.next(1000, PeerHasAllPieces, ClientHasNoActiveRequests);
    auto requested = tr_bitfield{ 250 };
    for (auto const& span : spans)
    {
        requested.set_span(span.begin, span.end);
    }
    EXPECT_EQ(240U, requested.count());
    EXPECT_EQ(0U, requested.count(0, 10));
    EXPECT_EQ(240U, requested.count(10, 250));
}

TEST_F(PeerMgrWishlistTest, onlyRequestsDupesDuringEndgame)
{
    auto mediator = std::make_unique<MockMediator>(*this);

    // setup: three pieces, all missing
    mediator->piece_count_ = 3;
    mediator->missing_block_count_[0] = 100;
    mediator->missing_block_count_[1] = 100;
    mediator->missing_block_count_[2] = 50;
    mediator->block_span_[0] = { 0, 100 };
    mediator->block_span_[1] = { 100, 200 };
    mediator->block_span_[2] = { 200, 250 };

    // and we want all three pieces
    mediator->client_wants_piece_.insert(0);
    mediator->client_wants_piece_.insert(1);
    mediator->client_wants_piece_.insert(2);

    // we've already requested blocks [0..10) from someone else,
    // but it is endgame, so we can request each block twice.
    // blocks [5..10) are already requested twice
    mediator->is_endgame_ = true;
    for (tr_block_index_t block = 0; block < 5; ++block)
    {
        mediator->active_request_count_[block] = 1;
    }
    for (tr_block_index_t block = 5; block < 10; ++block)
    {
        mediator->active_request_count_[block] = 2;
    }

    // if we ask wishlist for more blocks than exist,
    // it should omit blocks [5..10) from the return set
    auto const spans = Wishlist{ std::move(mediator) }.next(1000, PeerHasAllPieces, ClientHasNoActiveRequests);
    auto requested = tr_bitfield{ 250 };
    for (auto const& span : spans)
    {
        requested.set_span(span.begin, span.end);
    }
    EXPECT_EQ(245U, requested.count());
    EXPECT_EQ(5U, requested.count(0, 5));
    EXPECT_EQ(0U, requested.count(5, 10));
    EXPECT_EQ(240U, requested.count(10, 250));
}

TEST_F(PeerMgrWishlistTest, sequentialDownload)
{
    auto const get_spans = [this](size_t n_wanted)
    {
        auto mediator = std::make_unique<MockMediator>(*this);

        // setup: three pieces, all missing
        mediator->piece_count_ = 3;
        mediator->missing_block_count_[0] = 100;
        mediator->missing_block_count_[1] = 100;
        mediator->missing_block_count_[2] = 50;
        mediator->block_span_[0] = { 0, 100 };
        mediator->block_span_[1] = { 100, 200 };
        mediator->block_span_[2] = { 200, 250 };

        // and we want all three pieces
        mediator->client_wants_piece_.insert(0);
        mediator->client_wants_piece_.insert(1);
        mediator->client_wants_piece_.insert(2);

        // we enabled sequential download
        mediator->is_sequential_download_ = true;

        return Wishlist{ std::move(mediator) }.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // when we ask for blocks, apart from the last piece,
    // which will be returned first because it is smaller,
    // we should get pieces in order
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto requested = tr_bitfield{ 250 };
        auto const spans = get_spans(100);
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(100U, requested.count());
        EXPECT_EQ(50U, requested.count(0, 100));
        EXPECT_EQ(0U, requested.count(100, 200));
        EXPECT_EQ(50U, requested.count(200, 250));
    }

    // Same premise as previous test, but ask for more blocks.
    for (int run = 0; run < NumRuns; ++run)
    {
        auto requested = tr_bitfield{ 250 };
        auto const spans = get_spans(200);
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(200U, requested.count());
        EXPECT_EQ(100U, requested.count(0, 100));
        EXPECT_EQ(50U, requested.count(100, 200));
        EXPECT_EQ(50U, requested.count(200, 250));
    }
}

TEST_F(PeerMgrWishlistTest, doesNotRequestTooManyBlocks)
{
    auto mediator = std::make_unique<MockMediator>(*this);

    // setup: three pieces, all missing
    mediator->piece_count_ = 3;
    mediator->missing_block_count_[0] = 100;
    mediator->missing_block_count_[1] = 100;
    mediator->missing_block_count_[2] = 50;
    mediator->block_span_[0] = { 0, 100 };
    mediator->block_span_[1] = { 100, 200 };
    mediator->block_span_[2] = { 200, 250 };

    // and we want everything
    for (tr_piece_index_t i = 0; i < 3; ++i)
    {
        mediator->client_wants_piece_.insert(i);
    }

    // but we only ask for 10 blocks,
    // so that's how many we should get back
    auto const n_wanted = 10U;
    auto const spans = Wishlist{ std::move(mediator) }.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    auto n_got = size_t{};
    for (auto const& span : spans)
    {
        n_got += span.end - span.begin;
    }
    EXPECT_EQ(n_wanted, n_got);
}

TEST_F(PeerMgrWishlistTest, prefersHighPriorityPieces)
{
    auto const get_ranges = [this](size_t n_wanted)
    {
        auto mediator = std::make_unique<MockMediator>(*this);

        // setup: three pieces, all missing
        mediator->piece_count_ = 3;
        mediator->missing_block_count_[0] = 100;
        mediator->missing_block_count_[1] = 100;
        mediator->missing_block_count_[2] = 100;
        mediator->block_span_[0] = { 0, 100 };
        mediator->block_span_[1] = { 100, 200 };
        mediator->block_span_[2] = { 200, 300 };

        // and we want everything
        for (tr_piece_index_t i = 0; i < 3; ++i)
        {
            mediator->client_wants_piece_.insert(i);
        }

        // and the second piece is high priority
        mediator->piece_priority_[1] = TR_PRI_HIGH;

        return Wishlist{ std::move(mediator) }.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // wishlist should pick the high priority piece's blocks first.
    //
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_ranges(10);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(10U, requested.count());
        EXPECT_EQ(0U, requested.count(0, 100));
        EXPECT_EQ(10U, requested.count(100, 200));
        EXPECT_EQ(0U, requested.count(200, 300));
    }
}

TEST_F(PeerMgrWishlistTest, prefersNearlyCompletePieces)
{
    auto const get_ranges = [this](size_t n_wanted)
    {
        auto mediator = std::make_unique<MockMediator>(*this);

        // setup: three pieces, same size
        mediator->piece_count_ = 3;
        mediator->block_span_[0] = { 0, 100 };
        mediator->block_span_[1] = { 100, 200 };
        mediator->block_span_[2] = { 200, 300 };

        // and we want everything
        for (tr_piece_index_t i = 0; i < 3; ++i)
        {
            mediator->client_wants_piece_.insert(i);
        }

        // but some pieces are closer to completion than others
        mediator->missing_block_count_[0] = 10;
        mediator->missing_block_count_[1] = 20;
        mediator->missing_block_count_[2] = 100;
        for (tr_piece_index_t piece = 0; piece < 3; ++piece)
        {
            auto const& span = mediator->block_span_[piece];
            auto const have_end = span.end - mediator->missing_block_count_[piece];

            for (tr_piece_index_t i = span.begin; i < have_end; ++i)
            {
                mediator->client_has_block_.insert(i);
            }
        }

        return Wishlist{ std::move(mediator) }.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // wishlist prefers to get pieces completed ASAP, so it
    // should pick the ones with the fewest missing blocks first.
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const ranges = get_ranges(10);
        auto requested = tr_bitfield{ 300 };
        for (auto const& range : ranges)
        {
            requested.set_span(range.begin, range.end);
        }
        EXPECT_EQ(10U, requested.count());
        EXPECT_EQ(10U, requested.count(0, 100));
        EXPECT_EQ(0U, requested.count(100, 300));
    }

    // Same premise as previous test, but ask for more blocks.
    // Since the second piece is also the second-closest to completion,
    // those blocks should be next in line.
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const ranges = get_ranges(20);
        auto requested = tr_bitfield{ 300 };
        for (auto const& range : ranges)
        {
            requested.set_span(range.begin, range.end);
        }
        EXPECT_EQ(20U, requested.count());
        EXPECT_EQ(10U, requested.count(0, 100));
        EXPECT_EQ(10U, requested.count(100, 200));
        EXPECT_EQ(0U, requested.count(200, 300));
    }
}

TEST_F(PeerMgrWishlistTest, prefersRarerPieces)
{
    auto const get_spans = [this](size_t n_wanted)
    {
        auto mediator = std::make_unique<MockMediator>(*this);

        // setup: three pieces, all missing
        mediator->piece_count_ = 3;
        mediator->missing_block_count_[0] = 100;
        mediator->missing_block_count_[1] = 100;
        mediator->missing_block_count_[2] = 100;
        mediator->block_span_[0] = { 0, 100 };
        mediator->block_span_[1] = { 100, 200 };
        mediator->block_span_[2] = { 200, 300 };

        // and we want everything
        for (tr_piece_index_t i = 0; i < 3; ++i)
        {
            mediator->client_wants_piece_.insert(i);
        }

        // but some pieces are rarer than others
        mediator->piece_replication_[0] = 1;
        mediator->piece_replication_[1] = 3;
        mediator->piece_replication_[2] = 2;

        return Wishlist{ std::move(mediator) }.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // wishlist prefers to request rarer pieces, so it
    // should pick the ones with the smallest replication.
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(100);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(100U, requested.count());
        EXPECT_EQ(100U, requested.count(0, 100));
        EXPECT_EQ(0U, requested.count(100, 300));
    }

    // Same premise as previous test, but ask for more blocks.
    // Since the third piece is the second-rarest, those blocks
    // should be next in line.
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(150);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(150U, requested.count());
        EXPECT_EQ(100U, requested.count(0, 100));
        EXPECT_EQ(0U, requested.count(100, 200));
        EXPECT_EQ(50U, requested.count(200, 300));
    }
}

TEST_F(PeerMgrWishlistTest, peerDisconnectDecrementsReplication)
{
    auto const get_spans = [this](size_t n_wanted)
    {
        auto mediator_ptr = std::make_unique<MockMediator>(*this);
        auto& mediator = *mediator_ptr;

        // setup: three pieces, all missing
        mediator.piece_count_ = 3;
        mediator.missing_block_count_[0] = 100;
        mediator.missing_block_count_[1] = 100;
        mediator.missing_block_count_[2] = 100;
        mediator.block_span_[0] = { 0, 100 };
        mediator.block_span_[1] = { 100, 200 };
        mediator.block_span_[2] = { 200, 300 };

        // and we want everything
        for (tr_piece_index_t i = 0; i < 3; ++i)
        {
            mediator.client_wants_piece_.insert(i);
        }

        // all pieces had the same rarity
        mediator.piece_replication_[0] = 2;
        mediator.piece_replication_[1] = 2;
        mediator.piece_replication_[2] = 2;

        // allow the wishlist to build its cache
        auto wishlist = Wishlist{ std::move(mediator_ptr) };
        (void)wishlist.next(1, PeerHasAllPieces, ClientHasNoActiveRequests);

        // a peer that has only the first piece disconnected, now the
        // first piece should be the rarest piece according to the cache
        auto have = tr_bitfield{ 3 };
        have.set(0);
        peer_disconnect_.emit(nullptr, have);

        // this is what a real mediator should return at this point:
        // mediator.piece_replication_[0] = 1;

        return wishlist.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // wishlist prefers to request rarer pieces, so it
    // should pick the ones with the smallest replication.
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(100);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(100U, requested.count());
        EXPECT_EQ(100U, requested.count(0, 100));
        EXPECT_EQ(0U, requested.count(100, 300));
    }

    // Same premise as previous test, but ask for more blocks.
    // Since the second and third piece are the second-rarest,
    // those blocks should be next in line.
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(150);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(150U, requested.count());
        EXPECT_EQ(100U, requested.count(0, 100));
        EXPECT_EQ(50U, requested.count(100, 300));
    }
}

TEST_F(PeerMgrWishlistTest, gotBitfieldIncrementsReplication)
{
    auto const get_spans = [this](size_t n_wanted)
    {
        auto mediator_ptr = std::make_unique<MockMediator>(*this);
        auto& mediator = *mediator_ptr;

        // setup: three pieces, all missing
        mediator.piece_count_ = 3;
        mediator.missing_block_count_[0] = 100;
        mediator.missing_block_count_[1] = 100;
        mediator.missing_block_count_[2] = 100;
        mediator.block_span_[0] = { 0, 100 };
        mediator.block_span_[1] = { 100, 200 };
        mediator.block_span_[2] = { 200, 300 };

        // and we want everything
        for (tr_piece_index_t i = 0; i < 3; ++i)
        {
            mediator.client_wants_piece_.insert(i);
        }

        // all pieces had the same rarity
        mediator.piece_replication_[0] = 2;
        mediator.piece_replication_[1] = 2;
        mediator.piece_replication_[2] = 2;

        // allow the wishlist to build its cache
        auto wishlist = Wishlist{ std::move(mediator_ptr) };
        (void)wishlist.next(1, PeerHasAllPieces, ClientHasNoActiveRequests);

        // a peer with first 2 pieces connected and sent a bitfield, now the
        // third piece should be the rarest piece according to the cache
        auto have = tr_bitfield{ 3 };
        have.set_span(0, 2);
        got_bitfield_.emit(nullptr, have);

        // this is what a real mediator should return at this point:
        // mediator.piece_replication_[0] = 3;
        // mediator.piece_replication_[1] = 3;

        return wishlist.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // wishlist prefers to request rarer pieces, so it
    // should pick the ones with the smallest replication.
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(100);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(100U, requested.count());
        EXPECT_EQ(0U, requested.count(0, 200));
        EXPECT_EQ(100U, requested.count(200, 300));
    }

    // Same premise as previous test, but ask for more blocks.
    // Since the first and second piece are the second-rarest,
    // those blocks should be next in line.
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(150);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(150U, requested.count());
        EXPECT_EQ(50U, requested.count(0, 200));
        EXPECT_EQ(100U, requested.count(200, 300));
    }
}

TEST_F(PeerMgrWishlistTest, gotBlockResortsPiece)
{
    auto const get_spans = [this](size_t n_wanted)
    {
        auto mediator_ptr = std::make_unique<MockMediator>(*this);
        auto& mediator = *mediator_ptr;

        // setup: three pieces, all missing
        mediator.piece_count_ = 3;
        mediator.missing_block_count_[0] = 100;
        mediator.missing_block_count_[1] = 100;
        mediator.missing_block_count_[2] = 100;
        mediator.block_span_[0] = { 0, 100 };
        mediator.block_span_[1] = { 100, 200 };
        mediator.block_span_[2] = { 200, 300 };

        // and we want everything
        for (tr_piece_index_t i = 0; i < 3; ++i)
        {
            mediator.client_wants_piece_.insert(i);
        }

        // allow the wishlist to build its cache
        auto wishlist = Wishlist{ std::move(mediator_ptr) };
        (void)wishlist.next(1, PeerHasAllPieces, ClientHasNoActiveRequests);

        // we received block 0 from someone, the wishlist should resort the
        // candidate list cache by consulting the mediator
        --mediator.missing_block_count_[0];
        got_block_.emit(nullptr, 0, 0);

        return wishlist.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // wishlist prefers to get pieces completed ASAP, so it
    // should pick the ones with the fewest missing blocks first.
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(100);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(100U, requested.count());
        EXPECT_EQ(100U, requested.count(0, 100));
        EXPECT_EQ(0U, requested.count(100, 300));
    }

    // Same premise as previous test, but ask for more blocks.
    // Since the first and second piece are the second nearest
    // to completion, those blocks should be next in line.
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(150);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(150U, requested.count());
        EXPECT_EQ(100U, requested.count(0, 100));
        EXPECT_EQ(50U, requested.count(100, 300));
    }
}

TEST_F(PeerMgrWishlistTest, gotHaveIncrementsReplication)
{
    auto const get_spans = [this](size_t n_wanted)
    {
        auto mediator_ptr = std::make_unique<MockMediator>(*this);
        auto& mediator = *mediator_ptr;

        // setup: three pieces, all missing
        mediator.piece_count_ = 3;
        mediator.missing_block_count_[0] = 100;
        mediator.missing_block_count_[1] = 100;
        mediator.missing_block_count_[2] = 100;
        mediator.block_span_[0] = { 0, 100 };
        mediator.block_span_[1] = { 100, 200 };
        mediator.block_span_[2] = { 200, 300 };

        // and we want everything
        for (tr_piece_index_t i = 0; i < 3; ++i)
        {
            mediator.client_wants_piece_.insert(i);
        }

        // all pieces had the same rarity
        mediator.piece_replication_[0] = 2;
        mediator.piece_replication_[1] = 2;
        mediator.piece_replication_[2] = 2;

        // allow the wishlist to build its cache
        auto wishlist = Wishlist{ std::move(mediator_ptr) };
        (void)wishlist.next(1, PeerHasAllPieces, ClientHasNoActiveRequests);

        // a peer sent a "Have" message for the first piece, now the
        // first piece should be the least rare piece according to the cache
        got_have_.emit(nullptr, 0);

        // this is what a real mediator should return at this point:
        // mediator.piece_replication_[0] = 3;

        return wishlist.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // wishlist prefers to request rarer pieces, so it
    // should pick the ones with the smallest replication.
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(200);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(200U, requested.count());
        EXPECT_EQ(0U, requested.count(0, 100));
        EXPECT_EQ(200U, requested.count(100, 300));
    }

    // Same premise as previous test, but ask for more blocks.
    // Since the first and second piece are the second-rarest,
    // those blocks should be next in line.
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(250);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(250U, requested.count());
        EXPECT_EQ(50U, requested.count(0, 100));
        EXPECT_EQ(200U, requested.count(100, 300));
    }
}

TEST_F(PeerMgrWishlistTest, gotHaveAllDoesNotAffectOrder)
{
    auto const get_spans = [this](size_t n_wanted)
    {
        auto mediator_ptr = std::make_unique<MockMediator>(*this);
        auto& mediator = *mediator_ptr;

        // setup: three pieces, all missing
        mediator.piece_count_ = 3;
        mediator.missing_block_count_[0] = 100;
        mediator.missing_block_count_[1] = 100;
        mediator.missing_block_count_[2] = 100;
        mediator.block_span_[0] = { 0, 100 };
        mediator.block_span_[1] = { 100, 200 };
        mediator.block_span_[2] = { 200, 300 };

        // and we want everything
        for (tr_piece_index_t i = 0; i < 3; ++i)
        {
            mediator.client_wants_piece_.insert(i);
        }

        // all pieces have different rarity
        mediator.piece_replication_[0] = 1;
        mediator.piece_replication_[1] = 2;
        mediator.piece_replication_[2] = 3;

        // allow the wishlist to build its cache
        auto wishlist = Wishlist{ std::move(mediator_ptr) };
        (void)wishlist.next(1, PeerHasAllPieces, ClientHasNoActiveRequests);

        // a peer sent a "Have All" message, this should not affect the piece order
        got_have_all_.emit(nullptr);

        // this is what a real mediator should return at this point:
        // mediator.piece_replication_[0] = 2;
        // mediator.piece_replication_[1] = 3;
        // mediator.piece_replication_[2] = 4;

        return wishlist.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // wishlist prefers to request rarer pieces, so it
    // should pick the ones with the smallest replication.
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const ranges = get_spans(150);
        auto requested = tr_bitfield{ 300 };
        for (auto const& range : ranges)
        {
            requested.set_span(range.begin, range.end);
        }
        EXPECT_EQ(150U, requested.count());
        EXPECT_EQ(100U, requested.count(0, 100));
        EXPECT_EQ(50U, requested.count(100, 200));
        EXPECT_EQ(0U, requested.count(200, 300));
    }

    // Same premise as previous test, but ask for more blocks.
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(250);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(250U, requested.count());
        EXPECT_EQ(200U, requested.count(0, 200));
        EXPECT_EQ(50U, requested.count(200, 300));
    }
}

TEST_F(PeerMgrWishlistTest, doesNotRequestPieceAfterPieceCompleted)
{
    auto mediator_ptr = std::make_unique<MockMediator>(*this);
    auto& mediator = *mediator_ptr;

    // setup: three pieces, piece 0 is nearly complete
    mediator.piece_count_ = 3;
    mediator.missing_block_count_[0] = 1;
    mediator.missing_block_count_[1] = 100;
    mediator.missing_block_count_[2] = 100;
    mediator.block_span_[0] = { 0, 100 };
    mediator.block_span_[1] = { 100, 200 };
    mediator.block_span_[2] = { 200, 300 };

    // and we want everything
    for (tr_piece_index_t i = 0; i < 3; ++i)
    {
        mediator.client_wants_piece_.insert(i);
    }

    // allow the wishlist to build its cache, it should have all 3 pieces
    // at this point
    auto wishlist = Wishlist{ std::move(mediator_ptr) };
    (void)wishlist.next(1, PeerHasAllPieces, ClientHasNoActiveRequests);

    // we just completed piece 0
    mediator.missing_block_count_[0] = 0;
    piece_completed_.emit(nullptr, 0);

    // receiving a "piece_completed" signal removes the piece from the
    // wishlist's cache, its blocks should not be in the return set.
    auto const spans = wishlist.next(10, PeerHasAllPieces, ClientHasNoActiveRequests);
    auto requested = tr_bitfield{ 300 };
    for (auto const& span : spans)
    {
        requested.set_span(span.begin, span.end);
    }
    EXPECT_EQ(10U, requested.count());
    EXPECT_EQ(0U, requested.count(0, 100));
    EXPECT_EQ(10U, requested.count(100, 300));
}

TEST_F(PeerMgrWishlistTest, settingPriorityRebuildsWishlist)
{
    auto const get_spans = [this](size_t n_wanted)
    {
        auto mediator_ptr = std::make_unique<MockMediator>(*this);
        auto& mediator = *mediator_ptr;

        // setup: three pieces, all missing
        mediator.piece_count_ = 3;
        mediator.missing_block_count_[0] = 100;
        mediator.missing_block_count_[1] = 100;
        mediator.missing_block_count_[2] = 100;
        mediator.block_span_[0] = { 0, 100 };
        mediator.block_span_[1] = { 100, 200 };
        mediator.block_span_[2] = { 200, 300 };

        // and we want everything
        for (tr_piece_index_t i = 0; i < 3; ++i)
        {
            mediator.client_wants_piece_.insert(i);
        }

        // allow the wishlist to build its cache
        auto wishlist = Wishlist{ std::move(mediator_ptr) };
        (void)wishlist.next(1, PeerHasAllPieces, ClientHasNoActiveRequests);

        // a file priority changed, the cache should be rebuilt.
        // let's say the file was in piece 1
        mediator.piece_priority_[1] = TR_PRI_HIGH;
        priority_changed_.emit(nullptr, nullptr, 0U, TR_PRI_HIGH);

        return wishlist.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // wishlist should pick the high priority piece's blocks first.
    //
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto const spans = get_spans(10);
        auto requested = tr_bitfield{ 300 };
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(10U, requested.count());
        EXPECT_EQ(0U, requested.count(0, 100));
        EXPECT_EQ(10U, requested.count(100, 200));
        EXPECT_EQ(0U, requested.count(200, 300));
    }
}

TEST_F(PeerMgrWishlistTest, settingSequentialDownloadRebuildsWishlist)
{
    auto const get_spans = [this](size_t n_wanted)
    {
        auto mediator_ptr = std::make_unique<MockMediator>(*this);
        auto& mediator = *mediator_ptr;

        // setup: three pieces, all missing
        mediator.piece_count_ = 3;
        mediator.missing_block_count_[0] = 100;
        mediator.missing_block_count_[1] = 100;
        mediator.missing_block_count_[2] = 100;
        mediator.block_span_[0] = { 0, 100 };
        mediator.block_span_[1] = { 100, 200 };
        mediator.block_span_[2] = { 200, 300 };

        // and we want everything
        for (tr_piece_index_t i = 0; i < 3; ++i)
        {
            mediator.client_wants_piece_.insert(i);
        }

        // allow the wishlist to build its cache
        auto wishlist = Wishlist{ std::move(mediator_ptr) };
        (void)wishlist.next(1, PeerHasAllPieces, ClientHasNoActiveRequests);

        // the sequential download setting was changed,
        // the cache should be rebuilt
        mediator.is_sequential_download_ = true;
        sequential_download_changed_.emit(nullptr, true);

        return wishlist.next(n_wanted, PeerHasAllPieces, ClientHasNoActiveRequests);
    };

    // we should get pieces in order when we ask for blocks
    // NB: when all other things are equal in the wishlist, pieces are
    // picked at random so this test -could- pass even if there's a bug.
    // So test several times to shake out any randomness
    static auto constexpr NumRuns = 1000;
    for (int run = 0; run < NumRuns; ++run)
    {
        auto requested = tr_bitfield{ 300 };
        auto spans = get_spans(150);
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(150U, requested.count());
        EXPECT_EQ(100U, requested.count(0, 100));
        EXPECT_EQ(50U, requested.count(100, 200));
        EXPECT_EQ(0U, requested.count(200, 300));
    }

    // Same premise as previous test, but ask for more blocks.
    for (int run = 0; run < NumRuns; ++run)
    {
        auto requested = tr_bitfield{ 300 };
        auto spans = get_spans(250);
        for (auto const& span : spans)
        {
            requested.set_span(span.begin, span.end);
        }
        EXPECT_EQ(250U, requested.count());
        EXPECT_EQ(200U, requested.count(0, 200));
        EXPECT_EQ(50U, requested.count(200, 300));
    }
}
