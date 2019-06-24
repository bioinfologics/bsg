#include <iostream>
#include <fstream>
#include <sdglib/datastores/LinkedReadsDatastore.hpp>
#include <sdglib/datastores/PairedReadsDatastore.hpp>
#include <sdglib/datastores/LongReadsDatastore.hpp>
#include <sdglib/workspace/WorkSpace.hpp>
#include "cxxopts.hpp"

uint32_t detect_read_size(const std::string &read_filename) {
    std::vector<uint32_t> read_sizes(100);

    FastqRecord rec;
    FastqReader<FastqRecord> fastqReader({}, read_filename);
    uint32_t i = 0;
    while(fastqReader.next_record(rec) and i < read_sizes.size()) {
        read_sizes[i++] = rec.seq.size();
    }
    for (uint32_t i = 100; i < 10000 and fastqReader.next_record(rec); i++) {
        uint32_t j = std::rand()%i;
        if (j < 100) {
            read_sizes[j] = rec.seq.size();
        }
    }
    return (*std::max_element(read_sizes.begin(), read_sizes.end()));
}

int main(int argc, char * argv[]) {
    std::cout << "sdg-datastore"<<std::endl<<std::endl;
    std::cout << "Git origin: " << GIT_ORIGIN_URL << " -> "  << GIT_BRANCH << std::endl;
    std::cout << "Git commit: " << GIT_COMMIT_HASH << std::endl<<std::endl;
    std::cout << "Executed command:"<<std::endl;
    for (auto i=0;i<argc;i++) std::cout<<argv[i]<<" ";
    std::cout<<std::endl<<std::endl;

    if (argc <2){
        std::cout<<"Please specify one of: make, update, view, compare"<<std::endl;
        exit(1);
    }

    if (0==strcmp(argv[1],"make")) {
        std::string read1, read2, long_reads, read_type, output;
        uint16_t min_readsize=0,max_readsize=150;
        try {
            cxxopts::Options options("sdg-datastore make", "BSG make datastore");

            options.add_options()
                    ("help", "Print help")
                    ("1,read1", "input reads, left", cxxopts::value<std::string>(read1))
                    ("2,read2", "input reads, right", cxxopts::value<std::string>(read2))
                    ("L,long_reads", "input reads, long", cxxopts::value<std::string>(long_reads))
                    ("t,read_type", "One of: paired,10x,long", cxxopts::value<std::string>(read_type))
                    ("l,min_read_size", "min size for each read, discards both if one is smaller (default 0)", cxxopts::value<uint16_t>(min_readsize))
                    ("s,max_read_size", "max size for short reads, truncates if longer (default 150)", cxxopts::value<uint16_t>(max_readsize))
                    ("o,output", "output file", cxxopts::value<std::string>(output));
            auto newargc=argc-1;
            auto newargv=&argv[1];
            auto result=options.parse(newargc,newargv);
            if (result.count("help")) {
                std::cout << options.help({""}) << std::endl;
                exit(0);
            }

            if (read_type.empty()) {
                throw cxxopts::OptionException(" please specify an input type");
            }
            if (long_reads.empty() and (read1.empty() or read2.empty() )) {
                throw cxxopts::OptionException(" please specify the paired read files");
            }
            if (read_type == "long" and long_reads.empty()) {
                throw cxxopts::OptionException(" please specify the long read files");
            }
            if (output.empty()) {
                throw cxxopts::OptionException(" please specify an output prefix");
            }


        } catch (const cxxopts::OptionException &e) {
            std::cout << "Error parsing options: " << e.what() << std::endl << std::endl
                      << "Use option --help to check command line arguments." << std::endl;
            exit(1);
        }

        //===== DATASTORE CREATION =====
        if (read_type == "10x" or read_type == "10xseq") {
            // TODO: Detect read size
            auto read_size = detect_read_size(read1);
            sdglib::OutputLog() << "Detected max read size " << read_size << std::endl;
            LinkedReadsDatastore::build_from_fastq(output + ".lrseq", read1, read2,
                                                   (read_type == "10xseq" ? LinkedReadsFormat::seq
                                                                          : LinkedReadsFormat::UCDavis), read_size,
                                                   0);
            //ds.dump_index_to_disk(output+".lrIdx");
        }
        else if (read_type == "paired") {
            // TODO: Detect read size
            auto read_size = detect_read_size(read1);
            sdglib::OutputLog() << "Detected max read size " << read_size << std::endl;
            PairedReadsDatastore::build_from_fastq(output + ".prseq", read1, read2, min_readsize, read_size, 0);
            //ds.dump_index_to_disk(output+".lrIdx");
        }
        else if (read_type == "long") {
            LongReadsDatastore::build_from_fastq(output+".loseq",long_reads);
        }
        else {
            std::cout << "read_type '" << read_type << "' is not supported (yet?)" << std::endl;
        }

    }
    else if (0==strcmp(argv[1],"view")) {
        std::vector<std::string> filenames;
        try {

            cxxopts::Options options("sdg-datastore view", "BSG view datastore");

            options.add_options()
                    ("help", "Print help")
                    ("d,datastore", "datastore name (multi)", cxxopts::value<std::vector<std::string>>(filenames));

            auto newargc=argc-1;
            auto newargv=&argv[1];
            auto result=options.parse(newargc,newargv);
            if (result.count("help")) {
                std::cout << options.help({""}) << std::endl;
                exit(0);
            }

            if (result.count("datastore")==0) {
                throw cxxopts::OptionException(" please specify datastore file (s)");
            }


        } catch (const cxxopts::OptionException &e) {
            std::cout << "Error parsing options: " << e.what() << std::endl << std::endl
                      << "Use option --help to check command line arguments." << std::endl;
            exit(1);
        }

    }
    else if (0==strcmp(argv[1],"compare")) {
        std::vector<std::string> filenames;
        try {

            cxxopts::Options options("sdg-datastore compare", "BSG compare linked reads datastores");

            options.add_options()
                    ("help", "Print help")
                    ("d,datastore", "datastore name (multi)", cxxopts::value<std::vector<std::string>>(filenames));

            auto newargc=argc-1;
            auto newargv=&argv[1];
            auto result=options.parse(newargc,newargv);
            if (result.count("help")) {
                std::cout << options.help({""}) << std::endl;
                exit(0);
            }

            if (result.count("datastore")==0) {
                throw cxxopts::OptionException(" please specify datastore file (s)");
            }


        } catch (const cxxopts::OptionException &e) {
            std::cout << "Error parsing options: " << e.what() << std::endl << std::endl
                      << "Use option --help to check command line arguments." << std::endl;
            exit(1);
        }

        WorkSpace ws;
        std::unordered_map<bsg10xTag, std::vector<uint64_t>> tag_occupancy;
        std::vector<LinkedReadsDatastore> datastores;
        for (auto i=0;i<filenames.size();++i){
            datastores.emplace_back(ws, filenames[i]);
            for (auto rc:datastores.back().get_tag_readcount()){
                if (rc.second>5) {
                    if (tag_occupancy.count(rc.first)==0) tag_occupancy[rc.first].resize(filenames.size());
                    tag_occupancy[rc.first][i]=rc.second;
                }
            }
        }
        std::ofstream tof("tag_occupancies.csv");
        for (const auto &tc : tag_occupancy){
            tof<<tc.first;
            for (auto c:tc.second) {
                tof<<","<<c;
            }
            tof<<std::endl;
        }


    }
    else if(0==strcmp(argv[1],"update")){
        std::cout<<"Datastore updates not implemented yet."<<std::endl;
    }
    else {
        std::cout<<"Please specify one of: make, update, view"<<std::endl;
    }
}

