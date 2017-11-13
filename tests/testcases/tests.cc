/*
The MIT License (MIT)

Copyright (c) 2017 German Mendez Bravo (Kronuz) @ german dot mb at gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <gtest/gtest.h>

#include "base_x.hh"


constexpr auto test_base2 = BaseX<>("01", " \n\r\t");
constexpr auto test_base16 = BaseX<>("0123456789abcdef", " \n\r\t");
constexpr auto test_base58 = BaseX<>("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", " \n\r\t");


TEST(UUID, Encode) {
	EXPECT_EQ(base58::standard().encode("\330\105\140\310\23\117\21\346\241\342\64\66\73\322\155\256"), "ThxCy1Ek2q6UhWQhj9CK1o");
	EXPECT_EQ(base62::standard().encode("\330\105\140\310\23\117\21\346\241\342\64\66\73\322\155\256"), "6a630O1jrtMjCrQDyG3D3O");
}

TEST(base58, Encoder) {
	EXPECT_EQ(base58::standard().encode("Hello world!"), "2NEpo7TZRhna7vSvL");
	EXPECT_EQ(base58::gmp().decode<uint256_t>("1TFvCj"), 987654321);
	EXPECT_EQ(base58::gmp().encode(987654321), "1TFvCj");
	EXPECT_EQ(base58::gmp().encode("Hello world!"), "1LDlk6QWOejX6rPrJ");
}

TEST(base62, Encoder) {
	EXPECT_EQ(base62::standard().decode<uint256_t>("14q60P"), 987654321);
	EXPECT_EQ(base62::standard().encode(987654321), "14q60P");
	EXPECT_EQ(base62::standard().encode("Hello world!"), "T8dgcjRGuYUueWht");
	EXPECT_EQ(base62::inverted().encode("Hello world!"), "t8DGCJrgUyuUEwHT");
}

TEST(base58, ShouldEncodeAndDecodeIntegers) {
	auto data = 987654321;

	auto gmpEncoded = base58::gmp().encode(data);
	auto bitcoinEncoded = base58::bitcoin().encode(data);
	auto rippleEncoded = base58::ripple().encode(data);
	auto flickrEncoded = base58::flickr().encode(data);

	EXPECT_EQ(gmpEncoded, "1TFvCj");
	EXPECT_EQ(bitcoinEncoded, "2WGzDn");
	EXPECT_EQ(rippleEncoded, "pWGzD8");
	EXPECT_EQ(flickrEncoded, "2vgZdM");

	auto gmpDecoded = base58::gmp().decode<uint256_t>(gmpEncoded);
	auto bitcoinDecoded = base58::bitcoin().decode<uint256_t>(bitcoinEncoded);
	auto rippleDecoded = base58::ripple().decode<uint256_t>(rippleEncoded);
	auto flickrDecoded = base58::flickr().decode<uint256_t>(flickrEncoded);

	EXPECT_EQ(gmpDecoded, data);
	EXPECT_EQ(bitcoinDecoded, data);
	EXPECT_EQ(rippleDecoded, data);
	EXPECT_EQ(flickrDecoded, data);

	auto encoded = base58::standard().encode(data);
	auto decoded = base58::standard().decode<uint256_t>(encoded);

	EXPECT_EQ(decoded, data);
}

TEST(base58, LongText) {
	auto data = "Lorem ipsum dolor consectetur.";

	auto gmpEncoded = base58::gmp().encode(data);
	auto bitcoinEncoded = base58::bitcoin().encode(data);
	auto rippleEncoded = base58::ripple().encode(data);
	auto flickrEncoded = base58::flickr().encode(data);

	EXPECT_EQ(gmpEncoded, "FIHZQEpJ739QdqChX1PkgTBqP1FaDgJWQiGvY92YA");
	EXPECT_EQ(bitcoinEncoded, "GKJcTFtL84ATguDka2SojWCuS2GdEjLZTmHzbA3bB");
	EXPECT_EQ(rippleEncoded, "GKJcTEtL3hwTguDk2pSojWUuSpGdNjLZTmHzbwsbB");
	EXPECT_EQ(flickrEncoded, "gjiBsfTk84asFUdKz2rNJvcUr2gCeJkysLhZAa3Ab");

	auto gmpDecoded = base58::gmp().decode(gmpEncoded);
	auto bitcoinDecoded = base58::bitcoin().decode(bitcoinEncoded);
	auto rippleDecoded = base58::ripple().decode(rippleEncoded);
	auto flickrDecoded = base58::flickr().decode(flickrEncoded);

	EXPECT_EQ(gmpDecoded, data);
	EXPECT_EQ(bitcoinDecoded, data);
	EXPECT_EQ(rippleDecoded, data);
	EXPECT_EQ(flickrDecoded, data);
}

TEST(base58, Tests) {
	EXPECT_EQ(test_base2.encode(uint256_t("000f", 16)), "1111");
	// EXPECT_EQ(test_base2.encode(uint256_t("00ff", 16)), "011111111"); // ->>
	EXPECT_EQ(test_base2.encode(uint256_t("00ff", 16)), "11111111");
	EXPECT_EQ(test_base2.encode(uint256_t("0fff", 16)), "111111111111");
	EXPECT_EQ(test_base2.encode(uint256_t("ff00ff00", 16)), "11111111000000001111111100000000");
	// EXPECT_EQ(test_base16.encode(uint256_t("0000000f", 16)), "000f"); // ->>
	EXPECT_EQ(test_base16.encode(uint256_t("0000000f", 16)), "f");
	// EXPECT_EQ(test_base16.encode(uint256_t("000fff", 16)), "0fff"); // ->>
	EXPECT_EQ(test_base16.encode(uint256_t("000fff", 16)), "fff");
	EXPECT_EQ(test_base16.encode(uint256_t("ffff", 16)), "ffff");
	// EXPECT_EQ(test_base58.encode(uint256_t("", 16)), ""); // ->>
	EXPECT_EQ(test_base58.encode(uint256_t("", 16)), "1");
	EXPECT_EQ(test_base58.encode(uint256_t("61", 16)), "2g");
	EXPECT_EQ(test_base58.encode(uint256_t("626262", 16)), "a3gV");
	EXPECT_EQ(test_base58.encode(uint256_t("636363", 16)), "aPEr");
	EXPECT_EQ(test_base58.encode(uint256_t("73696d706c792061206c6f6e6720737472696e67", 16)), "2cFupjhnEsSn59qHXstmK2ffpLv2");
	// EXPECT_EQ(test_base58.encode(uint256_t("00eb15231dfceb60925886b67d065299925915aeb172c06647", 16)), "1NS17iag9jJgTHD1VXjvLCEnZuQ3rJDE9L"); // ->>
	EXPECT_EQ(test_base58.encode(uint256_t("00eb15231dfceb60925886b67d065299925915aeb172c06647", 16)), "NS17iag9jJgTHD1VXjvLCEnZuQ3rJDE9L");
	EXPECT_EQ(test_base58.encode(uint256_t("516b6fcd0f", 16)), "ABnLTmg");
	EXPECT_EQ(test_base58.encode(uint256_t("bf4f89001e670274dd", 16)), "3SEo3LWLoPntC");
	EXPECT_EQ(test_base58.encode(uint256_t("572e4794", 16)), "3EFU7m");
	EXPECT_EQ(test_base58.encode(uint256_t("ecac89cad93923c02321", 16)), "EJDM8drfXA6uyA");
	EXPECT_EQ(test_base58.encode(uint256_t("10c8511e", 16)), "Rt5zm");
	// EXPECT_EQ(test_base58.encode(uint256_t("00000000000000000000", 16)), "1111111111"); // ->>
	EXPECT_EQ(test_base58.encode(uint256_t("00000000000000000000", 16)), "1");
	// EXPECT_EQ(test_base58.encode(uint256_t("801184cd2cdd640ca42cfc3a091c51d549b2f016d454b2774019c2b2d2e08529fd206ec97e", 16)), "5Hx15HFGyep2CfPxsJKe2fXJsCVn5DEiyoeGGF6JZjGbTRnqfiD");  // too long for 256 bits
	// EXPECT_EQ(test_base58.encode(uint256_t("003c176e659bea0f29a3e9bf7880c112b1b31b4dc826268187", 16)), "16UjcYNBG9GTK4uq2f7yYEbuifqCzoLMGS"); // ->>
	EXPECT_EQ(test_base58.encode(uint256_t("003c176e659bea0f29a3e9bf7880c112b1b31b4dc826268187", 16)), "6UjcYNBG9GTK4uq2f7yYEbuifqCzoLMGS");
	EXPECT_EQ(test_base58.encode(uint256_t("ffffffffffffffffffff", 16)), "FPBt6CHo3fovdL");
	EXPECT_EQ(test_base58.encode(uint256_t("ffffffffffffffffffffffffff", 16)), "NKioeUVktgzXLJ1B3t");
	EXPECT_EQ(test_base58.encode(uint256_t("ffffffffffffffffffffffffffffffff", 16)), "YcVfxkQb6JRzqk5kF2tNLv");
	EXPECT_EQ(test_base2.encode(uint256_t("fb6f9ac3", 16)), "11111011011011111001101011000011");
	EXPECT_EQ(test_base2.encode(uint256_t("179eea7a", 16)), "10111100111101110101001111010");
	EXPECT_EQ(test_base2.encode(uint256_t("6db825db", 16)), "1101101101110000010010111011011");
	EXPECT_EQ(test_base2.encode(uint256_t("93976aa7", 16)), "10010011100101110110101010100111");
	EXPECT_EQ(test_base58.encode(uint256_t("ef41b9ce7e830af7", 16)), "h26E62FyLQN");
	EXPECT_EQ(test_base58.encode(uint256_t("606cbc791036d2e9", 16)), "H8Sa62HVULG");
	EXPECT_EQ(test_base58.encode(uint256_t("bdcb0ea69c2c8ec8", 16)), "YkESUPpnfoD");
	EXPECT_EQ(test_base58.encode(uint256_t("1a2358ba67fb71d5", 16)), "5NaBN89ajtQ");
	EXPECT_EQ(test_base58.encode(uint256_t("e6173f0f4d5fb5d7", 16)), "fVAoezT1ZkS");
	EXPECT_EQ(test_base58.encode(uint256_t("91c81cbfdd58bbd2", 16)), "RPGNSU3bqTX");
	EXPECT_EQ(test_base58.encode(uint256_t("329e0bf0e388dbfe", 16)), "9U41ZkwwysT");
	EXPECT_EQ(test_base58.encode(uint256_t("30b10393210fa65b", 16)), "99NMW3WHjjY");
	EXPECT_EQ(test_base58.encode(uint256_t("ab3bdd18e3623654", 16)), "VeBbqBb4rCT");
	EXPECT_EQ(test_base58.encode(uint256_t("fe29d1751ec4af8a", 16)), "jWhmYLN9dUm");
	EXPECT_EQ(test_base58.encode(uint256_t("c1273ab5488769807d", 16)), "3Tbh4kL3WKW6g");
	EXPECT_EQ(test_base58.encode(uint256_t("6c7907904de934f852", 16)), "2P5jNYhfpTJxy");
	EXPECT_EQ(test_base58.encode(uint256_t("05f0be055db47a0dc9", 16)), "5PN768Kr5oEp");
	EXPECT_EQ(test_base58.encode(uint256_t("3511e6206829b35b12", 16)), "gBREojGaJ6DF");
	EXPECT_EQ(test_base58.encode(uint256_t("d1c7c2ddc4a459d503", 16)), "3fsekq5Esq2KC");
	EXPECT_EQ(test_base58.encode(uint256_t("1f88efd17ab073e9a1", 16)), "QHJbmW9ZY7jn");
	EXPECT_EQ(test_base58.encode(uint256_t("0f45dadf4e64c5d5c2", 16)), "CGyVUMmCKLRf");
	EXPECT_EQ(test_base58.encode(uint256_t("de1e5c5f718bb7fafa", 16)), "3pyy8U7w3KUa5");
	EXPECT_EQ(test_base58.encode(uint256_t("123190b93e9a49a46c", 16)), "ES3DeFrG1zbd");
	EXPECT_EQ(test_base58.encode(uint256_t("8bee94a543e7242e5a", 16)), "2nJnuWyLpGf6y");
	EXPECT_EQ(test_base58.encode(uint256_t("9fd5f2285362f5cfd834", 16)), "9yqFhqeewcW3pF");
	EXPECT_EQ(test_base58.encode(uint256_t("6987bac63ad23828bb31", 16)), "6vskE5Y1LhS3U4");
	EXPECT_EQ(test_base58.encode(uint256_t("19d4a0f9d459cc2a08b0", 16)), "2TAsHPuaLhh5Aw");
	EXPECT_EQ(test_base58.encode(uint256_t("a1e47ffdbea5a807ab26", 16)), "A6XzPgSUJDf1W5");
	EXPECT_EQ(test_base58.encode(uint256_t("35c231e5b3a86a9b83db", 16)), "42B8reRwPAAoAa");
	EXPECT_EQ(test_base58.encode(uint256_t("b2351012a48b8347c351", 16)), "B1hPyomGx4Vhqa");
	EXPECT_EQ(test_base58.encode(uint256_t("71d402694dd9517ea653", 16)), "7Pv2SyAQx2Upu8");
	EXPECT_EQ(test_base58.encode(uint256_t("55227c0ec7955c2bd6e8", 16)), "5nR64BkskyjHMq");
	EXPECT_EQ(test_base58.encode(uint256_t("17b3d8ee7907c1be34df", 16)), "2LEg7TxosoxTGS");
	EXPECT_EQ(test_base58.encode(uint256_t("7e7bba7b68bb8e95827f", 16)), "879o2ATGnmYyAW");
	EXPECT_EQ(test_base58.encode(uint256_t("db9c13f5ba7654b01407fb", 16)), "wTYfxjDVbiks874");
	EXPECT_EQ(test_base58.encode(uint256_t("6186449d20f5fd1e6c4393", 16)), "RBeiWhzZNL6VtMG");
	EXPECT_EQ(test_base58.encode(uint256_t("5248751cebf4ad1c1a83c3", 16)), "MQSVNnc8ehFCqtW");
	EXPECT_EQ(test_base58.encode(uint256_t("32090ef18cd479fc376a74", 16)), "DQdu351ExDaeYeX");
	EXPECT_EQ(test_base58.encode(uint256_t("7cfa5d6ed1e467d986c426", 16)), "XzW67T5qfEnFcaZ");
	EXPECT_EQ(test_base58.encode(uint256_t("9d8707723c7ede51103b6d", 16)), "g4eTCg6QJnB1UU4");
	EXPECT_EQ(test_base58.encode(uint256_t("6f4d1e392d6a9b4ed8b223", 16)), "Ubo7kZY5aDpAJp2");
	EXPECT_EQ(test_base58.encode(uint256_t("38057d98797cd39f80a0c9", 16)), "EtjQ2feamJvuqse");
	EXPECT_EQ(test_base58.encode(uint256_t("de7e59903177e20880e915", 16)), "xB2N7yRBnDYEoT2");
	EXPECT_EQ(test_base58.encode(uint256_t("b2ea24a28bc4a60b5c4b8d", 16)), "mNFMpJ2P3TGYqhv");
	EXPECT_EQ(test_base58.encode(uint256_t("cf84938958589b6ffba6114d", 16)), "4v8ZbsGh2ePz5sipt");
	EXPECT_EQ(test_base58.encode(uint256_t("dee13be7b8d8a08c94a3c02a", 16)), "5CwmE9jQqwtHkTF45");
	EXPECT_EQ(test_base58.encode(uint256_t("14cb9c6b3f8cd2e02710f569", 16)), "Pm85JHVAAdeUdxtp");
	EXPECT_EQ(test_base58.encode(uint256_t("ca3f2d558266bdcc44c79cb5", 16)), "4pMwomBAQHuUnoLUC");
	EXPECT_EQ(test_base58.encode(uint256_t("c031215be44cbad745f38982", 16)), "4dMeTrcxiVw9RWvj3");
	EXPECT_EQ(test_base58.encode(uint256_t("1435ab1dbc403111946270a5", 16)), "P7wX3sCWNrbqhBEC");
	EXPECT_EQ(test_base58.encode(uint256_t("d8c6e4d775e7a66a0d0f9f41", 16)), "56GLoRDGWGuGJJwPN");
	EXPECT_EQ(test_base58.encode(uint256_t("dcee35e74f0fd74176fce2f4", 16)), "5Ap1zyuYiJJFwWcMR");
	EXPECT_EQ(test_base58.encode(uint256_t("bfcc0ca4b4855d1cf8993fc0", 16)), "4cvafQW4PEhARKv9D");
	EXPECT_EQ(test_base58.encode(uint256_t("e02a3ac25ece7b54584b670a", 16)), "5EMM28xkpxZ1kkVUM");
	EXPECT_EQ(test_base58.encode(uint256_t("fe4d938fc3719f064cabb4bfff", 16)), "NBXKkbHwrAsiWTLAk6");
	EXPECT_EQ(test_base58.encode(uint256_t("9289cb4f6b15c57e6086b87ea5", 16)), "DCvDpjEXEbHjZqskKv");
	EXPECT_EQ(test_base58.encode(uint256_t("fc266f35626b3612bfe978537b", 16)), "N186PVoBWrNre35BGE");
	EXPECT_EQ(test_base58.encode(uint256_t("33ff08c06d92502bf258c07166", 16)), "5LC4SoW6jmTtbkbePw");
	EXPECT_EQ(test_base58.encode(uint256_t("6a81cac1f3666bc59dc67b1c3c", 16)), "9sXgUySUzwiqDU5WHy");
	EXPECT_EQ(test_base58.encode(uint256_t("9dfb8e7e744c544c0f323ea729", 16)), "EACsmGmkgcwsrPFzLg");
	EXPECT_EQ(test_base58.encode(uint256_t("1e7a1e284f70838b38442b682b", 16)), "3YEVk9bE7rw5qExMkv");
	EXPECT_EQ(test_base58.encode(uint256_t("2a862ad57901a8235f5dc74eaf", 16)), "4YS259nuTLfeXa5Wuc");
	EXPECT_EQ(test_base58.encode(uint256_t("74c82096baef21f9d3089e5462", 16)), "AjAcKEhUfrqm8smvM7");
	EXPECT_EQ(test_base58.encode(uint256_t("7a3edbc23d7b600263920261cc", 16)), "BBZXyRgey5S5DDZkcK");
	EXPECT_EQ(test_base58.encode(uint256_t("20435664c357d25a9c8df751cf4f", 16)), "CrwNL6Fbv4pbRx1zd9g");
	EXPECT_EQ(test_base58.encode(uint256_t("51a7aa87cf5cb1c12d045ec3422d", 16)), "X27NHGgKXmGzzQvDtpC");
	EXPECT_EQ(test_base58.encode(uint256_t("344d2e116aa26f1062a2cb6ebbef", 16)), "LEDLDvL1Hg4qt1efVXt");
	EXPECT_EQ(test_base58.encode(uint256_t("6941add7be4c0b5c7163e4928f8e", 16)), "fhMyN6gwoxE3uYraVzV");
	EXPECT_EQ(test_base58.encode(uint256_t("10938fcbb7c4ab991649734a14bf", 16)), "76TPrSDxzGQfSzMu974");
	EXPECT_EQ(test_base58.encode(uint256_t("eafe04d944ba504e9af9117b07de", 16)), "2VPgov563ryfe4L2Bj6M");
	EXPECT_EQ(test_base58.encode(uint256_t("58d0aeed4d35da20b6f052127edf", 16)), "ZenZhXF9YwP8nQvNtNz");
	EXPECT_EQ(test_base58.encode(uint256_t("d734984e2f5aecf25f7a3e353f8a", 16)), "2N7n3jFsTdyN49Faoq6h");
	EXPECT_EQ(test_base58.encode(uint256_t("57d873fdb405b7daf4bafa62068a", 16)), "ZJ7NwoP4wHvwyZg3Wjs");
	EXPECT_EQ(test_base58.encode(uint256_t("bda4ec7b40d0d65ca95dec4c4d3b", 16)), "2CijxjsNyvqTwPCfDcpA");
	EXPECT_EQ(test_base58.encode(uint256_t("826c4abdceb1b91f0d4ad665f86d2e", 16)), "4edfvuDQu9KzVxLuXHfMo");
	EXPECT_EQ(test_base58.encode(uint256_t("e7ecb35d07e65b960cb10574a4f51a", 16)), "7VLRYdB4cToipp2J2p3v9");
	EXPECT_EQ(test_base58.encode(uint256_t("4f2d72ead87b31d6869fba39eac6dc", 16)), "3DUjqJRcfdWhpsrLrGcQs");
	EXPECT_EQ(test_base58.encode(uint256_t("8b4f5788d60030950d5dfbf94c585d", 16)), "4u44JSRH5jP5X39YhPsmE");
	EXPECT_EQ(test_base58.encode(uint256_t("ee4c0a0025d1a74ace9fe349355cc5", 16)), "7fgACjABRQUGUEpN6VBBA");
	EXPECT_EQ(test_base58.encode(uint256_t("58ac05b9a0b4b66083ff1d489b8d84", 16)), "3UtJPyTwGXapcxHx8Rom5");
	EXPECT_EQ(test_base58.encode(uint256_t("1aa35c05e1132e8e049aafaef035d8", 16)), "kE2eSU7gM2619pT82iGP");
	EXPECT_EQ(test_base58.encode(uint256_t("771b0c28608484562a292e5d5d2b30", 16)), "4LGYeWhyfrjUByibUqdVR");
	EXPECT_EQ(test_base58.encode(uint256_t("78ff9a0e56f9e88dc1cd654b40d019", 16)), "4PLggs66qAdbmZgkaPihe");
	EXPECT_EQ(test_base58.encode(uint256_t("6d691bdd736346aa5a0a95b373b2ab", 16)), "44Y6qTgSvRMkdqpQ5ufkN");
}
