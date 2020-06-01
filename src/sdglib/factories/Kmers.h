//
// Created by Ben J. Ward (EI) on 27/05/2020.
//

#ifndef SDG_KMERS_H
#define SDG_KMERS_H

#pragma once

#include <array>
#include <limits>
#include <assert.h>

// I want this function to be able to run at compile time to create the constant static
// arrays that the Kmers class uses to convert chars to binary.
template<typename U>
constexpr std::array<U, 256> make_nuc2bin_tbl(){
    std::array<U, 256> tbl = {4};
    tbl['a'] = tbl['A'] = 0;
    tbl['c'] = tbl['C'] = 1;
    tbl['g'] = tbl['G'] = 2;
    tbl['t'] = tbl['T'] = 3;
    return tbl;
}

// Type traits that determine how Kmers iteration produces values.
class JustKmers;
class KmersAndPos;

template<typename F> struct is_formatter { static const bool value = false; };
template<> struct is_formatter<JustKmers> { static const bool value = true; };
template<> struct is_formatter<KmersAndPos> { static const bool value = true; };

// A type representing a "lazy" - if you want to call it that, container of all the k-mers in a sequence.
// Has an input iterator in its scope for iterating from the first k-mer in a sequence to the last.
template <typename U, typename F> class Kmers {
    // C++11 (explicit aliases for input iterator).
    using iterator_category = std::input_iterator_tag;
    using value_type = U;
    using reference = value_type const&;
    using pointer = value_type const*;
    using difference_type = ptrdiff_t;

    // Static array for the conversion from letter to character.
    // static const std::array<value_type, 256> nuc_to_bin = make_nuc2bin_tbl<value_type>();
public:
    // Unwrap std::string and pass to the cstring constructor.
    Kmers(const std::string& seq, const int K) : Kmers(seq.data(), K) {} // Requires C++11.

    Kmers(const char * seq, const int K) : sequence(seq), K(K) {
        // Here I check someone hasn't requested silly K size, before
        // making the local translation table for this container.
        // static asserts below?
        assert(is_formatter<F>::value);
        assert(K > 0);
        assert(!std::numeric_limits<value_type>::is_signed);
        // Forgive heavy bracket use - I need to get used to if C++ has the operator precedence I'm used to or not.
        assert(K <= ((std::numeric_limits<value_type>::digits / 2))); // -1 here to keep space for the sign bits.
        translation_table = make_nuc2bin_tbl<value_type>();
    }

    class iterator: public std::iterator<iterator_category, value_type, difference_type, pointer, reference> {
    public:
        explicit iterator(const Kmers& parent) // Do I need this is nested classes can access parent class vars?
            : parent(parent), position(parent.sequence), last_unknown(0), fwmer(0), rvmer(0) {
            ++this;
        }
        reference operator*() const {
            // I want to specialize this on template parameter F but am unsure on how to do it properly.
            return std::min(fwmer, rvmer);
        }
        pointer operator->() const {
            // I want to specialize this on template parameter F but am unsure on how to do it properly.
            return std::min(fwmer, rvmer);
        }
        // Advance the iterator to the next valid kmer in the sequence, or to the end of the string
        // (\0), whichever comes first.
        iterator& operator++() {
            // Here I disallow the pointer to be advanced past the end of the string (while loop condition).
            // I don't know if such safety is C++ style or not, or if given iterators relationship to
            // pointers, the C++ philosophy is to wash their hands and say "you're on your own".
            while(*position != '\0' && last_unknown < parent.K) {
                // Then add the base pointed to by nextbase to rvmer and fwmer.
                // Then increment nextbase
                const char nucleotide = *position;
                // Get bits for the nucleotide.
                // The array should be a static and constant array.
                value_type fbits = parent.translation_table[nucleotide];
                value_type rbits = ~fbits & (value_type) 3;
                last_unknown = fbits == 4 ? 0 : last_unknown + 1;
                fwmer = (fwmer << 2 | fbits);
                rvmer = (rvmer >> 2) | (rbits << ((parent.K - 1) * 2));
                ++position;
            }
            return *this;
        }
        bool reached_end() const {
            return *position == '\0';
        }
        bool operator<(Kmers::iterator other) {
            // true if both iterators refer to same sequence, and this iterator is at a lower position.
            return parent.sequence == other.parent.sequence && position < other.position;
        }
    private:
        const Kmers& parent;
        const char* position;
        value_type fwmer;
        value_type rvmer;
        size_t last_unknown;
    };
    // An iterator at the first valid Kmer in the sequence.
    iterator begin() { return iterator(this); }
    // An iterator past all valid Kmers in the sequence (has hit \0 in input string).
    iterator end() {
        iterator it = begin();
        while(!it.reached_end()) ++it;
        return it;
    }
private:
    // std::string& sequence;
    // Doesn't this make it less safe than a std::string reference though?
    // Someone could just free the memory out from underneath us.
    const char* sequence;
    const int K;
    std::array<U, 256> translation_table;
};

#endif //SDG_KMERS_H
