#include <blond/configuration.h>
#include <blond/constants.h>
#include <blond/globals.h>
#include <blond/math_functions.h>
#include <blond/utilities.h>
#include <blond/vector_math.h>
#include <gtest/gtest.h>

using namespace mymath;

TEST(testAdd, test1)
{
    int_vector_t a{1, 2, 3, 4, 5};
    int_vector_t b{1, 2, 3, 4, 5};
    int_vector_t c;
    c = a + b;
    for (uint i = 0; i < c.size(); ++i) {
        int ref = a[i] + b[i];
        int real = c[i];
        ASSERT_EQ(ref, real);
    }
}


TEST(testAdd, test2)
{
    f_vector_t a = linspace(0.0, 1.0, 42);
    f_vector_t b = linspace(-1.0, 0.0, 42);
    auto c = a + b;
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = a[i] + b[i];
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}


TEST(testAdd, test3)
{
    f_vector_t a = linspace(0.0, 1.0, 42);
    f_vector_t b = linspace(-1.0, 0.0, 42);
    auto c = a + b + 2;
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = 2 + a[i] + b[i];
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}

TEST(testAdd, test4)
{
    f_vector_t a = linspace(0.0, 1.0, 42);
    f_vector_t b = linspace(-1.0, 0.0, 42);
    auto c = -1 + a + b;
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = -1 + a[i] + b[i];
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}

TEST(testSub, test1)
{
    f_vector_t a = linspace(0.0, 1.0, 42);
    auto c = a - a;
    for (uint i = 0; i < c.size(); ++i) {
        auto real = c[i];
        ASSERT_DOUBLE_EQ(0.0, real);
    }
}


TEST(testSub, test2)
{
    f_vector_t a = linspace(0.0, 1.0, 42);
    a -= a;
    for (uint i = 0; i < a.size(); ++i) {
        auto real = a[i];
        ASSERT_DOUBLE_EQ(0.0, real);
    }
}

TEST(testSub, test3)
{
    f_vector_t a = linspace(0.0, 1.0, 42);
    a = a - a + 1.5;
    for (uint i = 0; i < a.size(); ++i) {
        auto real = a[i];
        ASSERT_DOUBLE_EQ(1.5, real);
    }
}

TEST(testSub, test4)
{
    f_vector_t a = linspace(0.0, 1.0, 42);
    auto c = 1.0 - a - 1.0;
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = 1.0 - a[i] - 1.0;
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}


TEST(testMul, test1)
{
    f_vector_t a = linspace(0.0, 15.0, 132);
    auto c = a * a;
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = a[i] * a[i];
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}


TEST(testMul, test2)
{
    f_vector_t a = linspace(0.0, 15.0, 132);
    f_vector_t b = linspace(0.0, 1.0, 132);
    auto c = a * b;
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = a[i] * b[i];
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}

TEST(testMul, test3)
{
    f_vector_t a = linspace(0.0, 15.0, 132);
    f_vector_t b = linspace(0.0, 1.0, 132);
    auto c = a * b * 0;
    for (uint i = 0; i < c.size(); ++i) {
        auto real = c[i];
        ASSERT_DOUBLE_EQ(0.0, real);
    }
}

TEST(testMul, test4)
{
    f_vector_t a = linspace(0.0, 15.0, 132);
    f_vector_t b = linspace(0.0, 1.0, 132);
    auto c = 0 * a * b;
    for (uint i = 0; i < c.size(); ++i) {
        auto real = c[i];
        ASSERT_DOUBLE_EQ(0.0, real);
    }
}


TEST(testDiv, test1)
{
    f_vector_t a = linspace(0.0, 15.0, 132);
    f_vector_t b = linspace(0.1, 1.0, 132);
    auto c = a / b;
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = a[i] / b[i];
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}

TEST(testDiv, test2)
{
    f_vector_t a = linspace(0.1, 15.0, 132);
    a /= a;
    for (uint i = 0; i < a.size(); ++i) {
        auto real = a[i];
        ASSERT_DOUBLE_EQ(1.0, real);
    }
}

TEST(testDiv, test3)
{
    f_vector_t a = linspace(0.1, 15.0, 132);
    auto c = a / 2;
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = a[i] / 2;
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}

TEST(testDiv, test4)
{
    f_vector_t a = linspace(0.1, 15.0, 132);
    auto c = 1.0 / a;
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = 1.0 / a[i];
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}


TEST(testApplyF, test1)
{
    f_vector_t a = linspace(0.1, 15.0, 132);
    auto c = apply_f(a, sqrt);
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = sqrt(a[i]);
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}

TEST(testApplyF, test2)
{
    f_vector_t a = linspace(0.1, 15.0, 132);
    auto c = apply_f(a, sin);
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = sin(a[i]);
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}


TEST(testApplyF, test3)
{
    f_vector_t a = linspace(0.1, 15.0, 132);
    auto square = [](double x) {return x * x;};
    auto c = apply_f(a, square);
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = a[i] * a[i];
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}


TEST(testApplyF, test4)
{
    f_vector_t a = linspace(0.1, 15.0, 132);
    auto cube = [](double x) {return x * x * x;};
    auto c = apply_f(a, cube);
    for (uint i = 0; i < c.size(); ++i) {
        auto ref = a[i] * a[i] * a[i];
        auto real = c[i];
        ASSERT_DOUBLE_EQ(ref, real);
    }
}


int main(int ac, char *av[])
{
    ::testing::InitGoogleTest(&ac, av);
    return RUN_ALL_TESTS();
}
