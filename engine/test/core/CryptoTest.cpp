//
// Created by blues on 2024/12/29.
//

#include <core/crypto/md5/MD5.h>
#include <gtest/gtest.h>
#include <map>

using namespace sky;

TEST(CryptoTest, MD5Test)
{
    ASSERT_EQ(MD5::CalculateMD5("").ToString(), "d41d8cd98f00b204e9800998ecf8427e");
    ASSERT_EQ(MD5::CalculateMD5("a").ToString(), "0cc175b9c0f1b6a831c399e269772661");
    ASSERT_EQ(MD5::CalculateMD5("abc").ToString(), "900150983cd24fb0d6963f7d28e17f72");
    ASSERT_EQ(MD5::CalculateMD5("message digest").ToString(), "f96b697d7cb7938d525a2f31aaf161d0");
    ASSERT_EQ(MD5::CalculateMD5("abcdefghijklmnopqrstuvwxyz").ToString(), "c3fcd3d76192e4007dfb496cca67e13b");
    ASSERT_EQ(MD5::CalculateMD5("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789").ToString(), "d174ab98d277d9f5a5611c2c9f419d9f");
    ASSERT_EQ(MD5::CalculateMD5("12345678901234567890123456789012345678901234567890123456789012345678901234567890").ToString(), "57edf4a22be3c955ac49da2e2107b67a");
}