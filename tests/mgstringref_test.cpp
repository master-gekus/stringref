#include "mgstringref.h"

#include <cassert>
#include <new>
#include <string>
#include <map>

#include <gtest/gtest.h>

namespace inplace {
    template <typename T>
    class allocator
    {
    public:
        typedef T value_type;

        allocator() = delete;

        inline allocator(void* buffer, size_t buffer_size, size_t block_size = 4096) :
            buffer_(static_cast<char*>(buffer)),
            block_size_(block_size)
        {
            if ((nullptr == buffer_) || (0 == buffer_size) || ((sizeof(std::size_t) + sizeof(T)) > block_size)
                || (block_size > buffer_size)) {
                throw std::bad_alloc();
            }
            block_count_ = buffer_size / block_size;
            memset(buffer_, 0, block_size_ * block_count_);
        }

        template <class U>
        inline allocator(const allocator<U>& other) noexcept :
            buffer_(other.buffer_),
            block_size_(other.block_size_),
            block_count_(other.block_count_)
        {
        }

        T* allocate(std::size_t n)
        {
            if ((n * sizeof(T)) > (block_size_ - sizeof(size_t))) {
                throw std::bad_alloc();
            }
            for (size_t i = 0; i < block_count_; i++) {
                std::size_t *header = reinterpret_cast<std::size_t*>(buffer_ + (i * block_size_));
                if (0 == (*header)) {
                    (*header) = (n * sizeof(T));
                    return reinterpret_cast<T*>(buffer_ + i * block_size_ + sizeof(size_t));
                }
            }
            throw std::bad_alloc();
        }

        void deallocate(T* p, std::size_t n)
        {
            size_t offset = reinterpret_cast<char*>(p) - buffer_ - sizeof(size_t);
            std::size_t *header = reinterpret_cast<std::size_t*>(buffer_ + offset);
            if (offset > (block_count_ * block_size_) || (0 != (offset % block_size_))
                || ((n * sizeof(T)) != (*header))) {
                throw std::bad_alloc();
            }
            (*header) = 0;
        }

        size_t used_block_count()
        {
            size_t count = 0;
            for (size_t i = 0; i < block_count_; i++) {
                if (0 != buffer_[i * block_size_]) {
                    count++;
                }
            }
            return count;
        }

    private:
        char *buffer_;
        size_t block_size_;
        size_t block_count_;

        template<typename U> friend class allocator;
    };

    typedef std::basic_string<char, std::char_traits<char>, allocator<char> > string;
    typedef std::basic_string<char16_t, std::char_traits<char16_t>, allocator<char16_t> > ustring;
    typedef mg::basic_stringref<char, std::char_traits<char>, allocator<char> > stringref;
    typedef mg::basic_stringref<char16_t, std::char_traits<char16_t>, allocator<char16_t> > ustringref;
}

class StandardAllocator : public ::testing::Test
{
};

class CustomAllocator : public ::testing::Test
{
public:
    CustomAllocator() :
        a(buf, sizeof(buf))
    {}

protected:
    char buf[4096 * 100];
    ::inplace::allocator<char> a;

    void TearDown() override
    {
        EXPECT_EQ(a.used_block_count(), static_cast<std::size_t>(0));
    }
};

TEST_F(CustomAllocator, StandardContainers)
{
    using namespace inplace;
    string *s = new string("String must be lenght enough to allocated memory.", a);
    string s1(*s);
    string s2 = *s;
    EXPECT_EQ(a.used_block_count(), static_cast<std::size_t>(3));
    delete s;
    EXPECT_EQ(s1, s2);
    EXPECT_EQ(a.used_block_count(), static_cast<std::size_t>(2));

    std::map<string, string, std::less<string>, allocator<std::map<string, string>::value_type> > m(a);
    m.emplace(string("String must be lenght enough to allocated memory.", a),
              string("String must be lenght enough to allocated memory.", a));

    EXPECT_EQ(m.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(m.begin()->first, s1);
}

TEST_F(StandardAllocator, EmptyConstrution)
{
    using namespace mg;
    stringref s;
    ustringref us;
    EXPECT_TRUE(s.empty());
    EXPECT_TRUE(us.empty());
}

TEST_F(CustomAllocator, EmptyConstrution)
{
    using namespace inplace;
    stringref s(a);
    ustringref us(a);
    EXPECT_EQ(a.used_block_count(), static_cast<std::size_t>(0));
    EXPECT_TRUE(s.empty());
    EXPECT_TRUE(us.empty());
}

TEST_F(StandardAllocator, ConstrutionFromConstString)
{
    using namespace mg;
    stringref s("Test string.");
    ustringref us(u"Test string.");
    EXPECT_EQ(s.size(), static_cast<std::size_t>(12));
    EXPECT_EQ(us.size(), static_cast<std::size_t>(12));

    stringref s1("Test string.", 4);
    ustringref us1(u"Test string.", 4);
    EXPECT_EQ(s1.size(), static_cast<std::size_t>(4));
    EXPECT_EQ(us1.size(), static_cast<std::size_t>(4));

    stringref s2("Test string.", 5, 6);
    ustringref us2(u"Test string.", 5, 6);
    EXPECT_EQ(s2.size(), static_cast<std::size_t>(6));
    EXPECT_EQ(us2.size(), static_cast<std::size_t>(6));

    stringref s4("Test", 5, 6);
    ustringref us4(u"Test", 5, 6);
    EXPECT_TRUE(s4.empty());
    EXPECT_TRUE(us4.empty());

    stringref s5("Test", 1, 6);
    ustringref us5(u"Test", 1, 6);
    EXPECT_EQ(s5.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(us5.size(), static_cast<std::size_t>(3));
}

TEST_F(CustomAllocator, ConstrutionFromConstString)
{
    using namespace inplace;
    stringref s("Test string.", a);
    ustringref us(u"Test string.", a);
    EXPECT_EQ(a.used_block_count(), static_cast<std::size_t>(2));
    EXPECT_EQ(s.size(), static_cast<std::size_t>(12));
    EXPECT_EQ(us.size(), static_cast<std::size_t>(12));

    stringref s1("Test string.", 4, a);
    ustringref us1(u"Test string.", 4, a);
    EXPECT_EQ(a.used_block_count(), static_cast<std::size_t>(4));
    EXPECT_EQ(s1.size(), static_cast<std::size_t>(4));
    EXPECT_EQ(us1.size(), static_cast<std::size_t>(4));

    stringref s2("Test string.", 5, 6, a);
    ustringref us2(u"Test string.", 5, 6, a);
    EXPECT_EQ(a.used_block_count(), static_cast<std::size_t>(6));
    EXPECT_EQ(s2.size(), static_cast<std::size_t>(6));
    EXPECT_EQ(us2.size(), static_cast<std::size_t>(6));

    stringref s4("Test", 5, 6, a);
    ustringref us4(u"Test", 5, 6, a);
    EXPECT_EQ(a.used_block_count(), static_cast<std::size_t>(6));
    EXPECT_TRUE(s4.empty());
    EXPECT_TRUE(us4.empty());

    stringref s5("Test", 1, 6, a);
    ustringref us5(u"Test", 1, 6, a);
    EXPECT_EQ(a.used_block_count(), static_cast<std::size_t>(8));
    EXPECT_EQ(s5.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(us5.size(), static_cast<std::size_t>(3));
}

