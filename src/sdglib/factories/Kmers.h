//
// Created by Ben J. Ward (EI) on 27/05/2020.
//

#pragma once

#include <array>
#include <limits>
#include <cassert>
#include <iostream>
#include <cstring>

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

template<typename U> class Kmer {
public:
    size_t pos;
    U fkmer;
    U rkmer;
    U ckmer;

    bool is_fwd() const {
        return ckmer == fkmer;
    }

    bool is_bwd() const {
        return !is_fwd();
    }

    void clear() {
        pos = fkmer = rkmer = ckmer = 0;
    }
};

template <typename U> class Kmers {
protected:
    // C++11 (explicit aliases for input iterator).
    using iterator_category = std::input_iterator_tag;
    using value_type = Kmer<U>;
    using reference = value_type const&;
    using pointer = value_type const*;
    using difference_type = ptrdiff_t;

public:
    // Unwrap std::string and pass to the cstring constructor.
    Kmers(const std::string& seq, const int K) : Kmers(seq.data(), K) {} // Requires C++11.

    Kmers(const char * seq, const int K) : sequence(seq), K(K) {
        assert(K > 0);
        assert(!std::numeric_limits<U>::is_signed);
        assert(K <= ((std::numeric_limits<U>::digits / 2)));
        translation_table = make_nuc2bin_tbl<U>();
    }

    class iterator: public std::iterator<iterator_category, value_type, difference_type, pointer, reference> {
    public:
        explicit iterator(Kmers& p, const size_t offset = 0) : parent(p) {
            position = p.sequence + offset;
            valid_nucleotides = 0;
            kmer.clear();
            //kmer.pos = position - p.sequence; // Currently uses 0 based bp co-ords.
            // TODO: Perhaps move into parent so it is not computed for every iterator or everytime end() is called?
            mask = (U(1) << (2 * parent.K)) - 1;
            incorporate_char(*position);
        }

        bool has_reached_end() { return *position == '\0'; }

        reference operator*() const {
            return kmer;
        }

        pointer operator->() const {
            return std::addressof(operator*());
        }

        // Advance the iterator to the next valid kmer in the sequence, or to the end of the string
        // (\0), whichever comes first.
        iterator& operator++() {
            scan_right();
            return *this;
        }

        bool operator<(Kmers::iterator other) {
            // true if both iterators refer to same sequence, and this iterator is at a lower position.
            return parent.sequence == other.parent.sequence && position < other.position;
        }
        bool operator<=(Kmers::iterator other) {
            // true if both iterators refer to same sequence, and this iterator is at a lower position.
            return parent.sequence == other.parent.sequence && position <= other.position;
        }
        bool operator!=(Kmers::iterator other) {
            // true if both iterators refer to the same sequence, and this iterator is not at the same positon.
            return parent.sequence == other.parent.sequence && position != other.position;
        }
        bool operator==(Kmers::iterator other) {
            // true if both iterators refer to the same sequence, and this iterator is not at the same positon.
            return parent.sequence == other.parent.sequence && position == other.position;
        }
    protected:
        const Kmers& parent;
        const char* position;
        value_type kmer;
        U mask;
        size_t valid_nucleotides;

        void scan_right() {
            while(*position != '\0') {
                ++position;
                const char nucleotide = *position;
                // Get bits for the nucleotide.
                // The array should be a static and constant array.
                incorporate_char(nucleotide);
                if(valid_nucleotides >= parent.K){
                    kmer.pos = ((position - parent.sequence) - parent.K) + 1;
                    break;
                }
            }
        }

        void incorporate_char(const char c) {
            U fbits = parent.translation_table[c];
            U rbits = ~fbits & (U) 3;
            valid_nucleotides = fbits == 4 ? 0 : valid_nucleotides + 1;
            kmer.fkmer = (kmer.fkmer << 2 | fbits) & mask;
            kmer.rkmer = ((kmer.rkmer >> 2) | (rbits << ((parent.K - 1) * 2))) & mask;
            kmer.ckmer = std::min(kmer.fkmer, kmer.rkmer);
        }
    };
    // An iterator at the first valid Kmer in the sequence.
    iterator begin() {
        iterator it(*this);
        ++it;
        return it;
    }
    // An iterator past all valid Kmers in the sequence (has hit \0 in input string).
    iterator end() {
        iterator it(*this, strlen(sequence));
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
