#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/BinaryArchive.h>
#include <gtest/gtest.h>

using namespace sky;

namespace {
    class FailingOutput : public OStreamArchive {
    public:
        explicit FailingOutput(std::ostream &stream) : OStreamArchive(stream)
        {
        }
        bool SaveRaw(const char *, size_t) override
        {
            ++calls;
            return false;
        }
        uint32_t calls = 0;
    };
} // namespace

TEST(BinaryArchiveStatus, ShortReadIsStickyWithoutStreamFailbit)
{
    const char         bytes[] = {1, 2, 3};
    IMemoryArchive     input(bytes, sizeof(bytes));
    BinaryInputArchive reader(input);
    uint32_t           value = 0;
    reader.LoadValue(value);
    EXPECT_FALSE(reader.IsValid());
    EXPECT_TRUE(input.GetStream().good());
    uint32_t unchanged = 42;
    reader.LoadValue(unchanged);
    EXPECT_EQ(unchanged, 42U);
    EXPECT_FALSE(reader.IsValid());
}

TEST(BinaryArchiveStatus, FailedWriteIsSticky)
{
    OMemoryArchive      bytes;
    FailingOutput       output(bytes.GetStream());
    BinaryOutputArchive writer(output);
    writer.SaveValue(42U);
    EXPECT_FALSE(writer.IsValid());
    writer.SaveValue(43U);
    EXPECT_FALSE(writer.IsValid());
    EXPECT_EQ(output.calls, 1U);
}
