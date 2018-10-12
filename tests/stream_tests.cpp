#include <gtest/gtest.h>

#include "../src/stream.h"

#include <type_traits>

namespace {

using namespace cppstream;

TEST(StreamConstruction, Infinite) {
    Stream s([](){ return 1;});

    EXPECT_EQ(std::vector<int>({1, 1, 1, 1, 1}), s | get(5) | to_vector());
}

TEST(StreamConstruction, InitializerList) {
    Stream s{1, 2, 3, 4, 5};

    EXPECT_EQ(std::vector<int>({1, 2, 3, 4, 5}), s | to_vector());
}

TEST(StreamConstruction, Iterator) {
    const std::vector<int> container({1, 2, 3, 4, 5});

    Stream s(container.begin(), container.end());

    EXPECT_EQ(container, s | to_vector());
}

TEST(StreamConstruction, ContainerCopy) {
    const std::vector<int> container({1, 2, 3, 4, 5});

    Stream s(container);

    EXPECT_EQ(container, s | to_vector());
}

TEST(StreamConstruction, ContainerMove) {
    Stream s(std::vector<int>({1, 2, 3, 4, 5}));

    EXPECT_EQ(std::vector<int>({1, 2, 3, 4, 5}), s | to_vector());
}

TEST(StreamConstruction, Pack) {
    Stream s(1, 2, 3, 4, 5);

    EXPECT_EQ(std::vector<int>({1, 2, 3, 4, 5}), s | to_vector());
}

TEST(StreamConstruction, Copy) {
    Stream s1(std::vector<int>({1, 2, 3, 4, 5}));
    Stream s2(s1);

    EXPECT_EQ(s1 | to_vector(), s2 | to_vector());
}

TEST(StreamConstruction, Move) {
    Stream s1(std::vector<int>({1, 2, 3, 4, 5}));
    Stream s2(std::move(s1));

    EXPECT_EQ(std::vector<int>({1, 2, 3, 4, 5}), s2 | to_vector());
}

TEST(StreamInfo, IsFinite) {
    const std::vector<int> container({1, 2, 3, 4, 5});

    Stream infinite([](){ return 11;});
    Stream il(std::vector<int>({1, 2, 3, 4, 5}));
    Stream iter(container.begin(), container.end());
    Stream cont_copy(container);
    Stream cont_move(std::vector<int>({1, 2, 3, 4, 5}));

    EXPECT_FALSE(infinite.is_finite());
    EXPECT_TRUE(il.is_finite());
    EXPECT_TRUE(iter.is_finite());
    EXPECT_TRUE(cont_copy.is_finite());
    EXPECT_TRUE(cont_move.is_finite());
}

TEST(StreamTerminalOpsTest, Nth) {
    Stream s{1, 2, 3, 4, 5};

    auto elem = s | nth(3);

    EXPECT_EQ(4, elem);
}

TEST(StreamTerminalOpsTest, PrintTo) {
    Stream s{1, 2, 3, 4, 5};
    std::ostringstream os_default, os_spec;

    s | print_to(os_default);

    Stream s2{1, 2, 3, 4, 5};
    s2 | print_to(os_spec, "_");

    EXPECT_EQ("1 2 3 4 5", os_default.str());
    EXPECT_EQ("1_2_3_4_5", os_spec.str());
}

TEST(StreamTerminalOpsTest, Sum) {
    Stream s{1, 2, 3, 4, 5};

    int stream_sum = s | sum();

    EXPECT_EQ(15, stream_sum);
}

TEST(StreamTerminalOpsTest, Reduce) {
    Stream s{1, 2, 3, 4, 5};

    auto simple_result = s | reduce<double, int>([](double res, int val) { return res + 2.0 * val; });
    auto complex_result = s | reduce<double, int>([](int val) { return 10.0 * val; },
                                                  [](double res, int val) { return res + 2.0 * val; });

    EXPECT_TRUE((std::is_same<double, decltype(simple_result)>::value));
    EXPECT_TRUE((std::is_same<double, decltype(complex_result)>::value));

    EXPECT_DOUBLE_EQ(29.0, simple_result);
    EXPECT_DOUBLE_EQ(38.0, complex_result);
}

TEST(StreamTerminalOpsTest, ToVector) {
    Stream s{1, 2, 3, 4, 5};

    auto vec = s | to_vector();

    EXPECT_EQ(std::vector<int>({1, 2, 3, 4, 5}), vec);
}

TEST(StreamNonTerminalOpsTest, Skip) {
    Stream s{1, 2, 3, 4, 5};

    auto skipped = s | skip(2);
    auto vec = skipped | to_vector();

    EXPECT_EQ(std::vector<int>({3, 4, 5}), vec);
    EXPECT_TRUE(skipped.is_finite());
}

TEST(StreamNonTerminalOpsTest, Get) {
    Stream s{1, 2, 3, 4, 5};

    auto got_stream = s | get(3);
    auto vec = got_stream | to_vector();

    EXPECT_EQ(std::vector<int>({1, 2, 3}), vec);
    EXPECT_TRUE(got_stream.is_finite());
}

TEST(StreamNonTerminalOpsTest, Filter) {
    Stream s{1, 2, 3, 4, 5};

    auto filtered_stream = s | filter([](int val) { return val % 2; });
    auto vec = filtered_stream | to_vector();

    EXPECT_EQ(std::vector<int>({1, 3, 5}), vec);
    EXPECT_TRUE(filtered_stream.is_finite());
}

TEST(StreamNonTerminalOpsTest, Group) {
    Stream s{1, 2, 3, 4, 5};

    auto grouped_stream = s | group(3);
    auto vec = grouped_stream | to_vector();

    EXPECT_EQ(std::vector<std::vector<int>>({{1, 2, 3},
                                             {4, 5}}), vec);
    EXPECT_TRUE(grouped_stream.is_finite());
}

TEST(StreamNonTerminalOpsTest, Map) {
    Stream s{1, 2, 3};

    auto mapped_stream = s | map([](int val) { return std::pair(val, val); });
    auto vec = mapped_stream | to_vector();

    EXPECT_EQ((std::vector<std::pair<int, int>>({std::pair(1, 1), std::pair(2, 2), std::pair(3, 3)})), vec);
    EXPECT_TRUE(mapped_stream.is_finite());
}

}
