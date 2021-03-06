#include "catch.hpp"

#include "qq.pb.h"

#include "highlighter.h"
#include "query_processing.h"
#include "utils.h"
#include "general_config.h"
#include "query_pool.h"
#include "posting_list_vec.h"
#include "histogram.h"
#include <grpc/support/histogram.h>


TEST_CASE( "SimpleHighlighter works", "[highlighter]" ) {
  std::string doc_str = "hello world";

  Offsets hello_offsets;
  hello_offsets.emplace_back( Offset{0, 4} ); 

  Offsets world_offsets;
  world_offsets.emplace_back( Offset{6, 10} ); 

  SECTION("One-term query") {
    OffsetsEnums enums;
    enums.push_back(Offset_Iterator(hello_offsets));

    SimpleHighlighter highlighter;
    auto s = highlighter.highlightOffsetsEnums(enums, 2, doc_str);
    REQUIRE(s == "<b>hello<\\b> world\n");
  }

  SECTION("Two-term query") {
    OffsetsEnums enums;
    enums.push_back(Offset_Iterator(hello_offsets));
    enums.push_back(Offset_Iterator(world_offsets));

    SimpleHighlighter highlighter;
    auto s = highlighter.highlightOffsetsEnums(enums, 2, doc_str);
    REQUIRE(s == "<b>hello<\\b> <b>world<\\b>\n");
  }
}


TEST_CASE( "SimpleHighlighter works for some corner cases", "[highlighter]" ) {
  SECTION("Empty") {
    // Currently it fails due to seg fault
    std::string doc_str = "";

    OffsetsEnums enums;

    SimpleHighlighter highlighter;
    auto s = highlighter.highlightOffsetsEnums(enums, 5, doc_str);
    REQUIRE(s == "");
  }


  SECTION("One letter") {
    // Currently it fails due to seg fault
    std::string doc_str = "0";

    Offsets offsets;
    offsets.emplace_back( Offset{0, 0} ); 

    OffsetsEnums enums;
    enums.push_back(Offset_Iterator(offsets));

    SimpleHighlighter highlighter;
    auto s = highlighter.highlightOffsetsEnums(enums, 5, doc_str);
    REQUIRE(s == "<b>0<\\b>\n");
  }

  SECTION("Two letter") {
    std::string doc_str = "0 1";

    Offsets offsets01;
    offsets01.emplace_back( Offset{0, 0} ); 

    Offsets offsets02;
    offsets02.emplace_back( Offset{2, 2} ); 

    OffsetsEnums enums;
    enums.push_back(Offset_Iterator(offsets01));
    enums.push_back(Offset_Iterator(offsets02));

    SimpleHighlighter highlighter;
    auto s = highlighter.highlightOffsetsEnums(enums, 5, doc_str);
    REQUIRE(s == "<b>0<\\b> <b>1<\\b>\n");
  }
}



TEST_CASE( "Extract offset pairs from a string", "[utils]" ) {
  SECTION("Adding entry") {
    std::map<Term, OffsetPairs> result;
    utils::add_term_offset_entry(&result, "hello", 5);
    utils::add_term_offset_entry(&result, "world", 11);
    utils::add_term_offset_entry(&result, "hello", 17);

    REQUIRE(result["hello"].size() == 2);
    REQUIRE(result["hello"][0] == std::make_tuple(0, 4));
    REQUIRE(result["hello"][1] == std::make_tuple(12, 16));

    REQUIRE(result["world"].size() == 1);
    REQUIRE(result["world"][0] == std::make_tuple(6, 10));
  }

  SECTION("hello world hello") {
    auto result = utils::extract_offset_pairs("hello world hello");

    REQUIRE(result.size() == 2);

    REQUIRE(result["hello"].size() == 2);
    REQUIRE(result["hello"][0] == std::make_tuple(0, 4));
    REQUIRE(result["hello"][1] == std::make_tuple(12, 16));

    REQUIRE(result["world"].size() == 1);
    REQUIRE(result["world"][0] == std::make_tuple(6, 10));
  }

  SECTION("empty string") {
    auto result = utils::extract_offset_pairs("");

    REQUIRE(result.size() == 0);
  }

  SECTION("one letter string") {
    auto result = utils::extract_offset_pairs("h");

    REQUIRE(result.size() == 1);
    REQUIRE(result["h"].size() == 1);
    REQUIRE(result["h"][0] == std::make_tuple(0, 0));
  }

  SECTION("space only string") {
    auto result = utils::extract_offset_pairs("    ");

    REQUIRE(result.size() == 0);
  }

  SECTION("pre and suffix spaces") {
    auto result = utils::extract_offset_pairs(" h ");

    REQUIRE(result.size() == 1);
    REQUIRE(result["h"].size() == 1);
    REQUIRE(result["h"][0] == std::make_tuple(1, 1));
  }

  SECTION("double space seperator") {
    auto result = utils::extract_offset_pairs("h  h");

    REQUIRE(result.size() == 1);

    REQUIRE(result["h"].size() == 2);
    REQUIRE(result["h"][0] == std::make_tuple(0, 0));
    REQUIRE(result["h"][1] == std::make_tuple(3, 3));
  }
}


TEST_CASE( "score_terms_in_doc()", "[score]" ) {
  // Assuming an engine that has the following documents
  //
  // hello world
  // hello wisconsin
  // hello world big world
  int n_total_docs_in_index = 3;
  qq_float avg_doc_length_in_index = (2 + 2 + 4) / 3.0;

  SECTION("Query wisconsin") {
    // This is document "hello wisconsin"
    int length_of_this_doc = 2;
    PostingListStandardVec pl01("wisconsin");
    pl01.AddPosting(StandardPosting(0, 1));

    PostingListStandardVec::PostingListVecIterator it = 
      pl01.Begin2();
    std::vector<qq_float> idfs_of_terms(1);
    idfs_of_terms[0] = calc_es_idf(3, 1);

    std::vector<PostingListStandardVec::PostingListVecIterator> iters{it};

    qq_float doc_score = 
      CalcDocScoreForOneQuery
      <typename PostingListStandardVec::PostingListVecIterator>(
        iters, idfs_of_terms, n_total_docs_in_index, avg_doc_length_in_index,
        length_of_this_doc);

    REQUIRE(utils::format_double(doc_score, 3) == "1.09");
  }

  SECTION("Query hello") {
    // This is document "hello world"
    int length_of_this_doc = 2;
  
    PostingListStandardVec pl01("hello");
    pl01.AddPosting(StandardPosting(0, 1));

    PostingListStandardVec::PostingListVecIterator it = 
      pl01.Begin2();
    std::vector<qq_float> idfs_of_terms(1);
    idfs_of_terms[0] = calc_es_idf(3, 3);

    std::vector<PostingListStandardVec::PostingListVecIterator> iters{it};

    qq_float doc_score = 
      CalcDocScoreForOneQuery
      <typename PostingListStandardVec::PostingListVecIterator>(
        iters, idfs_of_terms, n_total_docs_in_index, avg_doc_length_in_index,
        length_of_this_doc);

    REQUIRE(utils::format_double(doc_score, 3) == "0.149");
  }

  SECTION("Query hello+world") {
    // This is document "hello world"
    int length_of_this_doc = 2;

    PostingListStandardVec pl01("hello");
    pl01.AddPosting(StandardPosting(0, 1));

    PostingListStandardVec pl02("world");
    pl02.AddPosting(StandardPosting(0, 1));

    PostingListStandardVec::PostingListVecIterator it01 = 
      pl01.Begin2();
    PostingListStandardVec::PostingListVecIterator it02 = 
      pl02.Begin2();

    std::vector<qq_float> idfs_of_terms(2);
    idfs_of_terms[0] = calc_es_idf(3, 3);
    idfs_of_terms[1] = calc_es_idf(3, 2);

    std::vector<PostingListStandardVec::PostingListVecIterator> iters{it01, it02};

    qq_float doc_score = 
      CalcDocScoreForOneQuery
      <typename PostingListStandardVec::PostingListVecIterator>(
        iters, idfs_of_terms, n_total_docs_in_index, avg_doc_length_in_index,
        length_of_this_doc);

    REQUIRE(utils::format_double(doc_score, 3) == "0.672");
  }
}


TEST_CASE( "Config basic operations are OK", "[config]" ) {
  GeneralConfig config; 
  config.SetInt("mykey", 2);
  config.SetString("mykey", "myvalue");
  config.SetBool("mykey", true);
  config.SetStringVec("mykey", std::vector<std::string>{"hello", "world"});

  REQUIRE(config.GetInt("mykey") == 2);
  REQUIRE(config.GetString("mykey") == "myvalue");
  REQUIRE(config.GetBool("mykey") == true);
  REQUIRE(config.GetStringVec("mykey") 
      == std::vector<std::string>{"hello", "world"});
}

TEST_CASE("GRPC query copying", "[engine]") {
  qq::SearchRequest grpc_query;
  grpc_query.set_n_results(3);
  grpc_query.set_n_snippet_passages(5);
  grpc_query.add_terms("hello");
  grpc_query.add_terms("world");

  SearchQuery local_query;
  local_query.CopyFrom(grpc_query);

  REQUIRE(local_query.terms == TermList{"hello", "world"});
  REQUIRE(local_query.n_results == 3);
  REQUIRE(local_query.n_snippet_passages == 5);

  qq::SearchRequest new_grpc_query;
  local_query.CopyTo(&new_grpc_query);

  REQUIRE(new_grpc_query.terms(0) == "hello");
  REQUIRE(new_grpc_query.terms(1) == "world");
  REQUIRE(new_grpc_query.n_results() == 3);
  REQUIRE(new_grpc_query.n_snippet_passages() == 5);
}


TEST_CASE( "Histogram basic operations are fine", "[histogram]" ) {
  Histogram hist;

  for (int i = 0; i < 10; i++) {
    hist.Add(i);
  }

  std::cout << "Count: " << hist.Count() << std::endl;
  REQUIRE(hist.Count() == 10);
  REQUIRE(hist.Percentile(0) == 0);
  REQUIRE(hist.Percentile(100) == 9);

  for (int i = 0; i < 101; i += 10) {
    LOG(INFO) << ((double)i) << " :: " << hist.Percentile((double)i) << std::endl;  
  }
}



TEST_CASE( "Time operations are accurate", "[time][slow]" ) {
  SECTION("utils::duration() returned value is in seconds") {
    auto start = utils::now();    
    utils::sleep(1);
    auto end = utils::now();    
    auto duration = utils::duration(start, end);
    REQUIRE(round(duration) == 1);
  }
}


TEST_CASE( "Query pool work", "[aux]" ) {
  SECTION("TermPool") {
    TermPool pool;
    pool.Add(TermList{"hello"});
    pool.Add(TermList{"obama"});

    REQUIRE(pool.Next() == TermList{"hello"});
    REQUIRE(pool.Next() == TermList{"obama"});
    REQUIRE(pool.Next() == TermList{"hello"});
    REQUIRE(pool.Next() == TermList{"obama"});
  }

  SECTION("TermPoolArray") {
    TermPoolArray array(2);

    REQUIRE(array.Size() == 2);

    array.Add(0, TermList{"hello"});
    array.Add(1, TermList{"obama"});

    REQUIRE(array.Next(0) == TermList{"hello"});
    REQUIRE(array.Next(0) == TermList{"hello"});
    REQUIRE(array.Next(1) == TermList{"obama"});
    REQUIRE(array.Next(1) == TermList{"obama"});
  }

  SECTION("QueryLogReader") {
    QueryLogReader reader("src/testdata/query-log-sample.txt");
    
    TermList query;
    bool ret;

    ret = reader.NextQuery(query);
    REQUIRE(ret == true);
    REQUIRE(query == TermList{"hello", "world"});

    ret = reader.NextQuery(query);
    REQUIRE(ret == true);
    REQUIRE(query == TermList{"barack", "obama"});

    ret = reader.NextQuery(query);
    REQUIRE(ret == false);
  }

  SECTION("Loading query pool") {
    TermPoolArray array(2);

    array.LoadFromFile("src/testdata/query-log-sample.txt", 0);
    REQUIRE(array.Next(0) == TermList{"hello", "world"});
    REQUIRE(array.Next(0) == TermList{"hello", "world"});
    REQUIRE(array.Next(1) == TermList{"barack", "obama"});
    REQUIRE(array.Next(1) == TermList{"barack", "obama"});
  }

  SECTION("TermPoolArray factory") {
    auto array = CreateTermPoolArray(TermList{"hello"}, 2);
    REQUIRE(array->Size() == 2);
    REQUIRE(array->Next(0) == TermList{"hello"});
    REQUIRE(array->Next(1) == TermList{"hello"});
  }
}

TEST_CASE( "SearchResult copying works", "[engine]" ) {
  SECTION("SearchResult") {
    SearchResult result;
    SearchResultEntry entry;

    entry.doc_id = 10;
    entry.snippet = "my snippet 001";
    result.entries.push_back(entry);

    qq::SearchReply reply;
    result.CopyTo(&reply);

    REQUIRE(reply.entries_size() == 1);
    REQUIRE(reply.entries(0).doc_id() == 10);
    REQUIRE(reply.entries(0).snippet() == "my snippet 001");

    result.CopyTo(&reply);

    REQUIRE(reply.entries_size() == 1);
    REQUIRE(reply.entries(0).doc_id() == 10);
    REQUIRE(reply.entries(0).snippet() == "my snippet 001");
  }
}




