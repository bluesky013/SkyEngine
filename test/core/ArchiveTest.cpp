//
// Created by blues on 2023/10/11.
//

#include <core/archive/StreamArchive.h>
#include <core/archive/MemoryArchive.h>
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

TEST(StreamArchiveTest, MemoryArchive)
{
    std::string t("test memory archive");

    MemoryArchive archive;
    {
        archive << 1;   // 4
        archive << 0.f; // 4
        archive << 3.0; // 8
        archive << t; // 4 + 4
    }

    {
        int a;
        archive >> a;
        float b;
        archive >> b;
        double c;
        archive >> c;
        std::string d;
        archive >> d;

        ASSERT_EQ(a, 1);
        ASSERT_EQ(b, 0.f);
        ASSERT_EQ(c, 3.0);
        ASSERT_EQ(d, t);
    }

    ASSERT_EQ(archive.Size(), (4 + 4 + 8 + (4 + t.size())));
}