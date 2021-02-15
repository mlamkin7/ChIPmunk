// Modified from https://github.com/tfwillems/HipSTR/blob/master/src/bam_io.cpp

#ifndef BAM_IO_H_
#define BAM_IO_H_

#include <algorithm>
#include <iostream>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <map>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#include "htslib/bgzf.h"
#include "htslib/cram.h"
#include "htslib/hts.h"

#define bam_ins_size(b)  (b)->core.isize;
#include "htslib/sam.h"

#include "common.h"

// htslib encodes each base using a 4 bit integer
// This array converts each integer to its corresponding base
static const char HTSLIB_INT_TO_BASE[16] = {' ', 'A', 'C', ' ',
					    'G', ' ', ' ', ' ', 
					    'T', ' ', ' ', ' ', 
					    ' ', ' ', ' ', 'N'};

class CigarOp {
 public:
  char Type;
  int32_t Length;
  
  CigarOp(char type, int32_t length){
    Type   = type;
    Length = length;
  }
};




class BamAlignment {
 private:
  std::string bases_;
  std::string qualities_;
  std::vector<CigarOp> cigar_ops_;

  void ExtractSequenceFields();

 public:
  bam1_t *b_;
  std::string file_;
  bool built_;
  int32_t length_;
  int32_t pos_, end_pos_;

  BamAlignment(){
    b_       = bam_init1();
    built_   = false;
    length_  = -1;
    pos_     = 0;
    end_pos_ = -1;
  }

  BamAlignment(const BamAlignment &aln)
    : bases_(aln.bases_), qualities_(aln.qualities_), cigar_ops_(aln.cigar_ops_), file_(aln.file_){
    b_ = bam_init1();
    bam_copy1(b_, aln.b_);
    built_     = aln.built_;
    length_    = aln.length_;
    pos_       = aln.pos_;
    end_pos_   = aln.end_pos_;
  }

  BamAlignment& operator=(const BamAlignment& aln){
    bam_copy1(b_, aln.b_);
    file_      = aln.file_;
    built_     = aln.built_;
    length_    = aln.length_;
    pos_       = aln.pos_;
    end_pos_   = aln.end_pos_;
    bases_     = aln.bases_;
    qualities_ = aln.qualities_;
    cigar_ops_ = aln.cigar_ops_;
    return *this;
  }

  ~BamAlignment(){
    bam_destroy1(b_);
  }

  /* Number of bases */
  int32_t Length()              const { return length_;  }

  /* 0-based position where alignment starts*/
  int32_t Position()            const { return pos_;     }

  /* Non-inclusive end position of alignment */
  int32_t GetEndPosition()      const { return end_pos_; }

  /* Template length */
  int32_t TemplateLength()      const { return bam_ins_size(b_); }

  /* Name of the read */
  std::string Name()            const { return std::string(bam_get_qname(b_)); }

  /* ID number for reference sequence */
  int32_t RefID()               const { return b_->core.tid;      }

  /* Mapping quality score*/
  uint16_t MapQuality()         const { return b_->core.qual;     }

  /* ID number for mate's reference sequence */
  int32_t MateRefID()           const { return b_->core.mtid;     }

  /* 0-based position where mate's alignment starts */
  int32_t MatePosition()        const { return b_->core.mpos;     }

  /* Name of file from which the alignment was read */
  const std::string& Filename() const { return file_;             }
  
  /* Sequenced bases */
  const std::string& QueryBases(){
    if (!built_) ExtractSequenceFields();
    return bases_;
  }

  /* Quality score for each base */
  const std::string& Qualities(){
    if (!built_) ExtractSequenceFields();
    return qualities_;
  }

  const std::vector<CigarOp>& CigarData(){
    if (!built_) ExtractSequenceFields();
    return cigar_ops_;
  }

  bool RemoveTag(const char tag[2]) const {
    uint8_t* tag_data = bam_aux_get(b_, tag);
    if (tag_data == NULL)
      return false;
    return (bam_aux_del(b_, tag_data) == 0);
  }

  bool HasTag(const char tag[2]) const { return bam_aux_get(b_, tag) != NULL; }

  /*
  bool AddCharTag(const char tag[2], char& value){
    if (HasTag(tag))
      return false;
    return (bam_aux_append(b_, tag, 'A', 1, (uint8_t*)&value) == 0);
  }
  bool AddIntTag(const char tag[2], int64_t& value){
    if (HasTag(tag))
      return false;
    return (bam_aux_append(b_, tag, 'i', ___, (uint8_t*)&value) == 0);
  }
  bool AddFloatTag(const char tag[2], double& value){
    if (HasTag(tag))
      return false;
    return (bam_aux_append(b_, tag, 'f', ___, (uint8_t*)&value) == 0);
  }
  */

  bool AddStringTag(const char tag[2], const std::string& value){
    if (HasTag(tag))
      return false;
    bam_aux_append(b_, tag, 'Z', value.size()+1, (uint8_t*)value.c_str());
    return true;
  }

  bool GetCharTag(const char tag[2], char& value) const {
    uint8_t* tag_data = bam_aux_get(b_, tag);
    if (tag_data == NULL)
      return false;
    value = bam_aux2A(tag_data);
    return true; // TO DO: Check errno
  }

  bool GetIntTag(const char tag[2], int64_t& value) const {
    uint8_t* tag_data = bam_aux_get(b_, tag);
    if (tag_data == NULL)
      return false;
    value = bam_aux2i(tag_data);
    return true; // TO DO: Check errno
  }

  bool GetFloatTag(const char tag[2], double& value) const {
    uint8_t* tag_data = bam_aux_get(b_, tag);
    if (tag_data == NULL)
      return false;
    value = bam_aux2f(tag_data);
    return true; // TO DO: Check errno
  }
  
  bool GetStringTag(const char tag[2], std::string& value) const{
    uint8_t* tag_data = bam_aux_get(b_, tag);
    if (tag_data == NULL)
      return false;
    char* ptr = bam_aux2Z(tag_data);
    value = std::string(ptr);
    return true; // TO DO: Check errno
  }  

  bool IsDuplicate()         const { return (b_->core.flag & BAM_FDUP)         != 0;}
  bool IsFailedQC()          const { return (b_->core.flag & BAM_FQCFAIL)      != 0;}
  bool IsMapped()            const { return (b_->core.flag & BAM_FUNMAP)       == 0;}
  bool IsMateMapped()        const { return (b_->core.flag & BAM_FMUNMAP)      == 0;}
  bool IsReverseStrand()     const { return (b_->core.flag & BAM_FREVERSE)     != 0;}
  bool IsMateReverseStrand() const { return (b_->core.flag & BAM_FMREVERSE)    != 0;}
  bool IsPaired()            const { return (b_->core.flag & BAM_FPAIRED)      != 0;}
  bool IsProperPair()        const { return (b_->core.flag & BAM_FPROPER_PAIR) != 0;}
  bool IsFirstMate()         const { return (b_->core.flag & BAM_FREAD1)       != 0;}
  bool IsSecondMate()        const { return (b_->core.flag & BAM_FREAD2)       != 0;}
  bool IsSupplementary()     const { return (b_->core.flag & BAM_FSUPPLEMENTARY) != 0;}
  bool IsSecondary() const { return (b_->core.flag & BAM_FSECONDARY) != 0;}

  bool StartsWithSoftClip(){
    if (!built_) ExtractSequenceFields();
    if (cigar_ops_.empty())
      return false;
    return cigar_ops_.front().Type == 'S';
  }

  bool EndsWithSoftClip(){
    if (!built_) ExtractSequenceFields();
    if (cigar_ops_.empty())
      return false;
    return cigar_ops_.back().Type == 'S';
  }

  bool StartsWithHardClip(){
    if (!built_) ExtractSequenceFields();
    if (cigar_ops_.empty())
      return false;
    return cigar_ops_.front().Type == 'H';
  }

  bool EndsWithHardClip(){
    if (!built_) ExtractSequenceFields();
    if (cigar_ops_.empty())
      return false;
    return cigar_ops_.back().Type == 'H';
  }

  bool MatchesReference(){
    if (!built_) ExtractSequenceFields();
    for (std::vector<CigarOp>::iterator cigar_iter = cigar_ops_.begin(); \
	 cigar_iter != cigar_ops_.end(); cigar_iter++)
      if (cigar_iter->Type != 'M' && cigar_iter->Type != '=')
	return false;
    return true;
  }

  void SetIsDuplicate(bool ok){
    if (ok) b_->core.flag |= BAM_FDUP;
    else    b_->core.flag &= (~BAM_FDUP);
  }

  void SetIsFailedQC(bool ok){
    if (ok) b_->core.flag |= BAM_FQCFAIL;
    else    b_->core.flag &= (~BAM_FQCFAIL);
  }

  void SetIsMapped(bool ok){
    if (ok) b_->core.flag &= (~BAM_FUNMAP);
    else    b_->core.flag |= BAM_FUNMAP;
  }

  void SetIsMateMapped(bool ok){
    if (ok) b_->core.flag &= (~BAM_FMUNMAP);
    else    b_->core.flag |= BAM_FMUNMAP;
  }

  void SetIsReverseStrand(bool ok){
    if (ok) b_->core.flag |= BAM_FREVERSE;
    else    b_->core.flag &= (~BAM_FREVERSE);
  }

  void SetIsMateReverseStrand(bool ok){
    if (ok) b_->core.flag |= BAM_FMREVERSE;
    else    b_->core.flag &= (~BAM_FMREVERSE);
  }

  void SetIsPaired(bool ok){
    if (ok) b_->core.flag |= BAM_FPAIRED;
    else    b_->core.flag &= (~BAM_FPAIRED);
  }

  void SetIsProperPair(bool ok){
    if (ok) b_->core.flag |= BAM_FPROPER_PAIR;
    else    b_->core.flag &= (~BAM_FPROPER_PAIR);
  }

  void SetIsFirstMate(bool ok){
    if (ok) b_->core.flag |= BAM_FREAD1;
    else    b_->core.flag &= (~BAM_FREAD1);
  }

  void SetIsSecondMate(bool ok){
    if (ok) b_->core.flag |= BAM_FREAD2;
    else    b_->core.flag &= (~BAM_FREAD2);
  }

  /*
   *  Trim an alignment that extends too far upstream or downstream of the provided region or has low base qualities on the ends
   *  Trims until either i) the base quality exceeds the provided threshold or ii) the alignment is fully within the provided region bounds
   *  Modifies the alignment such that subsequent calls to each function reflect the trimmming
   *  However, if the aligment is written to a new BAM file, the original alignment will be output
   */
  void TrimAlignment(int32_t min_read_start, int32_t max_read_stop, char min_base_qual='~');

  void TrimLowQualityEnds(char min_base_qual);
};







class ReadGroup {
 private:
  std::string id_;
  std::string sample_;
  std::string library_;

 public:
  ReadGroup(){}

  ReadGroup(const std::string& id, const std::string& sample, const std::string& library)
    : id_(id), sample_(sample), library_(library){}

  bool HasID()      const { return !id_.empty();      }
  bool HasSample()  const { return !sample_.empty();  }
  bool HasLibrary() const { return !library_.empty(); }

  const std::string& GetID()      const { return id_;      }
  const std::string& GetSample()  const { return sample_;  }
  const std::string& GetLibrary() const { return library_; }

  void SetID(const std::string& id)          { id_      = id;      }
  void SetSample(const std::string& sample)  { sample_  = sample;  }
  void SetLibrary(const std::string& library){ library_ = library; }
};






class BamHeader {
 private:
  std::map<std::string, int32_t> seq_indices_;
  std::vector<std::string> seq_names_;
  std::vector<uint32_t> seq_lengths_;
  std::vector<ReadGroup> read_groups_;

  void parse_read_groups();

 public:
  bam_hdr_t *header_;

  explicit BamHeader(bam_hdr_t *header){
    header_ = bam_hdr_dup(header);
    for (int32_t i = 0; i < header_->n_targets; i++){
      seq_names_.push_back(std::string(header_->target_name[i]));
      seq_lengths_.push_back(header_->target_len[i]);
      seq_indices_.insert(std::pair<std::string, int32_t>(seq_names_.back(), i));
    }
    parse_read_groups();
  }

  BamHeader(const BamHeader& other){
    header_      = bam_hdr_dup(other.header_);
    seq_indices_ = other.seq_indices_;
    seq_names_   = other.seq_names_;
    seq_lengths_ = other.seq_lengths_;
    read_groups_ = other.read_groups_;
  }

  BamHeader& operator=(const BamHeader& other){
    bam_hdr_destroy(header_);
    header_      = bam_hdr_dup(other.header_);
    seq_indices_ = other.seq_indices_;
    seq_names_   = other.seq_names_;
    seq_lengths_ = other.seq_lengths_;
    read_groups_ = other.read_groups_;
    return *this;
  }

  const std::vector<uint32_t>& seq_lengths()  const { return seq_lengths_; }
  const std::vector<std::string>& seq_names() const { return seq_names_;   }
  const std::vector<ReadGroup>& read_groups() const { return read_groups_; }

  int32_t num_seqs() const { return header_->n_targets; }
  int32_t ref_id(const std::string& ref) const {
    std::map< std::string, int32_t >::const_iterator iter = seq_indices_.find(ref);
    //    auto iter = seq_indices_.find(ref);
    if (iter == seq_indices_.end())
      return -1;
    return iter->second;
  }
  
  std::string ref_name(int32_t ref_id) const {
    if (ref_id == -1)
      return "*";
    if (ref_id >= 0 && ref_id < (int)seq_names_.size())
      return seq_names_[ref_id];
    PrintMessageDieOnError("Invalid reference ID provided to ref_name() function", M_ERROR);
  }

  uint32_t ref_length(int32_t ref_id) const {
    if (ref_id >= 0 && ref_id < (int)seq_lengths_.size())
      return seq_lengths_[ref_id];
    PrintMessageDieOnError("Invalid reference ID provided to ref_length() function", M_ERROR);
  }

  ~BamHeader(){
    bam_hdr_destroy(header_);
  }
};

class BamCramReader {
 private:
  samFile   *in_;
  bam_hdr_t *hdr_;
  hts_idx_t *idx_;
  std::string path_;
  BamHeader*  header_;

  // Instance variables for the most recently set region
  hts_itr_t *iter_;        // Iterator
  std::string chrom_;      // Chromosome
  int32_t     start_;      // Start pos
  uint64_t    min_offset_; // Offset after first alignment
  BamAlignment first_aln_; // First alignment

  // Private unimplemented copy constructor and assignment operator to prevent operations
  BamCramReader(const BamCramReader& other);
  BamCramReader& operator=(const BamCramReader& other);

  bool file_exists(const std::string& path){
    return (access(path.c_str(), F_OK) != -1);
  }

 public:
  BamCramReader(const std::string& path, std::string fasta_path = "")
    : path_(path), chrom_(""){

    // Open the file itself
    if (!file_exists(path))
      PrintMessageDieOnError("File " + path + " doest not exist", M_ERROR);
    in_ = sam_open(path.c_str(), "r");
    if (in_ == NULL)
      PrintMessageDieOnError("Failed to open file " + path, M_ERROR);
    
    if (in_->is_cram) {
      if (fasta_path.empty()) {
	PrintMessageDieOnError("Must specify a FASTA reference file path for CRAM file " + path, M_ERROR);
      }
      if (!file_exists(fasta_path+".fai")) {
	PrintMessageDieOnError("File " + fasta_path + ".fai doest not exist", M_ERROR);
      }
      // Open fasta fai file for CRAM
      std::string fai_path = fasta_path+".fai";
      char* fai = new char[fai_path.size()+1];
      for (size_t i = 0; i<fai_path.size(); ++i) {
	fai[i] = fai_path[i];
      }
      fai[fai_path.size()] = '\0';

      if (hts_set_fai_filename(in_, fai) <0 ) {
	PrintMessageDieOnError("Failed to open FASTA reference file for CRAM file", M_ERROR);
      }
      delete [] fai;
    }

    // Read the header
    if ((hdr_ = sam_hdr_read(in_)) == 0)
      PrintMessageDieOnError("Failed to read the header for file " + path, M_ERROR);
    header_ = new BamHeader(hdr_);

    // Open the index
    idx_ = sam_index_load(in_, path.c_str());
    if (idx_ == NULL) 
      PrintMessageDieOnError("Failed to load the index for file " + path, M_ERROR);

    iter_       = NULL;
    start_      = -1;
    min_offset_ = 0;
  }

  const BamHeader* bam_header() const { return header_; }
  const std::string& path()     const { return path_;   }
  
  ~BamCramReader(){
    bam_hdr_destroy(hdr_);
    delete header_;
    sam_close(in_);

    hts_idx_destroy(idx_);
    
    if (iter_ != NULL)
      hts_itr_destroy(iter_);
    
  }

  bool GetNextAlignment(BamAlignment& aln);
  
  bool SetRegion(const std::string& chrom, int32_t start, int32_t end);
};


void compare_bam_headers(const BamHeader* hdr_a, const BamHeader* hdr_b, const std::string& file_a, const std::string& file_b);






class BamCramMultiReader {
 private:
  std::vector<BamCramReader*> bam_readers_;
  std::vector<BamAlignment> cached_alns_;
  std::vector<std::pair<int32_t, int32_t> > aln_heap_;
  int merge_type_;

  // Private unimplemented copy constructor and assignment operator to prevent operations
  BamCramMultiReader(const BamCramMultiReader& other);
  BamCramMultiReader& operator=(const BamCramMultiReader& other);

 public:
  const static int ORDER_ALNS_BY_POSITION = 0;
  const static int ORDER_ALNS_BY_FILE     = 1;

  BamCramMultiReader(const std::vector<std::string>& paths, std::string fasta_path = "", int merge_type = ORDER_ALNS_BY_POSITION){
    if (paths.empty())
      PrintMessageDieOnError("Must provide at least one file to BamCramMultiReader constructor", M_ERROR);
    if (merge_type != ORDER_ALNS_BY_POSITION && merge_type != ORDER_ALNS_BY_FILE)
      PrintMessageDieOnError("Invalid merge type provided to BamCramMultiReader constructor", M_ERROR);
    for (size_t i = 0; i < paths.size(); i++){
      cached_alns_.push_back(BamAlignment());
      bam_readers_.push_back(new BamCramReader(paths[i], fasta_path));
      compare_bam_headers(bam_readers_[0]->bam_header(), bam_readers_[i]->bam_header(), paths[0], paths[i]);
    }
    merge_type_ = merge_type;
  }

  ~BamCramMultiReader(){
    for (size_t i = 0; i < bam_readers_.size(); i++)
      delete bam_readers_[i];
  }

  int get_merge_type() const { return merge_type_; }

  const BamHeader* bam_header() const {
    return bam_readers_[0]->bam_header();
  }

  const BamHeader* bam_header(int file_index) const {
    if (file_index >= 0 && file_index < (int)bam_readers_.size())
      return bam_readers_[file_index]->bam_header(); 
    PrintMessageDieOnError("Invalid file index provided to bam_header() function", M_ERROR);
  }

  bool SetRegion(const std::string& chrom, int32_t start, int32_t end);

  bool GetNextAlignment(BamAlignment& aln);
};








class BamWriter {
 private:
  BGZF* output_;

  // Private unimplemented copy constructor and assignment operator to prevent operations
  BamWriter(const BamWriter& other);
  BamWriter& operator=(const BamWriter& other);

 public:
  BamWriter(const std::string& path, const BamHeader* bam_header){
    std::string mode = "w";
    output_ = bgzf_open(path.c_str(), mode.c_str());
    if (output_ == NULL)
      PrintMessageDieOnError("Failed to open BAM output file", M_ERROR);
    if (bam_hdr_write(output_, bam_header->header_) == -1)
      PrintMessageDieOnError("Failed to write the BAM header to the output file", M_ERROR);
  }

  void Close(){
    if (bgzf_close(output_) != 0)
      PrintMessageDieOnError("Failed to close BAM output file", M_ERROR);
    output_ = NULL;
  }

  bool SaveAlignment(BamAlignment& aln){
    if (output_ == NULL)
      return false;
    return (bam_write1(output_, aln.b_) != -1);
  }

  ~BamWriter(){
    if (output_ != NULL)
      bgzf_close(output_);
  }
};

#endif
