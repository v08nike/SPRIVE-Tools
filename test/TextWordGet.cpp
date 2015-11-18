// Copyright (c) 2015 The Khronos Group Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and/or associated documentation files (the
// "Materials"), to deal in the Materials without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Materials, and to
// permit persons to whom the Materials are furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Materials.
//
// MODIFICATIONS TO THIS FILE MAY MEAN IT NO LONGER ACCURATELY REFLECTS
// KHRONOS STANDARDS. THE UNMODIFIED, NORMATIVE VERSIONS OF KHRONOS
// SPECIFICATIONS AND HEADER INFORMATION ARE LOCATED AT
//    https://www.khronos.org/registry/
//
// THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.

#include "UnitSPIRV.h"

namespace {

using libspirv::AssemblyContext;
using spvtest::AutoText;

#define TAB "\t"
#define NEWLINE "\n"
#define BACKSLASH R"(\)"
#define QUOTE R"(")"

TEST(TextWordGet, NullTerminator) {
  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS, AssemblyContext(AutoText("Word"), nullptr)
                             .getWord(word, &endPosition));
  ASSERT_EQ(4u, endPosition.column);
  ASSERT_EQ(0u, endPosition.line);
  ASSERT_EQ(4u, endPosition.index);
  ASSERT_STREQ("Word", word.c_str());
}

TEST(TextWordGet, TabTerminator) {
  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS, AssemblyContext(AutoText("Word\t"), nullptr)
                             .getWord(word, &endPosition));
  ASSERT_EQ(4u, endPosition.column);
  ASSERT_EQ(0u, endPosition.line);
  ASSERT_EQ(4u, endPosition.index);
  ASSERT_STREQ("Word", word.c_str());
}

TEST(TextWordGet, SpaceTerminator) {
  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS, AssemblyContext(AutoText("Word "), nullptr)
                             .getWord(word, &endPosition));
  ASSERT_EQ(4u, endPosition.column);
  ASSERT_EQ(0u, endPosition.line);
  ASSERT_EQ(4u, endPosition.index);
  ASSERT_STREQ("Word", word.c_str());
}

TEST(TextWordGet, SemicolonTerminator) {
  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS, AssemblyContext(AutoText("Wo;rd"), nullptr)
                             .getWord(word, &endPosition));
  ASSERT_EQ(2u, endPosition.column);
  ASSERT_EQ(0u, endPosition.line);
  ASSERT_EQ(2u, endPosition.index);
  ASSERT_STREQ("Wo", word.c_str());
}

TEST(TextWordGet, MultipleWords) {
  AutoText input("Words in a sentence");
  AssemblyContext data(input, nullptr);

  spv_position_t endPosition = {};
  const char* words[] = {"Words", "in", "a", "sentence"};

  std::string word;
  for (uint32_t wordIndex = 0; wordIndex < 4; ++wordIndex) {
    ASSERT_EQ(SPV_SUCCESS, data.getWord(word, &endPosition));
    ASSERT_EQ(strlen(words[wordIndex]),
              endPosition.column - data.position().column);
    ASSERT_EQ(0u, endPosition.line);
    ASSERT_EQ(strlen(words[wordIndex]),
              endPosition.index - data.position().index);
    ASSERT_STREQ(words[wordIndex], word.c_str());

    data.setPosition(endPosition);
    if (3 != wordIndex) {
      ASSERT_EQ(SPV_SUCCESS, data.advance());
    } else {
      ASSERT_EQ(SPV_END_OF_STREAM, data.advance());
    }
  }
}

TEST(TextWordGet, QuotesAreKept) {
  AutoText input(R"("quotes" "around words")");
  const char* expected[] = {R"("quotes")", R"("around words")"};
  AssemblyContext data(input, nullptr);

  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS, data.getWord(word, &endPosition));
  EXPECT_EQ(8u, endPosition.column);
  EXPECT_EQ(0u, endPosition.line);
  EXPECT_EQ(8u, endPosition.index);
  EXPECT_STREQ(expected[0], word.c_str());

  // Move to the next word.
  data.setPosition(endPosition);
  data.seekForward(1);

  ASSERT_EQ(SPV_SUCCESS, data.getWord(word, &endPosition));
  EXPECT_EQ(23u, endPosition.column);
  EXPECT_EQ(0u, endPosition.line);
  EXPECT_EQ(23u, endPosition.index);
  EXPECT_STREQ(expected[1], word.c_str());
}

TEST(TextWordGet, QuotesBetweenWordsActLikeGlue) {
  AutoText input(R"(quotes" "between words)");
  const char* expected[] = {R"(quotes" "between)", "words"};
  AssemblyContext data(input, nullptr);

  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS, data.getWord(word, &endPosition));
  EXPECT_EQ(16u, endPosition.column);
  EXPECT_EQ(0u, endPosition.line);
  EXPECT_EQ(16u, endPosition.index);
  EXPECT_STREQ(expected[0], word.c_str());

  // Move to the next word.
  data.setPosition(endPosition);
  data.seekForward(1);

  ASSERT_EQ(SPV_SUCCESS, data.getWord(word, &endPosition));
  EXPECT_EQ(22u, endPosition.column);
  EXPECT_EQ(0u, endPosition.line);
  EXPECT_EQ(22u, endPosition.index);
  EXPECT_STREQ(expected[1], word.c_str());
}

TEST(TextWordGet, QuotingWhitespace) {
  AutoText input(QUOTE "white " NEWLINE TAB " space" QUOTE);
  // Whitespace surrounded by quotes acts like glue.
  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS,
            AssemblyContext(input, nullptr).getWord(word, &endPosition));
  EXPECT_EQ(input.str.length(), endPosition.column);
  EXPECT_EQ(0u, endPosition.line);
  EXPECT_EQ(input.str.length(), endPosition.index);
  EXPECT_EQ(input.str, word);
}

TEST(TextWordGet, QuoteAlone) {
  AutoText input(QUOTE);
  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS,
            AssemblyContext(input, nullptr).getWord(word, &endPosition));
  ASSERT_EQ(1u, endPosition.column);
  ASSERT_EQ(0u, endPosition.line);
  ASSERT_EQ(1u, endPosition.index);
  ASSERT_STREQ(QUOTE, word.c_str());
}

TEST(TextWordGet, EscapeAlone) {
  AutoText input(BACKSLASH);
  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS,
            AssemblyContext(input, nullptr).getWord(word, &endPosition));
  ASSERT_EQ(1u, endPosition.column);
  ASSERT_EQ(0u, endPosition.line);
  ASSERT_EQ(1u, endPosition.index);
  ASSERT_STREQ(BACKSLASH, word.c_str());
}

TEST(TextWordGet, EscapeAtEndOfInput) {
  AutoText input("word" BACKSLASH);
  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS,
            AssemblyContext(input, nullptr).getWord(word, &endPosition));
  ASSERT_EQ(5u, endPosition.column);
  ASSERT_EQ(0u, endPosition.line);
  ASSERT_EQ(5u, endPosition.index);
  ASSERT_STREQ("word" BACKSLASH, word.c_str());
}

TEST(TextWordGet, Escaping) {
  AutoText input("w" BACKSLASH QUOTE "o" BACKSLASH NEWLINE "r" BACKSLASH ";d");
  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS,
            AssemblyContext(input, nullptr).getWord(word, &endPosition));
  ASSERT_EQ(10u, endPosition.column);
  ASSERT_EQ(0u, endPosition.line);
  ASSERT_EQ(10u, endPosition.index);
  ASSERT_EQ(input.str, word);
}

TEST(TextWordGet, EscapingEscape) {
  AutoText input("word" BACKSLASH BACKSLASH " abc");
  std::string word;
  spv_position_t endPosition = {};
  ASSERT_EQ(SPV_SUCCESS,
            AssemblyContext(input, nullptr).getWord(word, &endPosition));
  ASSERT_EQ(6u, endPosition.column);
  ASSERT_EQ(0u, endPosition.line);
  ASSERT_EQ(6u, endPosition.index);
  ASSERT_STREQ("word" BACKSLASH BACKSLASH, word.c_str());
}

}  // anonymous namespace
