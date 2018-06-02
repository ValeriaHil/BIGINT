#include "big_integer.h"

#include <cstring>
#include <stdexcept>
#include <iostream>
#include <utility>
#include <algorithm>

const uint32_t MX_DIG = UINT32_MAX;
const uint64_t BASE = static_cast<uint64_t> (MX_DIG) + 1;
const size_t BASE_DEG = 32;

big_integer::big_integer() : sign(true), data(1, 0) {
}

big_integer::big_integer(big_integer const &other) : sign(other.sign), data(other.data) {
}

big_integer::big_integer(int a) {
    if (a >= 0) {
        sign = true;
        data.push_back(static_cast<uint32_t>(a));
    } else {
        sign = false;
        data.push_back(static_cast<uint32_t>(static_cast<long long>(a) * -1));
    }
}

big_integer::big_integer(unsigned int a) : sign(true), data(1, a) {

}

big_integer::big_integer(std::vector<uint32_t> const &v, bool s) : data(v) {
    if (v.size() == 1 && v[0] == 0) {
        s = true;
    }
    sign = s;
    remove_unnecessary_zeros();
}

big_integer::big_integer(std::string const &str) {
    if (str.length() == 0) {
        *this = big_integer(0);
    } else {
        size_t start = 0;
        if (str[0] == '-' || str[0] == '+') {
            start = 1;
        }

        big_integer res;
        for (size_t i = start; i < str.size(); i++) {
            res *= 10;
            res += str[i] - '0';
        }

        if (str[0] == '-') {
            res = -res;
        }
        swap(res);
    }
}

big_integer::~big_integer() = default;

big_integer &big_integer::operator=(big_integer const &other) {
    big_integer copy(other);
    swap(copy);
    return *this;
}

big_integer &big_integer::operator+=(big_integer const &rhs) {
    if (this->sign && !rhs.sign) {
        return *this -= (-rhs);
    }
    if (!this->sign && rhs.sign) {
        return *this = rhs - (-*this);
    }

    size_t n = this->data.size();
    size_t m = rhs.data.size();
    bool carry = false;
    for (size_t i = 0; i < std::max(n, m) || carry; i++) {
        if (i == this->data.size()) {
            this->data.push_back(0);
        }
        uint64_t res = static_cast<uint64_t> (this->data[i]) + carry + (i < rhs.data.size() ? rhs.data[i] : 0);
        carry = (res > MX_DIG);
        this->data[i] = static_cast<uint32_t>(res & MX_DIG);
    }

    remove_unnecessary_zeros();

    return *this;
}

big_integer &big_integer::operator-=(big_integer const &rhs) {
    if (this->sign && !rhs.sign) {
        return *this += -(rhs);
    }
    if (!this->sign && !rhs.sign) {
        return *this = -(rhs) - (-(*this));
    }

    if (rhs > *this) {
        return *this = -(rhs - (*this));
    }

    bool carry = false;
    for (size_t i = 0; i < rhs.data.size() || carry; i++) {
        uint64_t cur = (static_cast<uint64_t>((i < rhs.data.size() ? rhs.data[i] : 0)) + carry);
        carry = (cur > this->data[i]);
        this->data[i] -= (cur & MX_DIG);
    }

    remove_unnecessary_zeros();

    return *this;
}

big_integer &big_integer::operator*=(big_integer const &rhs) {
    std::vector<uint32_t> ans(this->data.size() + rhs.data.size(), 0);

    for (size_t i = 0; i < this->data.size(); i++) {
        uint32_t carry = 0;
        for (size_t j = 0; j < rhs.data.size() || carry; j++) {
            uint64_t cur = (ans[i + j]) + static_cast<uint64_t>(this->data[i]) *
                                          (j < (int) rhs.data.size() ? rhs.data[j] : 0) +
                           carry;
            ans[i + j] = static_cast<uint32_t>(cur & MX_DIG);
            carry = static_cast<uint32_t>(cur >> BASE_DEG);
        }
    }

    *this = big_integer(ans, this->sign == rhs.sign);
    remove_unnecessary_zeros();

    return *this;
}

big_integer &big_integer::operator/=(big_integer const &rhs) {
    return *this = div_and_mod(*this, rhs).first;
}

big_integer &big_integer::operator%=(big_integer const &rhs) {
    return *this = div_and_mod(*this, rhs).second;
}

big_integer &big_integer::operator&=(big_integer const &rhs) {
    return apply_bitwise_operation(rhs, std::bit_and<uint32_t>());
}

big_integer &big_integer::operator|=(big_integer const &rhs) {
    return apply_bitwise_operation(rhs, std::bit_or<uint32_t>());
}

big_integer &big_integer::operator^=(big_integer const &rhs) {
    return apply_bitwise_operation(rhs, std::bit_xor<uint32_t>());

}

big_integer &big_integer::operator<<=(int rhs) {
    if (rhs == 0) {
        return *this;
    }

    size_t shift = rhs / BASE_DEG;
    size_t from_cur = rhs % BASE_DEG;
    size_t from_next = BASE_DEG - from_cur;
    size_t size = this->data.size() + shift + 1;
    std::vector<uint32_t> data(size);


    for (size_t i = shift; i < size; i++) {
        if (i - shift < this->data.size()) {
            data[i] = this->data[i - shift] << from_cur;
        }
        if (i - shift - 1 >= 0 && i - shift - 1 < this->data.size() && i != shift) {
            data[i] |= static_cast<u_int64_t>(this->data[i - shift - 1]) >> from_next;
        }
    }

    *this = big_integer(data, this->sign);
    remove_unnecessary_zeros();
    return *this;
}

big_integer &big_integer::operator>>=(int rhs) {
    if (rhs == 0) {
        return *this;
    }

    size_t shift = rhs / BASE_DEG;
    size_t from_cur = rhs % BASE_DEG;
    size_t from_next = BASE_DEG - from_cur;
    size_t size = std::max(this->data.size() - shift, static_cast<size_t> (0));
    std::vector<uint32_t> data(size);

    for (size_t i = 0; i < size; i++) {
        data[i] = this->data[i + shift] >> from_cur;
        if (i + shift + 1 < this->data.size()) {
            data[i] |= this->data[i + shift + 1] << from_next;
        }
    }
    *this = big_integer(data, this->sign);
    remove_unnecessary_zeros();
    if (!this->sign) {
        this->sign = true;
        *this = ~*this;
    }
    return *this;
}

big_integer big_integer::operator+() const {
    return *this;
}

big_integer big_integer::operator-() const {
    big_integer r = *this;
    if (r.data.size() == 1 && r.data[0] == 0) {
        return r;
    }
    r.sign = !r.sign;
    return r;
}

big_integer big_integer::operator~() const {
    return -(*this) - 1;
}

//big_integer &big_integer::operator++() {
//    return *this;
//}
//
//big_integer big_integer::operator++(int) {
//    big_integer r = *this;
//    *this += 1;
//    return r;
//}
//
//big_integer &big_integer::operator--() {
//    return *this;
//}
//
//big_integer big_integer::operator--(int) {
//    big_integer r = *this;
//    *this -= 1;
//    return r;
//}

big_integer operator+(big_integer a, big_integer const &b) {
    return a += b;
}

big_integer operator-(big_integer a, big_integer const &b) {
    return a -= b;
}

big_integer operator*(big_integer a, big_integer const &b) {
    return a *= b;
}

big_integer operator/(big_integer a, big_integer const &b) {
    return a /= b;
}

big_integer operator%(big_integer a, big_integer const &b) {
    return a %= b;
}

big_integer operator&(big_integer a, big_integer const &b) {
    return a &= b;
}

big_integer operator|(big_integer a, big_integer const &b) {
    return a |= b;
}

big_integer operator^(big_integer a, big_integer const &b) {
    return a ^= b;
}

big_integer operator<<(big_integer a, int b) {
    return a <<= b;
}

big_integer operator>>(big_integer a, int b) {
    return a >>= b;
}

bool operator==(big_integer const &a, big_integer const &b) {
    if (a.sign != b.sign || a.data.size() != b.data.size()) {
        return false;
    }
    for (size_t i = 0; i < a.data.size(); i++) {
        if (a.data[i] != b.data[i]) {
            return false;
        }
    }
    return true;
}

bool operator!=(big_integer const &a, big_integer const &b) {
    return !(a == b);
}

bool operator<(big_integer const &a, big_integer const &b) {
    return !(a >= b);
}

bool operator>(big_integer const &a, big_integer const &b) {
    return !(a <= b);
}

bool operator<=(big_integer const &a, big_integer const &b) {
    return (a == b || a < b);
}

bool operator>=(big_integer const &a, big_integer const &b) {
    if (a == b) {
        return true;
    }
    if (a.sign && !b.sign) {
        return true;
    }
    if (!a.sign && b.sign) {
        return false;
    }
    if (a.sign) {
        if (a.data.size() == b.data.size()) {
            size_t i = a.data.size();
            for (; i != 0 && a.data[i - 1] == b.data[i - 1]; i--) {
            }
            return a.data[i - 1] >= b.data[i - 1];
        } else {
            return a.data.size() > b.data.size();
        }
    } else {
        if (a.data.size() == b.data.size()) {
            for (size_t i = a.data.size(); i != 0; i--) {
                if (a.data[i - 1] > b.data[i - 1]) {
                    return false;
                }
            }
            return true;
        } else {
            return a.data.size() < b.data.size();
        }
    }
}

void big_integer::swap(big_integer &b) {
    std::swap(this->sign, b.sign);
    std::swap(this->data, b.data);
}

void big_integer::remove_unnecessary_zeros() {
    while (this->data.size() > 1 && this->data.back() == 0) {
        this->data.pop_back();
    }
}

std::vector<uint32_t> big_integer::twos_complement(big_integer const &x) {
    if (x.sign) {
        return x.data;
    } else {
        big_integer other = x;
        other.sign = true;
        for (auto &a : other.data) {
            a = ~a;
        }
        return (other + 1).data;
    }
}

std::pair<big_integer, big_integer> big_integer::div_and_mod(const big_integer &a, const big_integer &b) {
    size_t n = a.data.size();
    size_t m = b.data.size();
    bool sign = (a.sign == b.sign);
    if (m == 1) {
        return std::make_pair(big_integer(quotient(a.data, b.data[0]), sign),
                              big_integer(remainder(a.data, b.data[0]), a.sign));
    }

    uint32_t f = static_cast<uint32_t>((BASE) / static_cast<uint64_t>(b.data.back() + 1));
    big_integer r = a * f;
    r.sign = true;
    big_integer d = b * f;
    d.sign = true;

    if (r < d) {
        return {big_integer(0), *this};
    }

    n = r.data.size();
    m = d.data.size();
    std::vector<uint32_t> ans(n - m + 1);

    big_integer q, h;
    uint64_t d1 = d.data.back();

    for (int k = r.data.size() - 1; k > static_cast<int>(r.data.size() - d.data.size()); k--) {
        h <<= BASE_DEG;
        h += big_integer(r.data[k]);
    }
    for (size_t k = r.data.size() - d.data.size() + 1; k--;) {
        h <<= BASE_DEG;
        h += r.data[k];

        uint64_t r2 = h.data.back();
        if (h.data.size() > d.data.size()) {
            r2 *= (BASE);
            r2 += h.data[h.data.size() - 2];
        }
        uint64_t qt = std::min(r2 / d1, (uint64_t) MX_DIG);
        big_integer dq = d * (uint32_t) (qt & MX_DIG);
        while (h < dq) {
            qt--;
            dq -= d;
        }
        ans[k] = static_cast<unsigned int>(qt & MX_DIG);
        h -= dq;
    }
    *this = big_integer(ans, sign);
    remove_unnecessary_zeros();
    h.remove_unnecessary_zeros();
    return {*this, h / f};
}

std::vector<uint32_t> big_integer::quotient(std::vector<uint32_t> a, uint32_t b) {
    size_t n = a.size();
    std::vector<uint32_t> res(n);
    uint64_t carry = 0;

    for (size_t i = n; i > 0; i--) {
        size_t k = i - 1;
        uint64_t tmp = carry * BASE + a[k];
        res[k] = static_cast<uint32_t>(tmp / b);
        carry = tmp % b;
    }
    return res;
}

std::vector<uint32_t> big_integer::remainder(std::vector<uint32_t> a, uint32_t b) {
    size_t n = a.size();
    std::vector<uint32_t> res;
    uint64_t carry = 0;

    for (size_t i = n; i > 0; i--) {
        size_t k = i - 1;
        carry = static_cast<uint32_t>((carry * BASE + a[k]) % b);
    }
    res.push_back(static_cast<uint32_t>(carry));
    return res;
}

template<class FunctorT>
big_integer &big_integer::apply_bitwise_operation(big_integer const &rhs, FunctorT functor) {
    std::vector<uint32_t> a = twos_complement(*this);
    std::vector<uint32_t> b = twos_complement(rhs);

    for (size_t i = 0; i < std::max(a.size(), b.size()); i++) {
        if (i == a.size()) {
            a.push_back(this->sign ? 0 : MX_DIG);
        }
        a[i] = functor(a[i], (i < b.size() ? b[i] : (rhs.sign ? 0 : MX_DIG)));
    }

    bool sign = (a.back() >> (BASE_DEG - 1)) == 0;
    big_integer res = big_integer(a, sign);
    res.data = twos_complement(res);
    return *this = res;
}


std::string to_string(big_integer const &a) {
    std::string result;
    big_integer abs_big = a;
    abs_big.sign = true;

    while (abs_big.data.size() != 1 || (abs_big.data.size() == 1 && abs_big.data[0] != 0)) {
        big_integer t = abs_big % 10;
        abs_big = abs_big / 10;
        result.push_back('0' + t.data[0]);
        abs_big.remove_unnecessary_zeros();
    }

    while (!result.empty() && result.back() == '0') {
        result.pop_back();
    }

    if (result.empty()) {
        result.push_back('0');
    }

    if (!a.sign) {
        result.push_back('-');
    }

    std::reverse(result.begin(), result.end());
    return result;
}

std::ostream &operator<<(std::ostream &s, big_integer const &a) {
    return s << to_string(a);
}
