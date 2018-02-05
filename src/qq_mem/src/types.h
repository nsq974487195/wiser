#ifndef TYPES_H
#define TYPES_H


#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <sstream>

#include "qq.pb.h"


// for direct IO
#define PAGE_SIZE 512

// for flash based documents
#define FLAG_DOCUMENTS_ON_FLASH false
#define DOCUMENTS_CACHE_SIZE 100
#define DOCUMENTS_ON_FLASH_FILE "/mnt/ssd/documents_store"

// for flash based postings
#define FLAG_POSTINGS_ON_FLASH false
#define POSTINGS_CACHE_SIZE 100
#define POSTINGS_ON_FLASH_FILE "/mnt/ssd/postings_store"
#define POSTING_SERIALIZATION "cereal"    // cereal or protobuf

// for precomputing of snippets generating
#define FLAG_SNIPPETS_PRECOMPUTE false

// size of snippets cache of highlighter
#define SNIPPETS_CACHE_SIZE 100
#define FLAG_SNIPPETS_CACHE false
#define SNIPPETS_CACHE_ON_FLASH_SIZE 100
#define FLAG_SNIPPETS_CACHE_ON_FLASH false
#define SNIPPETS_CACHE_ON_FLASH_FILE "/mnt/ssd/snippets_store.cache"

// Basic types for query
typedef std::string Term;
typedef std::vector<Term> TermList;
typedef TermList Query;
enum class SearchOperator {AND, OR};
typedef int DocIdType;
typedef double qq_float;

// Basic types for highlighter
typedef std::vector<int> TopDocs;
typedef std::tuple<int, int> Offset;  // (startoffset, endoffset)
typedef Offset OffsetPair; // Alias for Offset, more intuitive
typedef std::vector<Offset> Offsets;
typedef Offsets OffsetPairs; // Alias for Offsets, more intuitive
typedef int Position;
typedef std::vector<Position> Positions;
typedef std::vector<std::string> Snippets;

typedef unsigned long byte_cnt_t;

class TermWithOffset { // class Term_With_Offset
    public:
        Term term_;
        Offsets offsets_;

        TermWithOffset(Term term_in, Offsets offsets_in) : term_(term_in), offsets_(offsets_in) {} 
};
typedef std::vector<TermWithOffset> TermWithOffsetList;


class DocInfo {
 public:
  DocInfo(std::string body, std::string tokens, std::string token_offsets, 
          std::string token_positions) 
    :body_(body), tokens_(tokens), token_offsets_(token_offsets), 
     token_positions_(token_positions) {}

  TermList GetTokens() const;

  // return a table of offset pairs
  // Each row is for a term
  std::vector<OffsetPairs> GetOffsetPairsVec() const;

  // return a table of positions
  // Each row is for a term
  std::vector<Positions> GetPositions() const;
  const std::string Body() const;
  const int BodyLength() const;

 private:
  const std::string body_;
  const std::string tokens_;
  const std::string token_offsets_;
  const std::string token_positions_;
};


struct SearchQuery {
  SearchQuery(){}
  SearchQuery(const qq::SearchRequest &grpc_req){
    this->CopyFrom(grpc_req);
  }
  SearchQuery(const TermList &terms_in): terms(terms_in) {}
  SearchQuery(const TermList &terms_in, const bool &return_snippets_in)
    : terms(terms_in), return_snippets(return_snippets_in) {}

  TermList terms;
  int n_results = 5;
  bool return_snippets = false;
  int n_snippet_passages = 3;
  bool is_phrase = false;

  std::string ToStr() const {
    std::ostringstream oss;
    for (auto &term : terms) {
      oss << term << " ";
    }
    oss << "n_results(" << n_results << ") "
        << "return_snippets(" << return_snippets << ") "
        << "n_snippet_passages(" << n_snippet_passages << ") "
        << "is_phrase(" << is_phrase << ") "
        << std::endl;
    return oss.str();
  }

  void CopyTo(qq::SearchRequest *request) {
    for (auto &term : terms) {
      request->add_terms(term);
    }

    request->set_n_results(n_results);
    request->set_return_snippets(return_snippets);
    request->set_n_snippet_passages(n_snippet_passages);
    request->set_is_phrase(is_phrase);
  }

  void CopyFrom(const qq::SearchRequest &grpc_req) {
    int n = grpc_req.terms_size();
    terms.clear();
    for (int i = 0; i < n; i++) {
      terms.push_back(grpc_req.terms(i));
    }

    n_results = grpc_req.n_results();
    return_snippets = grpc_req.return_snippets();
    n_snippet_passages = grpc_req.n_snippet_passages();
    is_phrase = grpc_req.is_phrase();
  }
};


struct SearchResultEntry {
  std::string snippet;
  DocIdType doc_id;
  qq_float doc_score;

  std::string ToStr() {
    return "DocId: " + std::to_string(doc_id) 
      + " Doc Score: " + std::to_string(doc_score)
      + " Snippet: " + snippet;
  }
  void CopyTo(qq::SearchReplyEntry *grpc_entry) {
    grpc_entry->set_doc_id(doc_id);
    grpc_entry->set_snippet(snippet);
    grpc_entry->set_doc_score(doc_score);
  }
};

struct SearchResult {
  std::vector<SearchResultEntry> entries;

  std::size_t Size() {return entries.size();}
  std::string ToStr() {
    std::string ret;
    for (auto entry : entries) {
      ret += entry.ToStr() + "\n";
    }
    return ret;
  }
  void CopyTo(qq::SearchReply *grpc_reply) {
    grpc_reply->clear_entries();
    for ( auto & result_entry : entries ) {
      qq::SearchReplyEntry *grpc_entry = grpc_reply->add_entries();
      result_entry.CopyTo(grpc_entry);
    }
  }
};


// Basic types for precomputation
// for snippet generating
typedef std::pair<int, float> Passage_Score; // (passage,score)
typedef std::vector<Passage_Score> Passage_Scores;
typedef std::pair<int, int> Passage_Split; // (start offset in offsets, len)
// for sentence breaking each document
typedef std::pair<int, int> Passage_Segment; // (startoffset, length)
typedef std::vector<Passage_Segment> Passage_Segments;

// Basic types for File Reader
typedef std::pair<int, int> Store_Segment;    // startoffset, length on flash based file

#endif
