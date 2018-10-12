#ifndef STREAM_UTILS_H
#define STREAM_UTILS_H

#include <optional>

namespace cppstream::internal {

template<class T, typename = void>
struct is_value_generator : std::false_type {
};

template<class T>
struct is_value_generator<T,
        typename std::enable_if_t<std::is_invocable_v<T> &&
                                  !std::is_same<std::invoke_result_t<T>, void>::value>>
        : std::true_type {
};

template<class T, typename = void>
struct is_iterator : std::false_type {
};

template<class T>
struct is_iterator<T,
        typename std::enable_if_t<!std::is_same<typename std::iterator_traits<T>::value_type, void>::value>>
        : std::true_type {
};

template<class C, typename = void>
struct is_container : std::false_type {
};

template<class C>
struct is_container<C,
        typename std::enable_if_t<!is_value_generator<C>::value &&
                                  is_iterator<decltype(std::declval<C>().begin())>::value &&
                                  is_iterator<decltype(std::declval<C>().end())>::value>>
        : std::true_type {
};

template<class Generator>
class InfiniteGenerator final {
public:
    using value_type = std::invoke_result_t<Generator>;

    InfiniteGenerator(Generator && value_generator)
            : value_generator_(std::move(value_generator)) {}

    InfiniteGenerator(const InfiniteGenerator & other) = default;

    InfiniteGenerator(InfiniteGenerator && other)
            : value_generator_(std::move(other.value_generator_)) {}

    ~InfiniteGenerator() = default;

    InfiniteGenerator & operator=(const InfiniteGenerator & other) = delete;

    std::optional<value_type> operator()() {
        return {value_generator_()};
    }

private:
    Generator value_generator_;
};

template<class T>
class PackGenerator final {
    using container_iterator = typename std::vector<T>::const_reverse_iterator;
public:
    using value_type = T;

    PackGenerator() = default;

    PackGenerator(const PackGenerator & other) = default;

    PackGenerator(PackGenerator && other)
            : container_(std::move(other.container_)),
              current_(container_.cbegin()),
              end_(container_.cend()) {}

    ~PackGenerator() = default;

    PackGenerator & operator=(const PackGenerator & other) = default;

    PackGenerator & operator=(PackGenerator && other) {
        container_ = std::move(other.container_);
        current_ = container_.cbegin();
        end_ = container_.cend();
    }

    void push_back(T value) {
        container_.push_back(value);
        current_ = container_.crbegin();
        end_ = container_.crend();
    }

    std::optional<value_type> operator()() {
        if (current_ == end_) return std::nullopt;

        return {*(current_++)};
    }

private:
    std::vector<T> container_;
    container_iterator current_;
    container_iterator end_;
};

template<class Container>
class ContainerGenerator final {
    using container_iterator = typename Container::const_iterator;
public:
    using value_type = typename Container::value_type;

    ContainerGenerator(const Container & container)
            : container_(container),
              current_(container_.cbegin()),
              end_(container_.cend()) {}

    ContainerGenerator(Container && container)
            : container_(std::move(container)),
              current_(container_.cbegin()),
              end_(container_.cend()) {}

    ContainerGenerator(const ContainerGenerator & other)
            : container_(other.container_),
              current_(container_.cbegin()),
              end_(container_.cend()) {}

    ContainerGenerator(ContainerGenerator && other)
            : container_(std::move(other.container_)),
              current_(container_.cbegin()),
              end_(container_.cend()) {}

    ~ContainerGenerator() = default;

    ContainerGenerator & operator=(const ContainerGenerator & other) {
        container_ = other.container_;
        current_ = container_.begin();
        end_ = container_.end();
    }

    ContainerGenerator & operator=(ContainerGenerator && other) {
        container_ = std::move(other.container_);
        current_ = container_.begin();
        end_ = container_.end();
    }

    std::optional<value_type> operator()() {
        if (current_ == end_) return std::nullopt;

        return {*(current_++)};
    }

private:
    Container container_;
    container_iterator current_;
    container_iterator end_;
};

template<class ParentGenerator>
class SkipGenerator {
public:
    using value_type = typename ParentGenerator::value_type;

    SkipGenerator(const ParentGenerator & parent_gen, size_t amount)
            : parent_gen_(parent_gen), amount_to_skip_(amount), skipped_(false) {}

    SkipGenerator(const SkipGenerator & other) = default;

    SkipGenerator(SkipGenerator && other)
            : parent_gen_(std::move(other.parent_gen_)),
              amount_to_skip_(other.amount_to_skip_),
              skipped_(other.skipped_) {}

    ~SkipGenerator() = default;

    SkipGenerator & operator=(const SkipGenerator & other) = delete;

    std::optional<value_type> operator()() {
        if (!skipped_) {
            std::optional<value_type> opt;
            size_t i = 0;
            while (i < amount_to_skip_ && (opt = parent_gen_())) {
                ++i;
            }
            skipped_ = true;
            if (i < amount_to_skip_) {
                return std::nullopt;
            }
        }
        return parent_gen_();
    }

private:
    ParentGenerator parent_gen_;
    const size_t amount_to_skip_;
    bool skipped_;
};

template<class ParentGenerator>
class GetGenerator {
public:
    using value_type = typename ParentGenerator::value_type;

    GetGenerator(const ParentGenerator & parent_gen, size_t amount)
            : parent_gen_(parent_gen), amount_to_get_(amount), amount_got_(0) {}

    GetGenerator(const GetGenerator & other) = default;

    GetGenerator(GetGenerator && other)
            : parent_gen_(std::move(other.parent_gen_)),
              amount_to_get_(other.amount_to_get_),
              amount_got_(other.amount_got_) {}

    ~GetGenerator() = default;

    GetGenerator & operator=(const GetGenerator & other) = delete;

    std::optional<value_type> operator()() {
        if (amount_got_ >= amount_to_get_) {
            return std::nullopt;
        }

        ++amount_got_;
        return parent_gen_();
    }

private:
    ParentGenerator parent_gen_;
    const size_t amount_to_get_;
    size_t amount_got_;
};

template<class ParentGenerator, class Predicate>
class FilterGenerator {
public:
    using value_type = typename ParentGenerator::value_type;

    FilterGenerator(const ParentGenerator & parent_gen,
                    const Predicate & predicate)
            : parent_gen_(parent_gen), predicate_(predicate) {}

    FilterGenerator(const ParentGenerator & parent_gen,
                    Predicate && predicate)
            : parent_gen_(parent_gen), predicate_(std::move(predicate)) {}

    FilterGenerator(FilterGenerator && other)
            : parent_gen_(std::move(other.parent_gen_)),
              predicate_(std::move(other.predicate_)) {}

    FilterGenerator(const FilterGenerator & other) = default;

    ~FilterGenerator() = default;

    FilterGenerator & operator=(const FilterGenerator & other) = delete;

    std::optional<value_type> operator()() {
        std::optional<value_type> opt;
        while ((opt = parent_gen_()) && !predicate_(opt.value()));

        return opt;
    }

private:
    ParentGenerator parent_gen_;
    Predicate predicate_;
};

template<class ParentGenerator>
class GroupGenerator {
    using parent_value_type = typename ParentGenerator::value_type;
public:
    using value_type = std::vector<parent_value_type>;

    GroupGenerator(const ParentGenerator & parent_gen, size_t group_size)
            : parent_gen_(parent_gen), group_size_(group_size) {}

    GroupGenerator(const GroupGenerator & other) = default;

    GroupGenerator(GroupGenerator && other)
            : parent_gen_(std::move(other.parent_gen_)),
              group_size_(other.group_size_) {}

    ~GroupGenerator() = default;

    GroupGenerator & operator=(const GroupGenerator & other) = delete;

    std::optional<value_type> operator()() {
        value_type group;
        std::optional<parent_value_type> opt = parent_gen_();
        if (!opt.has_value()) {
            return std::nullopt;
        }

        size_t i = 0;
        do {
            group.push_back(opt.value());
            ++i;
        } while (i < group_size_ && (opt = parent_gen_()));
        return group;
    }

private:
    ParentGenerator parent_gen_;
    const size_t group_size_;
};

template<class ParentGenerator, class Transform>
class MapGenerator {
    using parent_value_type = typename ParentGenerator::value_type;
public:
    using value_type = std::invoke_result_t<Transform, parent_value_type>;

    MapGenerator(const ParentGenerator & parent_gen,
                 const Transform & transform)
            : parent_gen_(parent_gen), transform_(transform) {}

    MapGenerator(const ParentGenerator & parent_gen,
                 Transform && transform)
            : parent_gen_(parent_gen), transform_(std::move(transform)) {}

    MapGenerator(MapGenerator && other)
            : parent_gen_(std::move(other.parent_gen_)),
              transform_(std::move(other.transform_)) {}

    MapGenerator(const MapGenerator & other) = default;

    ~MapGenerator() = default;

    MapGenerator & operator=(const MapGenerator & other) = delete;

    std::optional<value_type> operator()() {
        std::optional<parent_value_type> opt = parent_gen_();
        if (!opt.has_value()) {
            return std::nullopt;
        }

        return transform_(opt.value());
    }

private:
    ParentGenerator parent_gen_;
    Transform transform_;
};

}

#endif //STREAM_UTILS_H
