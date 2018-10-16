//
// Created by Luis Yanes (EI) on 09/10/2018.
//

#include <catch.hpp>
#include <sglib/indexers/UniqueKmerIndex.hpp>
#include <sglib/indexers/NKmerIndex.hpp>

TEST_CASE("UniqueKmerIndex create and lookup") {
    unsigned int K(15);
    std::vector<KmerIDX> readkmers;
    StreamKmerIDXFactory skf(K);
    std::string seqMissing = "AAAAAAAAAAAAAAA";
    std::string seqPresent = "CTTGCGGGTTTCCAG";
    SequenceGraph sg;
    sg.load_from_gfa("../tests/datasets/tgraph.gfa");
    UniqueKmerIndex ukm(sg, K);
    ukm.generate_index(sg, true);

    REQUIRE(ukm.getMap().size() != 0);

    skf.produce_all_kmers(seqMissing.data(),readkmers);
    REQUIRE(ukm.find(readkmers[0].kmer) == ukm.end()); // FAILS TO FIND NON PRESENT KMERS

    readkmers.clear();
    skf.produce_all_kmers(seqPresent.data(),readkmers);
    REQUIRE(ukm.find(readkmers[0].kmer) != ukm.end()); // FINDS PRESENT KMERS
}

TEST_CASE("UniqueKmerIndex63 create and lookup") {
    std::vector<KmerIDX128> readkmers;
    StreamKmerIDXFactory128 skf(63);
    std::string seqMissing = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    std::string seqPresent = "CTTGCGGGTTTCCAGGAACTGGCTGTCCTCGGCGTTCAGCGCCATCGACTTCCAGTCCAGCCC";
    SequenceGraph sg;
    sg.load_from_gfa("../tests/datasets/tgraph.gfa");
    Unique63merIndex ukm(sg);
    ukm.generate_index(sg, true);

    REQUIRE(ukm.getMap().size() != 0);

    skf.produce_all_kmers(seqMissing.data(),readkmers);
    REQUIRE(ukm.find(readkmers[0].kmer) == ukm.end()); // FAILS TO FIND NON PRESENT KMERS

    readkmers.clear();
    skf.produce_all_kmers(seqPresent.data(),readkmers);
    REQUIRE(ukm.find(readkmers[0].kmer) != ukm.end()); // FINDS PRESENT KMERS
}

TEST_CASE("NKmerIndex create and lookup") {
    const uint8_t k = 15;
    NKmerIndex assembly_kmers(k);

    std::vector<uint64_t> readkmers;
    StreamKmerFactory skf(k);
    std::string seqMissing = "AAAAAAAAAAAAAAA";
    std::string seqPresent = "CTTGCGGGTTTCCAG";
    SequenceGraph sg;
    sg.load_from_gfa("../tests/datasets/tgraph.gfa");
    assembly_kmers.generate_index(sg);

    REQUIRE(!assembly_kmers.empty());

    skf.produce_all_kmers(seqMissing.data(),readkmers);
    REQUIRE(assembly_kmers.find(readkmers[0]) == assembly_kmers.end()); // FAILS TO FIND NON PRESENT KMERS

    readkmers.clear();
    skf.produce_all_kmers(seqPresent.data(),readkmers);
    REQUIRE(assembly_kmers.find(readkmers[0]) != assembly_kmers.end()); // FINDS PRESENT KMERS
}