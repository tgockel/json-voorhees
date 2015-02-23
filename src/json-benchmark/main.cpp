#include <jsonv/all.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>

using namespace jsonv;

struct generated_json_settings
{
    std::uniform_int_distribution<int> kind_distribution{0, 6};
    
    template <typename TRng>
    jsonv::kind kind(TRng& rng, std::size_t current_depth)
    {
        if (current_depth < 2)
            return jsonv::kind::object;
        
        auto k = static_cast<jsonv::kind>(kind_distribution(rng));
        if (current_depth > 5 && (k == jsonv::kind::array || k == jsonv::kind::object))
            return kind(rng, current_depth);
        else
            return k;
    }
    
    std::uniform_int_distribution<std::size_t> array_length_distribution{0, 100};
    template <typename TRng>
    std::size_t array_length(TRng& rng)
    {
        return array_length_distribution(rng);
    }
    
    std::uniform_int_distribution<std::size_t> object_size_distribution{5, 25};
    template <typename TRng>
    std::size_t object_size(TRng& rng)
    {
        return object_size_distribution(rng);
    }
    
    std::uniform_int_distribution<std::int64_t> string_length_distribution{0, 100};
    template <typename TRng>
    std::size_t string_length(TRng& rng)
    {
        return string_length_distribution(rng);
    }
    
    
    std::normal_distribution<> decimal_distribution{1.0, 40.0};
    template <typename TRng>
    double decimal(TRng& rng)
    {
        return decimal_distribution(rng);
    }
    
    std::uniform_int_distribution<std::int64_t> integer_distribution{~0};
    template <typename TRng>
    std::int64_t integer(TRng& rng)
    {
        return integer_distribution(rng);
    }
};

template <typename TRng, typename TSettings>
jsonv::value generate_json(TRng& rng, TSettings& settings, std::size_t current_depth = 0)
{
    switch (settings.kind(rng, current_depth))
    {
        case kind::array:
        {
            value out = array();
            std::size_t len = settings.array_length(rng);
            for (std::size_t idx = 0; idx < len; ++idx)
                out.push_back(generate_json(rng, settings, current_depth + 1));
            return out;
        }
        case kind::boolean:
            return !(rng() & 1);
        case kind::decimal:
            return settings.decimal(rng);
        case kind::integer:
            return settings.integer(rng);
        case kind::string:
            // TODO: Improve this
            return std::string(settings.string_length(rng), 'a');
        case kind::object:
        {
            value out = object();
            std::size_t len = settings.object_size(rng);
            for (std::size_t idx = 1; idx <= len; ++idx)
            {
                // TODO: Improve
                std::string key(idx, 'a');
                value val = generate_json(rng, settings, current_depth + 1);
                out.insert({ std::move(key), std::move(val) });
            }
            return out;
        }
        case kind::null:
        default:
            return null;
    }
}

class stopwatch
{
public:
    using clock      = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration   = std::chrono::nanoseconds;
    
    struct ticker
    {
        stopwatch* owner;
        time_point start_time;
        
        ticker(stopwatch* o) :
                owner(o),
                start_time(clock::now())
        { }
        
        ticker(ticker&& src) :
                owner(src.owner),
                start_time(src.start_time)
        {
            src.owner = nullptr;
        }
        
        ~ticker()
        {
            if (owner)
                owner->add_time(clock::now() - start_time);
        }
    };
    
public:
    stopwatch() :
            tick_count(0),
            total_time(duration(0))
    { }
    
    ticker start()
    {
        return ticker(this);
    }
    
    void add_time(duration dur)
    {
        ++tick_count;
        total_time += dur;
    }
    
public:
    std::size_t tick_count;
    duration    total_time;
};

void test_jsonv(const std::string& input)
{
    jsonv::parse(input);
}

int main(int argc, char** argv)
{
    // First -- create a "pretty" encoded string that all tests will read from
    std::string encoded;
    {
        std::ostringstream encoded_stream;
        ostream_pretty_encoder out(encoded_stream);
        std::mt19937_64 rng{std::random_device()()};
        generated_json_settings settings;
        value val = generate_json(rng, settings);
        out.encode(val);
        encoded = encoded_stream.str();
        
        // save the string to a file in case we want to re-use it
        std::ofstream file("temp.json", std::ofstream::out | std::ofstream::trunc);
        file << encoded;
    }
    
    using test_function = void (*)(const std::string&);
    std::map<std::string, test_function> tests = {
            { "JSON Voorhees", test_jsonv }
        };
    
    for (const auto& pair : tests)
    {
        stopwatch watch;
        for (int idx = 0; idx < 10; ++idx)
        {
            std::cout << idx << '/' << 10 << std::endl;
            auto ticker = watch.start();
            pair.second(encoded);
        }
        
        auto average = std::chrono::duration_cast<std::chrono::microseconds>(watch.total_time) / watch.tick_count;
        std::cout << pair.first << '\t' << average.count() << "us" << std::endl;
    }
}
