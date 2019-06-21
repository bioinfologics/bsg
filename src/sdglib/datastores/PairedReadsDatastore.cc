//
// Created by Bernardo Clavijo (EI) on 10/02/2018.
//

#include "PairedReadsDatastore.hpp"
#include <sdglib/workspace/WorkSpace.hpp>
#include <sdglib/mappers/PairedReadsMapper.hpp>
#include <sdglib/logger/OutputLog.hpp>
#include <sdglib/types/GenericTypes.hpp>
#include <fstream>
#include <strings.h>
#include <cstring>


void PairedReadsDatastore::print_status() {
    sdglib::OutputLog()<<"PairedRead Datastore from "<<filename<<" contains "<<size()-1<<" reads."<<std::endl;
    mapper.print_status();
}

void PairedReadsDatastore::build_from_fastq(std::string read1_filename,std::string read2_filename, std::string output_filename, uint64_t _min_rs, uint64_t _rs, size_t chunksize) {

    //std::cout<<"Memory used by every read's entry:"<< sizeof(PairedRead)<<std::endl;
    //read each read, put it on the index and on the appropriate tag

    uint64_t _size(0);
    sdglib::OutputLog(sdglib::LogLevels::INFO)<<"Creating Datastore from "<<read1_filename<<" | "<<read2_filename<<std::endl;
    auto fd1=fopen(read1_filename.c_str(),"r");
    if (!fd1) {
        std::cerr << "Failed to open " << read1_filename <<": " << strerror(errno);
        throw std::runtime_error("Could not open " + read1_filename);
    }
    auto fd2=fopen(read2_filename.c_str(),"r");
    if (!fd2) {
        std::cerr << "Failed to open " << read2_filename <<": " << strerror(errno);
        throw std::runtime_error("Could not open " + read2_filename);
    }
    char readbuffer[3000];
    memset(readbuffer, 0, 3000);
    //first, build an index of tags and offsets
    sdglib::OutputLog()<<"Reading chunks of "<<chunksize<<" pairs"<<std::endl;
    std::vector<PairedReadData> readdatav;
    readdatav.reserve(chunksize);
    std::vector<std::ifstream> chunkfiles;
    std::vector<PairedReadData> next_in_chunk;
    PairedReadData currrent_read;
    //First, create the chunk files
    uint64_t pairs=0,discarded=0,truncated=0;
    std::ofstream output(output_filename.c_str());

    output.write((const char *) &BSG_MAGIC, sizeof(BSG_MAGIC));
    output.write((const char *) &BSG_VN, sizeof(BSG_VN));
    BSG_FILETYPE type(PairedDS_FT);
    output.write((char *) &type, sizeof(type));

    output.write((const char *) &_rs,sizeof(readsize));
    auto size_pos=output.tellp();
    output.write((const char *) &_size, sizeof(_size));//just to save the space!
    while (!feof(fd1) and !feof(fd2)) {

        if (NULL == fgets(readbuffer, 2999, fd1)) continue;
        if (NULL == fgets(readbuffer, 2999, fd1)) continue;
        currrent_read.seq1=std::string(readbuffer);
        if (NULL == fgets(readbuffer, 2999, fd1)) continue;
        if (NULL == fgets(readbuffer, 2999, fd1)) continue;
        if (currrent_read.seq1.back()=='\n') currrent_read.seq1.resize(currrent_read.seq1.size()-1);
        else {std::cout<<"READ IS LONGER THAN 2998bp. ABORTING!!!! Get your act together and choose the right datastore."<<std::endl; exit(1);};
        if (NULL == fgets(readbuffer, 2999, fd2)) continue;
        if (NULL == fgets(readbuffer, 2999, fd2)) continue;
        currrent_read.seq2=std::string(readbuffer);
        if (NULL == fgets(readbuffer, 2999, fd2)) continue;
        if (NULL == fgets(readbuffer, 2999, fd2)) continue;
        if (currrent_read.seq2.back()=='\n') currrent_read.seq2.resize(currrent_read.seq2.size()-1);
        else {std::cout<<"READ IS LONGER THAN 2998bp. ABORTING!!!! Get your act together and choose the right datastore"<<std::endl; exit(1);};
        if (currrent_read.seq1.size()<_min_rs or currrent_read.seq2.size()<_min_rs) {
            ++discarded;
            continue;
        }
        if (currrent_read.seq1.size()>_rs) {
            ++truncated;
            currrent_read.seq1.resize(_rs);
        }
        if (currrent_read.seq2.size()>_rs) {
            ++truncated;
            currrent_read.seq2.resize(_rs);
        }
        ++pairs;
        readdatav.push_back(currrent_read);
        if (readdatav.size()==chunksize){
            //dump
            char buffer[2*_rs+2];
            bzero(buffer,2*_rs+2);
            for (auto &r:readdatav){
                bzero(buffer,2*_rs+2);
                memcpy(buffer,r.seq1.data(),(r.seq1.size()>_rs ? _rs : r.seq1.size()));
                memcpy(buffer+_rs+1,r.seq2.data(),(r.seq2.size()>_rs ? _rs : r.seq2.size()));
                output.write(buffer,2*_rs+2);
            }
            sdglib::OutputLog()<<readdatav.size()<<" pairs dumped..."<<std::endl;
            readdatav.clear();
        }
    }
    if (readdatav.size()>0) {
        //dump
        char buffer[2*_rs+2];
        bzero(buffer,2*_rs+2);
        for (auto &r:readdatav){
            bzero(buffer,2*_rs+2);
            memcpy(buffer,r.seq1.data(),(r.seq1.size()>_rs ? _rs : r.seq1.size()));
            memcpy(buffer+_rs+1,r.seq2.data(),(r.seq2.size()>_rs ? _rs : r.seq2.size()));
            output.write(buffer,2*_rs+2);
        }
        sdglib::OutputLog()<<readdatav.size()<<" pairs dumped..."<<std::endl;
        readdatav.clear();
    }
    _size=pairs*2;
    output.seekp(size_pos);
    output.write((const char *) &_size, sizeof(_size));
    output.close();
    //DONE!
    sdglib::OutputLog(sdglib::LogLevels::INFO)<<discarded<<" pairs discarded due to short reads"<<std::endl;
    sdglib::OutputLog(sdglib::LogLevels::INFO)<<truncated<<" reads where truncated to "<<_rs<<"bp"<<std::endl;
    sdglib::OutputLog(sdglib::LogLevels::INFO)<<"Datastore with "<<_size<<" reads ("<<pairs<<" pairs)"<<std::endl;
}

void PairedReadsDatastore::read(std::ifstream &input_file) {
    //read filename
    uint64_t s;
    input_file.read((char *) &s, sizeof(s));
    filename.resize(s);
    input_file.read((char *) filename.data(), filename.size());
    load_index();
}

void PairedReadsDatastore::load_index(){
    fd=fopen(filename.c_str(),"r");
    if (!fd) {
        std::cerr << "Failed to open " << filename <<": " << strerror(errno);
        throw std::runtime_error("Could not open " + filename);
    }
    bsgMagic_t magic;
    bsgVersion_t version;
    BSG_FILETYPE type;
    fread((char *) &magic, sizeof(magic),1,fd);
    fread((char *) &version, sizeof(version),1,fd);
    fread((char *) &type, sizeof(type),1,fd);

    if (magic != BSG_MAGIC) {
        throw std::runtime_error(filename + " appears to be corrupted");
    }

    if (version < min_compat) {
        throw std::runtime_error("Incompatible version");
    }

    if (type != PairedDS_FT) {
        throw std::runtime_error("Incompatible file type");
    }

    fread( &readsize,sizeof(readsize),1,fd);
    fread( &_size,sizeof(_size),1,fd);
    readpos_offset=ftell(fd);
    sdglib::OutputLog()<<"PairedReadsDatastore open: "<<filename<<"  max read length: "<<readsize<<" Total reads: " <<size()<<std::endl;
}

void PairedReadsDatastore::write(std::ofstream &output_file) {
    //read filename
    uint64_t s=filename.size();
    output_file.write((char *) &s,sizeof(s));
    output_file.write((char *)filename.data(),filename.size());
}

void PairedReadsDatastore::write_selection(std::ofstream &output_file, std::vector<uint64_t> read_ids) {
    for (auto i=0;i<read_ids.size()-1;i+=2){
        if (read_ids[i]+1!=read_ids[i+1]) {
            sdglib::OutputLog()<<"ERROR: paired read selection not paired!"<<std::endl;
            return;//exit if not properly paired
        }
    }
    output_file.write((const char *) &BSG_MAGIC, sizeof(BSG_MAGIC));
    output_file.write((const char *) &BSG_VN, sizeof(BSG_VN));
    BSG_FILETYPE type(PairedDS_FT);
    output_file.write((char *) &type, sizeof(type));

    output_file.write((char *) &readsize,sizeof(readsize));
    uint64_t rids_size=read_ids.size();
    output_file.write((char *) &rids_size,sizeof(rids_size));
    char buffer[2*readsize+2];
    for (auto i=0;i<read_ids.size()-1;i+=2) {
        size_t read_offset_in_file = readpos_offset + (readsize + 1) * (read_ids[i] - 1);
        fseek(fd, read_offset_in_file, SEEK_SET);
        fread(buffer, 2 * readsize + 2, 1, fd);
        output_file.write(buffer,2 * readsize + 2);
    }
}

std::string PairedReadsDatastore::get_read_sequence(size_t readID) {
    char buffer[readsize+1];
    size_t read_offset_in_file=readpos_offset+(readsize+1)*(readID-1);
    fseek(fd,read_offset_in_file,SEEK_SET);
    fread(buffer,readsize+1,1,fd);
    return std::string(buffer);
}


const char* BufferedPairedSequenceGetter::get_read_sequence(uint64_t readID) {
    size_t read_offset_in_file=datastore.readpos_offset+(datastore.readsize+1)*(readID-1);
    if (read_offset_in_file<buffer_offset or read_offset_in_file+chunk_size>buffer_offset+bufsize) {
        buffer_offset=read_offset_in_file;
        lseek(fd,read_offset_in_file,SEEK_SET);
        read(fd,buffer,bufsize);
    }
    return buffer+(read_offset_in_file-buffer_offset);
}

std::unordered_set<__uint128_t, int128_hash> PairedReadsDatastore::get_all_kmers128(int k, int min_tag_cov) {
    class StreamKmerFactory128 : public  KMerFactory128 {
    public:
        explicit StreamKmerFactory128(uint8_t k) : KMerFactory128(k){}
        inline void produce_all_kmers(const char * seq, std::vector<__uint128_t> &mers){
            // TODO: Adjust for when K is larger than what fits in __uint128_t!
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
                    } else {
                        // Is bwd
                        mers.emplace_back(rkmer);
                    }
                }
                ++s;
            }
        }
    };
    StreamKmerFactory128 skf(k);

    //reserve space by counting reads first, save only the integer, do not merge just count and insert in the set
    std::vector<__uint128_t> all_kmers;
    BufferedPairedSequenceGetter bprsg(*this,100000,1000);
    for (auto rid=1;rid<=size();++rid) {
        skf.produce_all_kmers(bprsg.get_read_sequence(rid), all_kmers);
    }

    std::sort(all_kmers.begin(),all_kmers.end());
    std::unordered_set<__uint128_t, int128_hash> kset;
    auto ri=all_kmers.begin();
    auto nri=all_kmers.begin();
    while (ri<all_kmers.end()){
        while (nri<all_kmers.end() and *nri==*ri) ++nri;
        if (nri-ri>=min_tag_cov) kset.insert(*ri);
        ri=nri;
    }
    return std::move(kset);
}

std::unordered_set<__uint128_t, int128_hash> PairedReadsDatastore::get_reads_kmers128(int k, int min_tag_cov, std::vector<uint64_t> reads) {
    class StreamKmerFactory128 : public  KMerFactory128 {
    public:
        explicit StreamKmerFactory128(uint8_t k) : KMerFactory128(k){}
        inline void produce_all_kmers(const char * seq, std::vector<__uint128_t> &mers){
            // TODO: Adjust for when K is larger than what fits in __uint128_t!
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
                    } else {
                        // Is bwd
                        mers.emplace_back(rkmer);
                    }
                }
                ++s;
            }
        }
    };
    StreamKmerFactory128 skf(k);

    //reserve space by counting reads first, save only the integer, do not merge just count and insert in the set
    std::vector<__uint128_t> all_kmers;
    BufferedPairedSequenceGetter bprsg(*this,100000,1000);
    for (auto rid:reads) {
        skf.produce_all_kmers(bprsg.get_read_sequence(rid), all_kmers);
    }

    std::sort(all_kmers.begin(),all_kmers.end());
    std::unordered_set<__uint128_t, int128_hash> kset;
    auto ri=all_kmers.begin();
    auto nri=all_kmers.begin();
    while (ri<all_kmers.end()){
        while (nri<all_kmers.end() and *nri==*ri) ++nri;
        if (nri-ri>=min_tag_cov) kset.insert(*ri);
        ri=nri;
    }
    return std::move(kset);
}

PairedReadsDatastore::PairedReadsDatastore(WorkSpace &ws, std::string _filename) : ws(ws), mapper(ws, *this) {
    filename=_filename;
    load_index();
}

PairedReadsDatastore::PairedReadsDatastore(WorkSpace &ws, std::string read1_filename, std::string read2_filename,
                                           std::string output_filename, int min_readsize, int max_readsize) : ws(ws), mapper(ws, *this) {
    build_from_fastq(read1_filename,read2_filename,output_filename,min_readsize,max_readsize);
}

uint64_t PairedReadsDatastore::size() const {return _size;}

PairedReadsDatastore::PairedReadsDatastore(WorkSpace &ws, std::ifstream &infile) : ws(ws), mapper{ws, *this} {
    read(infile);
    mapper.read(infile);
}

PairedReadsDatastore::PairedReadsDatastore(WorkSpace &ws, std::string _filename, std::ifstream &input_file) : ws(ws), mapper(ws, *this) {
    uint64_t s;
    filename=_filename;
    fd=fopen(filename.c_str(),"r");
    bsgMagic_t magic;
    bsgVersion_t version;
    BSG_FILETYPE type;
    input_file.read((char *) &magic, sizeof(magic));
    input_file.read((char *) &version, sizeof(version));
    input_file.read((char *) &type, sizeof(type));

    if (magic != BSG_MAGIC) {
        throw std::runtime_error(filename + " appears to be corrupted");
    }

    if (version < min_compat) {
        throw std::runtime_error(filename + "has an ncompatible version");
    }

    if (type != PairedDS_FT) {
        throw std::runtime_error(filename + " has an incompatible file type");
    }

    input_file.read( (char *) &readsize,sizeof(readsize));
    input_file.read( (char *) &_size,sizeof(_size));
    readpos_offset=input_file.tellg();
    fseek(fd,readpos_offset,SEEK_SET);
    input_file.seekg(_size*(readsize+1),std::ios_base::cur);
    sdglib::OutputLog()<<"PairedReadsDatastore open: "<<_filename<<"  max read length: "<<readsize<<" Total reads: " <<size()<<std::endl;

}

PairedReadsDatastore::PairedReadsDatastore(WorkSpace &ws, PairedReadsDatastore &o) : ws(ws), mapper(ws, *this) {
    this->readsize = o.readsize;
    this->filename = o.filename;
    this->readpos_offset = o.readpos_offset;
    this->_size = o._size;
    this->fd = fopen(o.filename.c_str(), "r");

    this->mapper.reads_in_node = o.mapper.reads_in_node;
    this->mapper.read_to_node = o.mapper.read_to_node;
    this->mapper.frdist = o.mapper.frdist;
    this->mapper.rfdist = o.mapper.rfdist;
    this->mapper.read_direction_in_node = o.mapper.read_direction_in_node;
}

PairedReadsDatastore& PairedReadsDatastore::operator=(PairedReadsDatastore const &o) {
    if (&o == this) return *this;

    this->mapper = o.mapper;
    this->filename = o.filename;
    this->_size = o._size;
    this->readpos_offset = o.readpos_offset;
    this->readsize = o.readsize;
    this->ws = o.ws;
    this->fd = fopen(filename.c_str(), "r");
    return *this;
}