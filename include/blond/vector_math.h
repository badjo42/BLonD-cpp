/*
 * vector_math.h
 *
 *  Created on: Nov 2, 2016
 *      Author: kiliakis
 */

#pragma once

#include <algorithm>
#include <functional>

//
// Useful vector operations
//

// apply a unary function func at every element of vector a
template <typename T, typename F>
std::vector<T> apply_f(const std::vector<T> &a, F func)
{
    std::vector<T> result;
    result.reserve(a.size());
    std::transform(a.begin(), a.end(), std::back_inserter(result), func);
    return result;
}

//
// Vector - Vector basic operations
//

template <typename T>
std::vector<T> operator+(const std::vector<T> &a, const std::vector<T> &b)
{
    assert(a.size() == b.size());

    std::vector<T> result;
    result.reserve(a.size());

    std::transform(a.begin(), a.end(), b.begin(),
                   std::back_inserter(result), std::plus<T>());
    return result;

}

template <typename T>
std::vector<T> operator+=(std::vector<T> &a, const std::vector<T> &b)
{
    assert(a.size() == b.size());

    std::transform(a.begin(), a.end(), b.begin(),
                   a.begin(), std::plus<T>());
    return a;
}


template <typename T>
std::vector<T> operator-(const std::vector<T> &a, const std::vector<T> &b)
{
    assert(a.size() == b.size());

    std::vector<T> result;
    result.reserve(a.size());

    std::transform(a.begin(), a.end(), b.begin(),
                   std::back_inserter(result), std::minus<T>());
    return result;

}

template <typename T>
std::vector<T> operator-=(std::vector<T> &a, const std::vector<T> &b)
{
    assert(a.size() == b.size());

    std::transform(a.begin(), a.end(), b.begin(),
                   a.begin(), std::minus<T>());
    return a;
}

template <typename T>
std::vector<T> operator*(const std::vector<T> &a, const std::vector<T> &b)
{
    assert(a.size() == b.size());

    std::vector<T> result;
    result.reserve(a.size());

    std::transform(a.begin(), a.end(), b.begin(),
                   std::back_inserter(result), std::multiplies<T>());
    return result;

}

template <typename T>
std::vector<T> operator*=(std::vector<T> &a, const std::vector<T> &b)
{
    assert(a.size() == b.size());

    std::transform(a.begin(), a.end(), b.begin(),
                   a.begin(), std::multiplies<T>());
    return a;
}


template <typename T>
std::vector<T> operator/(const std::vector<T> &a, const std::vector<T> &b)
{
    assert(a.size() == b.size());

    std::vector<T> result;
    result.reserve(a.size());

    std::transform(a.begin(), a.end(), b.begin(),
                   std::back_inserter(result), std::divides<T>());
    return result;

}

template <typename T>
std::vector<T> operator/=(std::vector<T> &a, const std::vector<T> &b)
{
    assert(a.size() == b.size());

    std::transform(a.begin(), a.end(), b.begin(),
                   a.begin(), std::divides<T>());
    return a;
}


//
// Vector - scalar basic operations
//


template <typename T, typename U>
std::vector<T> operator+(const std::vector<T> &a, const U &b)
{

    std::vector<T> result;
    result.reserve(a.size());

    std::transform(a.begin(), a.end(), std::back_inserter(result),
                   std::bind2nd(std::plus<T>(), b));
    return result;

}

template <typename T, typename U>
std::vector<T> operator+(const U &b, const std::vector<T> &a)
{
    return a + b;
}

template <typename T, typename U>
std::vector<T> operator+=(std::vector<T> &a, const U &b)
{
    std::transform(a.begin(), a.end(), a.begin(),
                   std::bind2nd(std::plus<T>(), b));
    return a;
}


template <typename T, typename U>
std::vector<T> operator-(const std::vector<T> &a, const U &b)
{
    std::vector<T> result;
    result.reserve(a.size());

    std::transform(a.begin(), a.end(), std::back_inserter(result),
                   std::bind2nd(std::minus<T>(), b));
    return result;

}

template <typename T, typename U>
std::vector<T> operator-(const U &b, const std::vector<T> &a)
{
    std::vector<T> result;
    result.reserve(a.size());

    std::transform(a.begin(), a.end(), std::back_inserter(result),
                   std::bind1st(std::minus<T>(), b));
    return result;

}

template <typename T, typename U>
std::vector<T> operator-=(std::vector<T> &a, const U &b)
{
    std::transform(a.begin(), a.end(), a.begin(),
                   std::bind2nd(std::minus<T>(), b));
    return a;
}

template <typename T, typename U>
std::vector<T> operator*(const std::vector<T> &a, const U &b)
{
    std::vector<T> result;
    result.reserve(a.size());

    std::transform(a.begin(), a.end(), std::back_inserter(result),
                   std::bind2nd(std::multiplies<T>(), b));
    return result;

}

template <typename T, typename U>
std::vector<T> operator*(const U &b, const std::vector<T> &a)
{
    return a * b;
}

template <typename T, typename U>
std::vector<T> operator*=(std::vector<T> &a, const U &b)
{
    std::transform(a.begin(), a.end(), a.begin(),
                   std::bind2nd(std::multiplies<T>(), b));
    return a;
}


template <typename T, typename U>
std::vector<T> operator/(const std::vector<T> &a, const U &b)
{
    std::vector<T> result;
    result.reserve(a.size());

    std::transform(a.begin(), a.end(), std::back_inserter(result),
                   std::bind2nd(std::divides<T>(), b));
    return result;

}

template <typename T, typename U>
std::vector<T> operator/(const U &b, const std::vector<T> &a)
{
    std::vector<T> result;
    result.reserve(a.size());

    std::transform(a.begin(), a.end(), std::back_inserter(result),
                   std::bind1st(std::divides<T>(), b));
    return result;

}

template <typename T, typename U>
std::vector<T> operator/=(std::vector<T> &a, const U &b)
{
    std::transform(a.begin(), a.end(), a.begin(),
                   std::bind2nd(std::divides<T>(), b));
    return a;
}

