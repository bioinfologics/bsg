//
// Created by Luis Yanes (EI) on 15/09/2017.
//

#ifndef SEQSORTER_KMERIDX_H
#define SEQSORTER_KMERIDX_H

#include <vector>
#include <limits>
#include <tuple>
#include <cmath>
#include <sglib/factories/KMerFactory.hpp>
#include <sglib/types/KmerTypes.hpp>

struct KMerIDXFactoryParams {
    uint8_t k;
};

template<typename FileRecord>
class kmerIDXFactory : protected KMerFactory {
public:
    explicit kmerIDXFactory(KMerIDXFactoryParams params) : KMerFactory(params.k) {}

    ~kmerIDXFactory() {}

    void setFileRecord(FileRecord &rec) {
        currentRecord = rec;
        fkmer=0;
        rkmer=0;
        last_unknown=0;
    }

    // TODO: Adjust for when K is larger than what fits in uint64_t!
    const bool next_element(std::vector<KmerIDX> &mers) {
        uint64_t p(0);
        while (p < currentRecord.seq.size()) {
            //fkmer: grows from the right (LSB)
            //rkmer: grows from the left (MSB)
            bases++;
            fillKBuf(currentRecord.seq[p], fkmer, rkmer, last_unknown);
            p++;
            if (last_unknown >= K) {
                if (fkmer <= rkmer) {
                    // Is fwd
                    mers.emplace_back(fkmer, currentRecord.id, p, 1);
                } else {
                    // Is bwd
                    mers.emplace_back(rkmer, -1 * currentRecord.id, p, 1);
                }
            }
        }
        return false;
    }

private:
    FileRecord currentRecord;
    uint64_t bases;
};

class StreamKmerIDXFactory : public  KMerFactory {
public:
    explicit StreamKmerIDXFactory(uint8_t k) : KMerFactory(k){}
    inline void produce_all_kmers(const char * seq, std::vector<KmerIDX> &mers){
        // TODO: Adjust for when K is larger than what fits in uint64_t!
        last_unknown=0;
        fkmer=0;
        rkmer=0;
        auto s=seq;
        while (*s!='\0' and *s!='\n') {
            //fkmer: grows from the right (LSB)
            //rkmer: grows from the left (MSB)
            fillKBuf(*s, fkmer, rkmer, last_unknown);
            if (last_unknown >= K) {
                if (fkmer <= rkmer) {
                    // Is fwd
                    mers.emplace_back(fkmer);
                    mers.back().contigID=1;
                } else {
                    // Is bwd
                    mers.emplace_back(rkmer);
                    mers.back().contigID=-1;
                }
            }
            ++s;
        }
    }
};

class StreamKmerIDXFactory128 : public  KMerFactory128 {
public:
    explicit StreamKmerIDXFactory128(uint8_t k) : KMerFactory128(k){}
    inline void produce_all_kmers(const char * seq, std::vector<KmerIDX128> &mers){
        // TODO: Adjust for when K is larger than what fits in uint64_t!
        last_unknown=0;
        fkmer=0;
        rkmer=0;
        auto s=seq;
        while (*s!='\0' and *s!='\n') {
            //fkmer: grows from the right (LSB)
            //rkmer: grows from the left (MSB)
            fillKBuf(*s, fkmer, rkmer, last_unknown);
            if (last_unknown >= K) {
                if (fkmer <= rkmer) {
                    // Is fwd
                    mers.emplace_back(fkmer);
                    mers.back().contigID=1;
                } else {
                    // Is bwd
                    mers.emplace_back(rkmer);
                    mers.back().contigID=-1;
                }
            }
            ++s;
        }
    }
};

#endif //SEQSORTER_KMERIDX_H