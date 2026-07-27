#pragma once

#include <stddef.h>

namespace ege
{

template <typename T> class Array
{
public:
    typedef T* iterator;

    class reverse_iterator
    {
    public:
        reverse_iterator(iterator it) { _it = it; }

        reverse_iterator(const reverse_iterator& rit) { _it = rit._it; }

        reverse_iterator& operator++()
        {
            --_it;
            return *this;
        }

        reverse_iterator& operator--()
        {
            ++_it;
            return *this;
        }

        T& operator*() { return *_it; }

        bool operator==(const reverse_iterator& rit) { return _it == rit._it; }

        bool operator!=(const reverse_iterator& rit) { return _it != rit._it; }

    private:
        iterator _it;
    };

public:
    Array()
    {
        m_capacity = 0;
        m_size     = 0;
        m_arr      = NULL;
    }

    Array(const Array& arr)
    {
        m_capacity = arr.m_capacity;
        m_size     = arr.m_size;
        m_arr      = m_capacity > 0 ? new T[m_capacity] : NULL;

        for (size_t i = 0; i < m_size; ++i) {
            m_arr[i] = arr.m_arr[i];
        }
    }

    Array& operator=(const Array& arr)
    {
        if (this != &arr) {
            T* replacement = arr.m_capacity > 0 ? new T[arr.m_capacity] : NULL;
            for (size_t i = 0; i < arr.m_size; ++i) {
                replacement[i] = arr.m_arr[i];
            }
            delete[] m_arr;
            m_arr      = replacement;
            m_capacity = arr.m_capacity;
            m_size     = arr.m_size;
        }
        return *this;
    }

    ~Array()
    {
        if (m_arr) {
            delete[] m_arr;
            m_arr = NULL;
        }
    }

    void resize(size_t sz, T c = T())
    {
        if (m_arr == NULL) {
            m_arr = sz > 0 ? new T[sz] : NULL;
            for (size_t i = 0; i < sz; ++i) {
                m_arr[i] = c;
            }
            m_capacity = sz;
            m_size     = 0;
        } else {
            T*     arr = sz > 0 ? new T[sz] : NULL;
            size_t i   = 0;
            const size_t retained = m_size < sz ? m_size : sz;
            for (; i < retained; ++i) {
                arr[i] = m_arr[i];
            }
            for (; i < sz; ++i) {
                arr[i] = c;
            }
            m_capacity = sz;
            delete[] m_arr;
            m_arr = arr;
            m_size = retained;
        }
    }

    iterator begin() { return m_arr; }

    iterator end() { return m_arr + m_size; }

    reverse_iterator rbegin() { return reverse_iterator(m_arr + m_size - 1); }

    reverse_iterator rend() { return reverse_iterator(m_arr - 1); }

    size_t size() const { return m_size; }

    T& front() { return m_arr[0]; }

    T& back() { return m_arr[m_size - 1]; }

    Array& push_back(const T& obj)
    {
        if (m_arr == NULL) {
            resize(8);
        } else if (m_size == m_capacity) {
            resize(m_capacity * 2);
        }
        m_arr[m_size++] = obj;
        return *this;
    }

    void pop_back()
    {
        if (m_size > 0) {
            --m_size;
        }
    }

    iterator erase(iterator position)
    {
        if (position == end()) {
            return position;
        }
        iterator it = position, it2 = position;
        for (; ++it2 != end(); ++it) {
            *it = *it2;
        }
        --m_size;
        return position;
    }

    iterator insert(iterator position, const T& val)
    {
        size_t pos = position - m_arr;
        if (m_arr == NULL) {
            resize(8);
        } else if (m_size == m_capacity) {
            resize(m_capacity * 2);
        }
        iterator it = end(), it2 = end();
        position = m_arr + pos;
        for (; it2 != position; --it2) {
            *it2 = *--it;
        }
        *it2 = val;
        ++m_size;
        return position;
    }

protected:
    size_t m_capacity;
    size_t m_size;
    T*     m_arr;
};

}
