#include "tlm/error.hpp"
#include "tlm/result.hpp"
#include <gtest/gtest.h>

TEST(ResultTest, ValueResultExposesValueAndNoError) {
    const tlm::Result<int, tlm::Error> result{42};

    const int *value = result.ok();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 42);
    EXPECT_EQ(result.err(), nullptr);
}

TEST(ResultTest, ErrorResultExposesErrorAndNoValue) {
    const tlm::Error error{tlm::ErrorCode::MalformedFrame};
    const tlm::Result<int, tlm::Error> result{error};

    const tlm::Error *result_error = result.err();
    ASSERT_EQ(result.ok(), nullptr);
    ASSERT_NE(result_error, nullptr);
    EXPECT_EQ(*result_error, error);
}

TEST(ResultTest, MatchOnValueResultCallsValueHandlerWithValue) {
    const tlm::Result<int, tlm::Error> result{42};
    bool err_called = false;

    const int seen = result.match([](int value) { return value; },
                                  [&err_called](tlm::Error) {
                                      err_called = true;
                                      return -1;
                                  });

    EXPECT_EQ(seen, 42);
    EXPECT_FALSE(err_called);
}

TEST(ResultTest, MatchOnErrorResultCallsErrorHandlerWithError) {
    const tlm::Error error{tlm::ErrorCode::MalformedFrame};
    const tlm::Result<int, tlm::Error> result{error};
    bool ok_called = false;

    const tlm::Error seen = result.match(
        [&ok_called](int) {
            ok_called = true;
            return tlm::Error{tlm::ErrorCode::BadChecksum};
        },
        [](tlm::Error err) { return err; });

    EXPECT_EQ(seen, error);
    EXPECT_FALSE(ok_called);
}
