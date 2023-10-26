//
// Created by blues on 2023/10/11.
//

#include <core/archive/StreamArchive.h>
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>

using namespace sky;

struct TestArchiveData {
    float a;
    uint64_t b;
    uint32_t c;
    uint16_t d;
    uint8_t e;
};

TEST(StreamArchiveTest, FStreamArchiveTestInOut)
{
    {
        std::fstream f("archive_test.bin", std::ios::out | std::ios::binary);
        OStreamArchive s(f);

        TestArchiveData t  = {};
        t.a = 0.1f;
        t.b = 2;
        t.c = 3;
        t.d = 4;
        t.e = 5;

        s << t.a << t.b << t.c << t.d << t.e;
    }

    {
        std::fstream f("archive_test.bin", std::ios::in | std::ios::binary);
        IStreamArchive s(f);

        TestArchiveData t = {};
        s >> t.a >> t.b >> t.c >> t.d >> t.e;

        ASSERT_EQ(t.a, 0.1f);
        ASSERT_EQ(t.b, 2);
        ASSERT_EQ(t.c, 3);
        ASSERT_EQ(t.d, 4);
        ASSERT_EQ(t.e, 5);
    }
}

TEST(StreamArchiveTest, SStreamArchiveTestInOut)
{
    std::stringstream f;
    {
        OStreamArchive s(f);

        TestArchiveData t  = {};
        t.a = 0.1f;
        t.b = 2;
        t.c = 3;
        t.d = 4;
        t.e = 5;

        s << t.a << t.b << t.c << t.d << t.e;
    }

    {
        IStreamArchive s(f);

        TestArchiveData t = {};
        s >> t.a >> t.b >> t.c >> t.d >> t.e;

        ASSERT_EQ(t.a, 0.1f);
        ASSERT_EQ(t.b, 2);
        ASSERT_EQ(t.c, 3);
        ASSERT_EQ(t.d, 4);
        ASSERT_EQ(t.e, 5);
    }
}