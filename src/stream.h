#ifndef STREAM_H
#define STREAM_H

#include <functional>
#include "stream_utils.h"

namespace cppstream {

struct print_to {
    std::ostream & ostream;
    const char * delimiter;

    explicit print_to(std::ostream & os, const char * delimiter = " ")
            : ostream(os), delimiter(delimiter) {}
};

struct nth {
    size_t n;

    explicit nth(size_t n) : n(n) {}
};

template<class U, class T>
struct reduce {
    typedef std::function<U(T)> Identity;
    typedef std::function<U(U, T)> Accumulator;

    Identity identity;
    Accumulator accumulator;

    explicit reduce(Accumulator && accumulator)
            : identity([](T val) { return static_cast<U>(val); }), accumulator(accumulator) {}

    reduce(Identity && identity,
           Accumulator && accumulator)
            : identity(std::move(identity)), accumulator(std::move(accumulator)) {}
};

struct to_vector {
};

struct sum {
};

struct skip {
    size_t amount;

    explicit skip(size_t amount) : amount(amount) {}
};

struct get {
    size_t amount;

    explicit get(size_t amount) : amount(amount) {}
};

struct group {
    size_t group_size;

    explicit group(size_t group_size) : group_size(group_size) {
//        assert(group_size > 0, "Groups can only have positive size.");
    }
};

template<class Predicate>
struct filter {
    Predicate predicate;

    explicit filter(Predicate && predicate) : predicate(std::move(predicate)) {}

    explicit filter(const Predicate & predicate) : predicate(predicate) {}
};

template<class Transform>
struct map {
    Transform transform;

    explicit map(Transform && transform) : transform(std::move(transform)) {}

    explicit map(const Transform & transform) : transform(transform) {}
};

struct IllegalStreamOperation : public std::logic_error {
    explicit IllegalStreamOperation(const char * msg) : logic_error(msg) {}
};

enum class StreamTag {
    Finite, Infinite
};

template<class StreamGenerator, StreamTag Tag>
class Stream {
public:
    using value_type = typename StreamGenerator::value_type;

    /**
    * Constructs Stream whose values are the return values of repeated calls to the generator function with no arguments.
    */
    template<class ValueGenerator>
    explicit Stream(ValueGenerator && generator,
                    typename std::enable_if_t<internal::is_value_generator<ValueGenerator>::value &&
                                              !internal::is_container<ValueGenerator>::value, ValueGenerator> * = nullptr)
            : generator_(internal::InfiniteGenerator<ValueGenerator>(std::move(generator))) {}

    /**
    * Construct Stream from range [begin, end)
    * @example Stream s(myVector.begin(), myVector.end())
    */
    template<class Iterator,
            typename = std::enable_if_t<internal::is_iterator<Iterator>::value>>
    Stream(Iterator first, Iterator last)
            : generator_(internal::ContainerGenerator(std::vector<value_type>(first, last))) {}

    /**
    * Constructs Stream from the Container
    * @example Stream s(myVector)
    */
    template<class Container,
            typename = std::enable_if_t<internal::is_container<Container>::value &&
                                        std::is_copy_constructible<Container>::value>>
    explicit Stream(const Container & container)
            : generator_(internal::ContainerGenerator(container)) {}

    /**
    * Constructs Stream from the rref Container
    * @example Stream s(std::move(myVector))
    */
    template<class Container>
    explicit Stream(Container && container,
                    typename std::enable_if_t<!internal::is_value_generator<Container>::value &&
                                              internal::is_container<Container>::value &&
                                              std::is_move_constructible<Container>::value, Container> * = nullptr)
            : generator_(internal::ContainerGenerator(std::move(container))) {}

    /**
    * Constructs Stream from the initializer_list
    * @example Stream s({1, 2, 3, 4, 5})
    */
    template<class T>
    Stream(std::initializer_list<T> il)
            : generator_(internal::ContainerGenerator(std::vector<value_type>(il))) {}

    /**
     * Constructs Stream from the pack
     * @example Stream a(1, 2, 3, 4, 10)
     */
    template<class T, class... Args>
    Stream(T first, Args... args)
            : Stream(args...) {
        generator_.push_back(first);
    }

    template<class T>
    Stream(T first,
           typename std::enable_if_t<!internal::is_value_generator<T>::value &&
                                     !internal::is_container<T>::value, T> * = nullptr) {
        generator_.push_back(first);
    }

    Stream(Stream && other) noexcept
            : generator_(std::move(other.generator_)) {}

    Stream(const Stream & other) = default;

    Stream & operator=(const Stream & other) = default;

    /**
     * Checks if stream is finite
     * @return true if stream is finite
     */
    constexpr bool is_finite() const { return Tag == StreamTag::Finite; }

    std::ostream & operator|(print_to && operation_props);

    value_type operator|(nth && operation_props);

    template<class U>
    U operator|(reduce<U, value_type> && operation_props);

    std::vector<value_type> operator|(to_vector && unused);

    value_type operator|(sum && unused);

    Stream<internal::SkipGenerator<StreamGenerator>, Tag> operator|(skip && operation_props);

    Stream<internal::GetGenerator<StreamGenerator>, StreamTag::Finite> operator|(get && operation_props);

    template<class Predicate>
    Stream<internal::FilterGenerator<StreamGenerator, Predicate>, Tag> operator|(filter<Predicate> && operation_props);

    Stream<internal::GroupGenerator<StreamGenerator>, Tag> operator|(group && operation_props);

    template<class Transform>
    Stream<internal::MapGenerator<StreamGenerator, Transform>, Tag> operator|(map<Transform> && operation_props);

    template<class OtherGen, StreamTag OtherTag> friend
    class Stream;

private:
    template<class StreamGen>
    Stream(StreamGen && generator, StreamTag unused)
            : generator_(std::move(generator)) {}

    StreamGenerator generator_;
};

template<class StreamGenerator, StreamTag Tag>
std::ostream &
Stream<StreamGenerator, Tag>::operator|(print_to && operation_props) {
    static_assert(Tag == StreamTag::Finite, "Operation print_to cannot be performed on infinite stream.");
    StreamGenerator gen(generator_);
    std::ostream & os = operation_props.ostream;
    std::optional<value_type> opt;
    if (opt = gen()) {
        os << opt.value();
        while (opt = gen()) {
            os << operation_props.delimiter << opt.value();
        }
    }
    return os;
}

template<class StreamGenerator, StreamTag Tag>
typename Stream<StreamGenerator, Tag>::value_type
Stream<StreamGenerator, Tag>::operator|(nth && operation_props) {
    StreamGenerator gen(generator_);
    std::optional<value_type> opt;
    size_t i = 0;
    while (i <= operation_props.n && (opt = gen())) {
        ++i;
    }
    if (i <= operation_props.n) {
        throw IllegalStreamOperation("Stream doesn't contain enough elements to perform operation 'nth'.");
    }
    return opt.value();
}

template<class StreamGenerator, StreamTag Tag>
template<class U>
U
Stream<StreamGenerator, Tag>::operator|(reduce<U, value_type> && operation_props) {
    static_assert(Tag == StreamTag::Finite, "Operation reduce cannot be performed on infinite stream.");
    StreamGenerator gen(generator_);
    std::optional<value_type> opt = gen();
    if (!opt.has_value()) {
        throw IllegalStreamOperation("Operation 'reduce' cannot be performed on empty stream.");
    }
    U result = operation_props.identity(opt.value());
    while (opt = gen()) {
        result = operation_props.accumulator(result, opt.value());
    }
    return result;
}

template<class StreamGenerator, StreamTag Tag>
auto Stream<StreamGenerator, Tag>::operator|(to_vector && unused) -> std::vector<value_type> {
    static_assert(Tag == StreamTag::Finite, "Operation to_vector cannot be performed on infinite stream.");
    StreamGenerator gen(generator_);
    std::optional<value_type> opt;
    std::vector<value_type> vec;
    while (opt = gen()) {
        vec.push_back(opt.value());
    }
    return vec;
}

template<class StreamGenerator, StreamTag Tag>
typename Stream<StreamGenerator, Tag>::value_type
Stream<StreamGenerator, Tag>::operator|(sum && unused) {
    static_assert(Tag == StreamTag::Finite, "Operation sum cannot be performed on infinite stream.");
    StreamGenerator gen(generator_);
    std::optional<value_type> opt = gen();
    if (!opt.has_value()) {
        throw IllegalStreamOperation("Operation 'sum' cannot be performed on empty stream.");
    }
    value_type stream_sum = opt.value();
    while (opt = gen()) {
        stream_sum += opt.value();
    }
    return stream_sum;
}

template<class StreamGenerator, StreamTag Tag>
Stream<internal::SkipGenerator<StreamGenerator>, Tag>
Stream<StreamGenerator, Tag>::operator|(skip && operation_props) {
    using SkipGen = internal::SkipGenerator<StreamGenerator>;
    return Stream<SkipGen, Tag>(SkipGen(generator_, operation_props.amount), Tag);
}

template<class StreamGenerator, StreamTag Tag>
Stream<internal::GetGenerator<StreamGenerator>, StreamTag::Finite>
Stream<StreamGenerator, Tag>::operator|(get && operation_props) {
    using GetGen = internal::GetGenerator<StreamGenerator>;
    return Stream<GetGen, StreamTag::Finite>(GetGen(generator_, operation_props.amount), StreamTag::Finite);
}

template<class StreamGenerator, StreamTag Tag>
template<class Predicate>
Stream<internal::FilterGenerator<StreamGenerator, Predicate>, Tag>
Stream<StreamGenerator, Tag>::operator|(filter<Predicate> && operation_props) {
    using FilterGen = internal::FilterGenerator<StreamGenerator, Predicate>;
    return Stream<FilterGen, Tag>(FilterGen(generator_, operation_props.predicate), Tag);
}

template<class StreamGenerator, StreamTag Tag>
Stream<internal::GroupGenerator<StreamGenerator>, Tag>
Stream<StreamGenerator, Tag>::operator|(group && operation_props) {
    using GroupGen = internal::GroupGenerator<StreamGenerator>;
    return Stream<GroupGen, Tag>(GroupGen(generator_, operation_props.group_size), Tag);
}

template<class StreamGenerator, StreamTag Tag>
template<class Transform>
Stream<internal::MapGenerator<StreamGenerator, Transform>, Tag>
Stream<StreamGenerator, Tag>::operator|(map<Transform> && operation_props) {
    using MapGen = internal::MapGenerator<StreamGenerator, Transform>;
    return Stream<MapGen, Tag>(MapGen(generator_, operation_props.transform), Tag);
}

// Deduction guides

template<class ValueGenerator>
explicit Stream(ValueGenerator
&& generator,
typename std::enable_if_t<internal::is_value_generator<ValueGenerator>::value &&
                          !internal::is_container<ValueGenerator>::value, ValueGenerator> * = nullptr) ->
Stream<internal::InfiniteGenerator<ValueGenerator>, StreamTag::Infinite>;

template<class Iterator,
        typename = std::enable_if_t<internal::is_iterator<Iterator>::value>>
Stream(Iterator
first,
Iterator last
) ->
Stream<internal::ContainerGenerator<std::vector<typename std::iterator_traits<Iterator>::value_type>>, StreamTag::Finite>;

template<class Container,
        typename = std::enable_if_t<internal::is_container<Container>::value &&
                                    std::is_copy_constructible<Container>::value>>
Stream(const Container & container)

->
Stream<internal::ContainerGenerator<Container>, StreamTag::Finite>;

template<class Container>
explicit Stream(Container
&& container,
typename std::enable_if_t<!internal::is_value_generator<Container>::value &&
                          internal::is_container<Container>::value &&
                          std::is_move_constructible<Container>::value, Container> * = nullptr) ->
Stream<internal::ContainerGenerator<Container>, StreamTag::Finite>;

template<class T>
Stream(std::initializer_list<T>
il) ->
Stream<internal::ContainerGenerator<std::vector<T>>, StreamTag::Finite>;

template<class T, class... Args>
Stream(T first, Args... args) ->
Stream<internal::PackGenerator<T>, StreamTag::Finite>;

template<class T>
Stream(T first,
typename std::enable_if_t<!internal::is_value_generator<T>::value &&
                          !internal::is_container<T>::value, T> * = nullptr) ->
Stream<internal::PackGenerator<T>, StreamTag::Finite>;

}

#endif //STREAM_H
