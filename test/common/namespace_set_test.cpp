#include <limits>

#include "common/namespace_set.h"
#include "common/roaring_mask.h"
#include "gtest/gtest.h"

using namespace riag::common;

TEST(NamespaceMaskTest, KeepsOffsetsBeyond32Bits) {
    const auto high = (uint64_t{1} << 32) + 17;
    Roaring64BitmapSemiMask mask(high + 10);
    mask.mask(17);
    mask.mask(high);
    EXPECT_EQ(mask.range(high, high + 1), (offset_vec_t{high}));
    EXPECT_EQ(mask.range(17, 18), (offset_vec_t{17}));
}

TEST(NamespaceMaskTest, IncludesLast32BitOffsetWithoutWrappingRangeEnd) {
    const auto last = uint64_t{std::numeric_limits<uint32_t>::max()};
    Roaring32BitmapSemiMask mask(last + 1);
    mask.mask(last);
    EXPECT_EQ(mask.range(last, last + 1), (offset_vec_t{last}));
    EXPECT_TRUE(mask.range(last + 1, last + 2).empty());
}

TEST(NamespaceSetTest, UnrestrictedIncludesGlobalAndNamedIDs) {
    const auto all = NamespaceSet{};
    EXPECT_TRUE(all.isUnrestricted());
    EXPECT_FALSE(all.isEmpty());
    EXPECT_TRUE(all.contains(GLOBAL_NAMESPACE_ID));
    EXPECT_TRUE(all.contains(1));
    EXPECT_TRUE(all.contains(std::numeric_limits<namespace_id_t>::max()));
    EXPECT_FALSE(all.getSingleton().has_value());
}

TEST(NamespaceSetTest, FiniteSelectionIsASetAndZeroIsAnOrdinaryDomain) {
    const auto selection = NamespaceSet::finite({9, GLOBAL_NAMESPACE_ID, 9, 2, 2});
    EXPECT_FALSE(selection.isUnrestricted());
    EXPECT_EQ(selection.getIDs().size(), 3);
    EXPECT_TRUE(selection.contains(GLOBAL_NAMESPACE_ID));
    EXPECT_TRUE(selection.contains(2));
    EXPECT_TRUE(selection.contains(9));
    EXPECT_FALSE(selection.contains(1));
    EXPECT_EQ(NamespaceSet::finite({7, 7}).getSingleton(), 7);
    EXPECT_EQ(NamespaceSet::finite({}), NamespaceSet::none());
}

TEST(NamespaceSetTest, InlineSelectionCannotBroadenItsBase) {
    const auto base = NamespaceSet::finite({GLOBAL_NAMESPACE_ID, 2});
    const auto inlineSelection = NamespaceSet::finite({2, 3});
    EXPECT_EQ(base.intersect(inlineSelection), NamespaceSet::single(2));
    EXPECT_EQ(NamespaceSet::single(2).intersect(NamespaceSet::single(3)), NamespaceSet::none());
    EXPECT_EQ(base.intersect(NamespaceSet{}), base);
    EXPECT_EQ(NamespaceSet{}.intersect(base), base);
    EXPECT_EQ(base.intersect(NamespaceSet::none()), NamespaceSet::none());
}

TEST(NamespaceSetTest, ScopeIntersectionLaws) {
    const std::vector<NamespaceSet> scopes{NamespaceSet{}, NamespaceSet::none(),
        NamespaceSet::single(GLOBAL_NAMESPACE_ID), NamespaceSet::single(7),
        NamespaceSet::finite({0, 3, 7}), NamespaceSet::finite({1, 3, 9})};
    for (const auto& left : scopes) {
        EXPECT_EQ(left.intersect(left), left);
        for (const auto& right : scopes) {
            const auto intersection = left.intersect(right);
            EXPECT_EQ(intersection, right.intersect(left));
            for (namespace_id_t id = 0; id < 12; ++id) {
                EXPECT_EQ(intersection.contains(id), left.contains(id) && right.contains(id));
            }
            for (const auto& third : scopes) {
                EXPECT_EQ(intersection.intersect(third), left.intersect(right.intersect(third)));
            }
        }
    }
}
