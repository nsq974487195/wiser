// #define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <iostream>
#include <iomanip>
#include <set>

#include <boost/filesystem.hpp>
#include <boost/lambda/lambda.hpp>    
#include "catch.hpp"
#include <glog/logging.h>

#include "native_doc_store.h"
#include "inverted_index.h"
#include "qq_engine.h"
#include "index_creator.h"
#include "utils.h"
#include "hash_benchmark.h"
#include "grpc_server_impl.h"
#include "qq_mem_uncompressed_engine.h"

#include "posting_list_direct.h"
#include "posting_list_raw.h"
#include "posting_list_protobuf.h"
#include "posting_list_vec.h"

#include "intersect.h"
#include "ranking.h"

#include "unifiedhighlighter.h"

unsigned int Factorial( unsigned int number ) {
    return number <= 1 ? number : Factorial(number-1)*number;
}

TEST_CASE( "glog can print", "[glog]" ) {
  LOG(ERROR) << "Found " << 4 << " cookies in GLOG";
}


TEST_CASE( "Factorials are computed", "[factorial]" ) {
    REQUIRE( Factorial(1) == 1 );
    REQUIRE( Factorial(2) == 2 );
    REQUIRE( Factorial(3) == 6 );
    REQUIRE( Factorial(10) == 3628800 );
}

TEST_CASE( "Document store implemented by C++ map", "[docstore]" ) {
    NativeDocStore store;
    int doc_id = 88;
    std::string doc = "it is a doc";

    REQUIRE(store.Has(doc_id) == false);

    store.Add(doc_id, doc);
    REQUIRE(store.Get(doc_id) == doc);

    store.Add(89, "doc89");
    REQUIRE(store.Size() == 2);

    store.Remove(89);
    REQUIRE(store.Size() == 1);

    REQUIRE(store.Has(doc_id) == true);

    store.Clear();
    REQUIRE(store.Has(doc_id) == false);
}

TEST_CASE( "Inverted Index essential operations are OK", "[inverted_index]" ) {
    InvertedIndex index;     
    index.AddDocument(100, TermList{"hello", "world"});
    index.AddDocument(101, TermList{"hello", "earth"});

    // Get just the iterator
    {
      IndexStore::const_iterator ci = index.Find(Term("hello"));
      REQUIRE(index.ConstEnd() != ci);

      ci = index.Find(Term("hellox"));
      REQUIRE(index.ConstEnd() == ci);
    }

    // Get non-exist
    {
        std::vector<int> ids = index.GetDocumentIds("notexist");
        REQUIRE(ids.size() == 0);
    }

    // Get existing
    {
        std::vector<int> ids = index.GetDocumentIds("hello");
        REQUIRE(ids.size() == 2);
        
        std::set<int> s1(ids.begin(), ids.end());
        REQUIRE(s1 == std::set<int>{100, 101});
    }

    // Search "hello"
    {
        auto doc_ids = index.Search(TermList{"hello"}, SearchOperator::AND);
        REQUIRE(doc_ids.size() == 2);

        std::set<int> s(doc_ids.begin(), doc_ids.end());
        REQUIRE(s == std::set<int>{100, 101});
    }

    // Search "world"
    {
        auto doc_ids = index.Search(TermList{"earth"}, SearchOperator::AND);
        REQUIRE(doc_ids.size() == 1);

        std::set<int> s(doc_ids.begin(), doc_ids.end());
        REQUIRE(s == std::set<int>{101});
    }

    // Search non-existing
    {
        auto doc_ids = index.Search(TermList{"nonexist"}, SearchOperator::AND);
        REQUIRE(doc_ids.size() == 0);
    }
}

TEST_CASE( "Basic Posting", "[posting_list]" ) {
    Posting posting(100, 200, Positions {1, 2});
    REQUIRE(posting.docID_ == 100);
    REQUIRE(posting.term_frequency_ == 200);
    REQUIRE(posting.positions_[0] == 1);
    REQUIRE(posting.positions_[1] == 2);
}

TEST_CASE( "Direct Posting List essential operations are OK", "[posting_list_direct]" ) {
    PostingList_Direct pl("term001");
    REQUIRE(pl.Size() == 0);

    pl.AddPosting(111, 1,  Positions {19});
    pl.AddPosting(232, 2,  Positions {10, 19});

    REQUIRE(pl.Size() == 2);

    for (auto it = pl.cbegin(); it != pl.cend(); it++) {
        auto p = pl.ExtractPosting(it); 
        REQUIRE((p.docID_ == 111 || p.docID_ == 232));
        if (p.docID_ == 111) {
            REQUIRE(p.term_frequency_ == 1);
            REQUIRE(p.positions_.size() == 1);
            REQUIRE(p.positions_[0] == 19);
        } else {
            // p.docID_ == 232
            REQUIRE(p.term_frequency_ == 2);
            REQUIRE(p.positions_.size() == 2);
            REQUIRE(p.positions_[0] == 10);
            REQUIRE(p.positions_[1] == 19);
        }
    }
}

TEST_CASE( "Raw String based Posting List essential operations are OK", "[posting_list_raw]" ) {
    PostingList_Raw pl("term001");
    REQUIRE(pl.Size() == 0);

    pl.AddPosting(111, 1,  Positions {19});
    pl.AddPosting(232, 2,  Positions {10, 19});

    REQUIRE(pl.Size() == 2);

    std::string serialized = pl.Serialize();
    PostingList_Raw pl1("term001", serialized);
    Posting p1 = pl1.GetPosting();
    REQUIRE(p1.docID_ == 111);
    REQUIRE(p1.term_frequency_ == 1);
    REQUIRE(p1.positions_.size() == 1);
    
    Posting p2 = pl1.GetPosting();
    REQUIRE(p2.docID_ == 232);
    REQUIRE(p2.term_frequency_ == 2);
    REQUIRE(p2.positions_.size() == 2);
}


TEST_CASE( "Protobuf based Posting List essential operations are OK", "[posting_list_protobuf]" ) {
    PostingList_Protobuf pl("term001");
    REQUIRE(pl.Size() == 0);

    pl.AddPosting(111, 1,  Positions {19});
    pl.AddPosting(232, 2,  Positions {10, 19});

    REQUIRE(pl.Size() == 2);

    std::string serialized = pl.Serialize();
    PostingList_Protobuf pl1("term001", serialized);
    Posting p1 = pl1.GetPosting();
    REQUIRE(p1.docID_ == 111);
    REQUIRE(p1.term_frequency_ == 1);
    REQUIRE(p1.positions_.size() == 1);
    
    Posting p2 = pl1.GetPosting();
    REQUIRE(p2.docID_ == 232);
    REQUIRE(p2.term_frequency_ == 2);
    REQUIRE(p2.positions_.size() == 2);
}

TEST_CASE( "QQSearchEngine", "[engine]" ) {
    QQSearchEngine engine;

    REQUIRE(engine.NextDocId() == 0);

    engine.AddDocument("my title", "my url", "my body");

    std::vector<int> doc_ids = engine.Search(TermList{"my"}, SearchOperator::AND);
    REQUIRE(doc_ids.size() == 1);

    std::string doc = engine.GetDocument(doc_ids[0]);
    REQUIRE(doc == "my body");
}


TEST_CASE( "QQSearchEngine can Find() a term", "[engine, benchmark]" ) {
    QQSearchEngine engine;

    engine.AddDocument("my title", "my url", "my body");

    InvertedIndex::const_iterator it = engine.Find(Term("my"));

    // We do not do any assertion here. 
    // Just want to make sure it runs. 
}



TEST_CASE( "Utilities", "[utils]" ) {
    SECTION("Leading space and Two spaces") {
        std::vector<std::string> vec = utils::explode(" hello  world", ' ');
        REQUIRE(vec.size() == 2);
        REQUIRE(vec[0] == "hello");
        REQUIRE(vec[1] == "world");
    }

    SECTION("Empty string") {
        std::vector<std::string> vec = utils::explode("", ' ');
        REQUIRE(vec.size() == 0);
    }

    SECTION("All spaces") {
        std::vector<std::string> vec = utils::explode("   ", ' ');
        REQUIRE(vec.size() == 0);
    }

    SECTION("Explode by some commone spaces") {
        std::vector<std::string> vec = utils::explode_by_non_letter(" hello\t world yeah");
        REQUIRE(vec.size() == 3);
        REQUIRE(vec[0] == "hello");
        REQUIRE(vec[1] == "world");
        REQUIRE(vec[2] == "yeah");
    }
}

TEST_CASE("Strict spliting", "[utils]") {
    SECTION("Regular case") {
        std::vector<std::string> vec = utils::explode_strict("a\tb", '\t');
        REQUIRE(vec.size() == 2);
        REQUIRE(vec[0] == "a");
        REQUIRE(vec[1] == "b");
    }

    SECTION("Separator at beginning and end of line") {
        std::vector<std::string> vec = utils::explode_strict("\ta\tb\t", '\t');

        REQUIRE(vec.size() == 4);
        REQUIRE(vec[0] == "");
        REQUIRE(vec[1] == "a");
        REQUIRE(vec[2] == "b");
        REQUIRE(vec[3] == "");
    }
}

TEST_CASE("Filling zeros", "[utils]") {
    REQUIRE(utils::fill_zeros("abc", 5) == "00abc");
    REQUIRE(utils::fill_zeros("abc", 0) == "abc");
    REQUIRE(utils::fill_zeros("abc", 3) == "abc");
    REQUIRE(utils::fill_zeros("", 3) == "000");
    REQUIRE(utils::fill_zeros("", 0) == "");
}

TEST_CASE( "LineDoc", "[line_doc]" ) {
    SECTION("Small size") {
        utils::LineDoc linedoc("src/testdata/tokenized_wiki_abstract_line_doc");
        std::vector<std::string> items;

        auto ret = linedoc.GetRow(items); 
        REQUIRE(ret == true);
        REQUIRE(items[0] == "col1");
        REQUIRE(items[1] == "col2");
        REQUIRE(items[2] == "col3");
        // for (auto s : items) {
            // std::cout << "-------------------" << std::endl;
            // std::cout << s << std::endl;
        // }
        
        ret = linedoc.GetRow(items);
        REQUIRE(ret == true);
        REQUIRE(items[0] == "Wikipedia: Anarchism");
        REQUIRE(items[1] == "Anarchism is a political philosophy that advocates self-governed societies based on voluntary institutions. These are often described as stateless societies,\"ANARCHISM, a social philosophy that rejects authoritarian government and maintains that voluntary institutions are best suited to express man's natural social tendencies.");
        REQUIRE(items[2] == "anarch polit philosophi advoc self govern societi base voluntari institut often describ stateless social reject authoritarian maintain best suit express man natur tendenc ");

        ret = linedoc.GetRow(items);
        REQUIRE(ret == false);
    }

    SECTION("Large size") {
        utils::LineDoc linedoc("src/testdata/test_doc_tokenized");
        std::vector<std::string> items;

        while (true) {
            std::vector<std::string> items;
            bool has_it = linedoc.GetRow(items);
            if (has_it) {
                REQUIRE(items.size() == 3);    
            } else {
                break;
            }
        }
    }
}

TEST_CASE( "boost library is usable", "[boost]" ) {
    using namespace boost::filesystem;

    path p{"./somefile-not-exist"};
    try
    {
        file_status s = status(p);
        REQUIRE(is_directory(s) == false);
        REQUIRE(exists(s) == false);
    }
    catch (filesystem_error &e)
    {
        std::cerr << e.what() << '\n';
    }
}

TEST_CASE( "Hash benchmark", "[benchmark]" ) {
    STLHash stl_hash;

    for (std::size_t i = 0; i < 100; i++) {
        stl_hash.Put(std::to_string(i), std::to_string(i * 100));
    }

    for (std::size_t i = 0; i < 100; i++) {
        std::string v = stl_hash.Get(std::to_string(i));
        REQUIRE(v == std::to_string(i * 100)); 
    }

    REQUIRE(stl_hash.Size() == 100);

}

TEST_CASE( "GRPC Sync Client and Server", "[grpc]" ) {
  auto server = CreateServer(std::string("localhost:50051"), 1, 1, 0);
  utils::sleep(1); // warm up the server

  auto client = CreateSyncClient("localhost:50051");

  SECTION("Echo") {
    EchoData request;
    request.set_message("hello");
    EchoData reply;
    auto ret = client->Echo(request, reply);

    REQUIRE(ret == true);
    REQUIRE(reply.message() == "hello");
    utils::sleep(1);

    // server will automatically destructed here.
  }

  SECTION("Add documents and search") {
    std::vector<int> doc_ids;
    bool ret;

    ret = client->Search("body", doc_ids);
    REQUIRE(doc_ids.size() == 0);
    REQUIRE(ret == true);

    client->AddDocument("my title", "my url", "my body");
    client->AddDocument("my title", "my url", "my spirit");

    doc_ids.clear();
    client->Search("body", doc_ids);
    REQUIRE(ret == true);
    REQUIRE(doc_ids.size() == 1);

    // server will automatically destructed here.
  }

}

void run_client() {
  auto client = CreateAsyncClient("localhost:50051", 64, 100, 1000, 8, 1, 8);
  client->Wait();
}

TEST_CASE( "GRPC Async Client and Server", "[grpc]" ) {
  auto server = CreateServer(std::string("localhost:50051"), 1, 40, 0);
  utils::sleep(1); // warm up the server

  // Use another thread to create client
  // std::thread client_thread(run_client);
  auto client = CreateAsyncClient("localhost:50051", 64, 100, 1000, 8, 1, 2);
  client->Wait();
  // client->ShowStats();
  client.release();

  // client_thread.join();
}

TEST_CASE( "IndexCreator works over the network", "[grpc]" ) {
  auto server = CreateServer(std::string("localhost:50051"), 1, 40, 0);
  utils::sleep(1); // warm up the server

  auto client = CreateSyncClient("localhost:50051");

  IndexCreator index_creator(
        "src/testdata/enwiki-abstract_tokenized.linedoc.sample", *client);
  index_creator.DoIndex();

  // Search synchroniously
  std::vector<int> doc_ids;
  bool ret;
  ret = client->Search("multicellular", doc_ids);
  REQUIRE(ret == true);
  REQUIRE(doc_ids.size() == 1);

  // client_thread.join();
}


TEST_CASE( "Search engine can load document and index them locally", "[engine]" ) {
  QQSearchEngine engine;
  int ret = engine.LoadLocalDocuments("src/testdata/enwiki-abstract_tokenized.linedoc.sample", 90);
  REQUIRE(ret == 90);

  std::vector<int> doc_ids = engine.Search(TermList{"multicellular"}, SearchOperator::AND);
  REQUIRE(doc_ids.size() == 1);
}

TEST_CASE( "Unified Highlighter essential operations are OK", "[unified_highlighter]" ) {
    QQSearchEngine engine;
    engine.AddDocument("my title", "my url", "hello world. This is Kan ...    Hello Kan, This is Madison!");
    /*engine.AddDocument("my title", "my url", "hello earth\n This is Kan! hello world. This is Kan!");
    engine.AddDocument("my title", "my url", "hello Madison.... This is Kan. hello world. This is Kan!");
    engine.AddDocument("my title", "my url", "hello Wisconsin, This is Kan. Im happy. hello world. This is Kan!");
*/
    UnifiedHighlighter test_highlighter(engine);
    Query query = {"hello", "kan"};
    TopDocs topDocs = {0}; 

    int maxPassages = 3;
    std::vector<std::string> res = test_highlighter.highlight(query, topDocs, maxPassages);
    REQUIRE(res.size()  == topDocs.size());
    /*REQUIRE(res[0] == "hello world");
    REQUIRE(res[1] == "hello earth");
    REQUIRE(res[2] == "hello Wisconsin");
    */
}

TEST_CASE( "SentenceBreakIterator essential operations are OK", "[sentence_breakiterator]" ) {

    // init 
    std::string test_string = "hello Wisconsin, This is Kan.  Im happy.";
    int breakpoint[2] = {30, 39};
    SentenceBreakIterator breakiterator(test_string);

    // iterate on a string
    int i = 0;
    while ( breakiterator.next() > 0 ) {
        
        int start_offset = breakiterator.getStartOffset();
        int end_offset = breakiterator.getEndOffset();
        REQUIRE(end_offset == breakpoint[i++]);
    }   

    // test offset-based next
    REQUIRE(i == 2);
    breakiterator.next(3);
    REQUIRE(breakiterator.getEndOffset() == 30);
    breakiterator.next(33);
    REQUIRE(breakiterator.getEndOffset() == 39);
    REQUIRE(breakiterator.next(40) == 0);
}


TEST_CASE( "Vector-based posting list works fine", "[posting_list]" ) {
  SECTION("New postings can be added and retrieved") {
    PostingList_Vec<Posting> pl("hello");   

    REQUIRE(pl.Size() == 0);
    pl.AddPosting(Posting(10, 88, Positions{28}));
    REQUIRE(pl.Size() == 1);
    REQUIRE(pl.GetPosting(0).GetDocId() == 10);
    REQUIRE(pl.GetPosting(0).GetTermFreq() == 88);
    REQUIRE(pl.GetPosting(0).GetPositions() == Positions{28});

    pl.AddPosting(Posting(11, 889, Positions{28, 230}));
    REQUIRE(pl.Size() == 2);
    REQUIRE(pl.GetPosting(1).GetDocId() == 11);
    REQUIRE(pl.GetPosting(1).GetTermFreq() == 889);
    REQUIRE(pl.GetPosting(1).GetPositions() == Positions{28, 230});
  }


  SECTION("Skipping works") {
    PostingList_Vec<Posting> pl("hello");   
    for (int i = 0; i < 100; i++) {
      pl.AddPosting(Posting(i, 1, Positions{28}));
    }
    REQUIRE(pl.Size() == 100);

    SECTION("It can stay at starting it") {
      PostingList_Vec<Posting>::iterator_t it;
      it = pl.SkipForward(0, 0);
      REQUIRE(it == 0);

      it = pl.SkipForward(8, 8);
      REQUIRE(it == 8);
    }

    SECTION("It can skip multiple postings") {
      PostingList_Vec<Posting>::iterator_t it;
      it = pl.SkipForward(0, 8);
      REQUIRE(it == 8);

      it = pl.SkipForward(0, 99);
      REQUIRE(it == 99);
    }

    SECTION("It returns pl.Size() when we cannot find doc id") {
      PostingList_Vec<Posting>::iterator_t it;
      it = pl.SkipForward(0, 1000);
      REQUIRE(it == pl.Size());
    }
  }

}


TEST_CASE( "Intersection", "[intersect]" ) {
  PostingList_Vec<Posting> pl01("hello");   
  for (int i = 0; i < 10; i++) {
    pl01.AddPosting(Posting(i, 1, Positions{28}));
  }

  SECTION("It intersects a subset") {
    PostingList_Vec<Posting> pl02("world");   
    for (int i = 5; i < 10; i++) {
      pl02.AddPosting(Posting(i, 1, Positions{28}));
    }

    std::vector<const PostingList_Vec<Posting>*> lists{&pl01, &pl02};
    std::vector<DocIdType> ret = intersect<Posting>(lists);
    REQUIRE(ret == std::vector<DocIdType>{5, 6, 7, 8, 9});
  }

  SECTION("It intersects an empty posting list") {
    PostingList_Vec<Posting> pl02("world");   

    std::vector<const PostingList_Vec<Posting>*> lists{&pl01, &pl02};
    std::vector<DocIdType> ret = intersect<Posting>(lists);
    REQUIRE(ret == std::vector<DocIdType>{});
  }

  SECTION("It intersects a non-overlapping posting list") {
    PostingList_Vec<Posting> pl02("world");   
    for (int i = 10; i < 20; i++) {
      pl02.AddPosting(Posting(i, 1, Positions{28}));
    }

    std::vector<const PostingList_Vec<Posting>*> lists{&pl01, &pl02};
    std::vector<DocIdType> ret = intersect<Posting>(lists);
    REQUIRE(ret == std::vector<DocIdType>{});
  }

  SECTION("It intersects a partial-overlapping posting list") {
    PostingList_Vec<Posting> pl02("world");   
    for (int i = 5; i < 15; i++) {
      pl02.AddPosting(Posting(i, 1, Positions{28}));
    }

    std::vector<const PostingList_Vec<Posting>*> lists{&pl01, &pl02};
    std::vector<DocIdType> ret = intersect<Posting>(lists);
    REQUIRE(ret == std::vector<DocIdType>{5, 6, 7, 8, 9});
  }

  SECTION("It intersects a super set") {
    PostingList_Vec<Posting> pl02("world");   
    for (int i = 0; i < 15; i++) {
      pl02.AddPosting(Posting(i, 1, Positions{28}));
    }

    std::vector<const PostingList_Vec<Posting>*> lists{&pl01, &pl02};
    std::vector<DocIdType> ret = intersect<Posting>(lists);
    REQUIRE(ret == std::vector<DocIdType>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
  }

  SECTION("It intersects a single list") {
    std::vector<const PostingList_Vec<Posting>*> lists{&pl01};
    std::vector<DocIdType> ret = intersect<Posting>(lists);
    REQUIRE(ret == std::vector<DocIdType>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
  }

  SECTION("It intersects three lists") {
    PostingList_Vec<Posting> pl02("world");   
    for (int i = 0; i < 5; i++) {
      pl02.AddPosting(Posting(i, 1, Positions{28}));
    }

    PostingList_Vec<Posting> pl03("more");   
    for (int i = 0; i < 2; i++) {
      pl03.AddPosting(Posting(i, 1, Positions{28}));
    }

    std::vector<const PostingList_Vec<Posting>*> lists{&pl01, &pl02, &pl03};
    std::vector<DocIdType> ret = intersect<Posting>(lists);
    REQUIRE(ret == std::vector<DocIdType>{0, 1});
  }

  SECTION("It intersects two empty list") {
    PostingList_Vec<Posting> pl01("hello");   
    PostingList_Vec<Posting> pl02("world");   

    std::vector<const PostingList_Vec<Posting>*> lists{&pl01, &pl02};
    std::vector<DocIdType> ret = intersect<Posting>(lists);
    REQUIRE(ret == std::vector<DocIdType>{});
  }

  SECTION("It handles a previous bug") {
    PostingList_Vec<Posting> pl01("hello");   
    pl01.AddPosting(Posting(8, 1, Positions{28}));
    pl01.AddPosting(Posting(10, 1, Positions{28}));

    PostingList_Vec<Posting> pl02("world");   
    pl02.AddPosting(Posting(7, 1, Positions{28}));
    pl02.AddPosting(Posting(10, 1, Positions{28}));
    pl02.AddPosting(Posting(15, 1, Positions{28}));

    std::vector<const PostingList_Vec<Posting>*> lists{&pl01, &pl02};
    std::vector<DocIdType> ret = intersect<Posting>(lists);
    REQUIRE(ret == std::vector<DocIdType>{10});
  }


  SECTION("It returns doc counts of each term") {
    PostingList_Vec<Posting> pl02("world");   
    for (int i = 0; i < 15; i++) {
      pl02.AddPosting(Posting(i, 1, Positions{28}));
    }

    std::vector<const PostingList_Vec<Posting>*> lists{&pl01, &pl02};
    TfIdfStore tfidf_store;

    std::vector<DocIdType> ret = intersect<Posting>(lists, &tfidf_store);
    REQUIRE(ret == std::vector<DocIdType>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    REQUIRE(tfidf_store.GetDocCount("hello") == 10);
    REQUIRE(tfidf_store.GetDocCount("world") == 15);
  }

  SECTION("It returns term freqs") {
    PostingList_Vec<RankingPosting> pl01("hello");   
    pl01.AddPosting(RankingPosting(8, 1));
    pl01.AddPosting(RankingPosting(10, 2));

    PostingList_Vec<RankingPosting> pl02("world");   
    pl02.AddPosting(RankingPosting(7, 1));
    pl02.AddPosting(RankingPosting(10, 8));
    pl02.AddPosting(RankingPosting(15, 1));

    std::vector<const PostingList_Vec<RankingPosting>*> lists{&pl01, &pl02};
    TfIdfStore tfidf_store;

    std::vector<DocIdType> ret = intersect<RankingPosting>(lists, &tfidf_store);

    REQUIRE(ret == std::vector<DocIdType>{10});
    REQUIRE(tfidf_store.GetTf(10, "hello") == 2);
    REQUIRE(tfidf_store.GetTf(10, "world") == 8);
  }

}

TEST_CASE( "DocLengthStore", "[ranking]" ) {
  SECTION("It can add some lengths.") {
    DocLengthStore store;
    store.AddLength(1, 7);
    store.AddLength(2, 8);
    store.AddLength(3, 9);

    REQUIRE(store.Size() == 3);
    REQUIRE(store.GetAvgLength() == 8);
  }
}


TEST_CASE( "Scoring", "[ranking]" ) {
  SECTION("TF is correct") {
    REQUIRE(calc_tf(1) == 1.0); // sample in ES document
    REQUIRE(calc_tf(4) == 2.0);
    REQUIRE(calc_tf(100) == 10.0);

    REQUIRE(utils::format_double(calc_tf(2), 3) == "1.41");
  }

  SECTION("IDF is correct (sample score in ES document)") {
    REQUIRE(utils::format_double(calc_idf(1, 1), 3) == "0.307");
  }


  SECTION("Field length norm is correct") {
    REQUIRE(calc_field_len_norm(4) == 0.5);
  }

  SECTION("ElasticSearch IDF") {
    REQUIRE(utils::format_double(calc_es_idf(1, 1), 3) == "0.288"); // From an ES run
  }

  SECTION("ElasticSearch IDF 2") {
    REQUIRE(utils::format_double(calc_es_idf(3, 1), 3)== "0.981"); // From an ES run
  }

  SECTION("ElasticSearch TF NORM") {
    REQUIRE(calc_es_tfnorm(1, 3, 3.0) == 1.0); // From an ES run
    REQUIRE(calc_es_tfnorm(1, 7, 7.0) == 1.0); // From an ES run
    REQUIRE( utils::format_double(calc_es_tfnorm(1, 2, 8/3.0), 3) == "1.11"); // From an ES run
  }
}



TEST_CASE( "TfIdfStore works", "[TfIdfStore]" ) {
  TfIdfStore table;

  SECTION("It sets and gets IDF") {
    table.SetDocCount("term1", 10);
    REQUIRE(table.GetDocCount("term1") == 10);
  }

  SECTION("It sets and gets TF") {
    table.SetTf(100, "term1", 2);
    REQUIRE(table.GetTf(100, "term1") == 2);
    REQUIRE(table.Size() == 1);

    table.SetDocCount("wisconsin", 1);

    SECTION("ToStr() works") {
      auto str = table.ToStr();
      REQUIRE(str == "DocId (100) : term1(2)\nDocument count for a term\nwisconsin(1)\n");
    }
  }

  SECTION("Iterator works") {
    table.SetTf(10, "term1", 2);
    table.SetTf(10, "term2", 3);
    table.SetTf(20, "term1", 8);
    table.SetTf(20, "term2", 8);

    REQUIRE(table.Size() == 2);

    SECTION("Doc iterators work") {
      TfIdfStore::row_iterator it = table.row_cbegin();
      REQUIRE(table.GetCurDocId(it) == 10);
      it++;
      REQUIRE(table.GetCurDocId(it) == 20);
      it++;
      REQUIRE(it == table.row_cend());
    }

    SECTION("Term iterators work") {
      TfIdfStore::row_iterator it = table.row_cbegin();
      TfIdfStore::col_iterator term_it = table.col_cbegin(it);
      REQUIRE(table.GetCurTerm(term_it) == "term1");
      REQUIRE(table.GetCurTermFreq(term_it) == 2);
      term_it++;
      REQUIRE(table.GetCurTerm(term_it) == "term2");
      REQUIRE(table.GetCurTermFreq(term_it) == 3);
      term_it++;
      REQUIRE(term_it == table.col_cend(it));
    }
  }
}


TEST_CASE( "We can get score for each document", "[ranking]" ) {
  SECTION("It gets the same score as ES, for one term") {
    TfIdfStore table;
    table.SetTf(0, "term1", 1);

    table.SetDocCount("term1", 1);

    TfIdfStore::row_iterator it = table.row_cbegin();

    TermScoreMap term_scores = score_terms_in_doc(table, it, 3, 3, 1);

    REQUIRE(utils::format_double(term_scores["term1"], 3) == "0.288"); // From an ES run
  }

  SECTION("It gets the same score as ES, for two terms") {
    TfIdfStore table;
    table.SetTf(0, "term1", 1);
    table.SetTf(0, "term2", 1);

    table.SetDocCount("term1", 1);
    table.SetDocCount("term2", 1);

    TfIdfStore::row_iterator it = table.row_cbegin();

    TermScoreMap term_scores = score_terms_in_doc(table, it, 7, 7, 1);

    REQUIRE(utils::format_double(term_scores["term1"], 3) == "0.288"); // From an ES run
    REQUIRE(utils::format_double(term_scores["term2"], 3) == "0.288"); // From an ES run
  }
}

TEST_CASE( "Utilities work", "[utils]" ) {
  SECTION("Count terms") {
    REQUIRE(count_terms("hello world") == 2);
  }

  SECTION("Count tokens") {
    CountMapType counts = count_tokens("hello hello you");
    assert(counts["hello"] == 2);
    assert(counts["you"] == 1);
    assert(counts.size() == 2);
  }
}

TEST_CASE( "Inverted index used by QQ memory uncompressed works", "[engine]" ) {
  InvertedIndexQqMem inverted_index;

  inverted_index.AddDocument(0, "hello world", "hello world");
  inverted_index.AddDocument(1, "hello wisconsin", "hello wisconsin");
  REQUIRE(inverted_index.Size() == 3);

  SECTION("It can find an intersection for single-term queries") {
    TfIdfStore store = inverted_index.FindIntersection(TermList{"hello"});
    REQUIRE(store.Size() == 2);
    auto it = store.row_cbegin();
    REQUIRE(TfIdfStore::GetCurDocId(it) == 0);
    it++;
    REQUIRE(TfIdfStore::GetCurDocId(it) == 1);
  }

  SECTION("It can find an intersection for two-term queries") {
    TfIdfStore store = inverted_index.FindIntersection(TermList{"hello", "world"});
    REQUIRE(store.Size() == 1);
    auto it = store.row_cbegin();
    REQUIRE(TfIdfStore::GetCurDocId(it) == 0);
  }
}


TEST_CASE( "QQ Mem Uncompressed Engine works", "[engine]" ) {
  QqMemUncompressedEngine engine;

  auto doc_id = engine.AddDocument("hello world", "hello world");
  REQUIRE(engine.GetDocument(doc_id) == "hello world");
  REQUIRE(engine.GetDocLength(doc_id) == 2);
  REQUIRE(engine.TermCount() == 2);

  doc_id = engine.AddDocument("hello wisconsin", "hello wisconsin");
  REQUIRE(engine.GetDocument(doc_id) == "hello wisconsin");
  REQUIRE(engine.GetDocLength(doc_id) == 2);
  REQUIRE(engine.TermCount() == 3);

  doc_id = engine.AddDocument("hello world big world", "hello world big world");
  REQUIRE(engine.GetDocument(doc_id) == "hello world big world");
  REQUIRE(engine.GetDocLength(doc_id) == 4);
  REQUIRE(engine.TermCount() == 4);

  SECTION("The engine can serve single-term queries") {
    TfIdfStore tfidf_store = engine.Query(TermList{"wisconsin"}); 
    REQUIRE(tfidf_store.Size() == 1);
    std::cout << tfidf_store.ToStr();

    DocScoreVec doc_scores = engine.Score(tfidf_store);
    REQUIRE(doc_scores.size() == 1);
    REQUIRE(doc_scores[0].doc_id == 1);
    REQUIRE(utils::format_double(doc_scores[0].score, 3) == "1.09");
  }

  SECTION("The engine can serve single-term queries with multiple results") {
    TfIdfStore tfidf_store = engine.Query(TermList{"hello"}); 
    REQUIRE(tfidf_store.Size() == 3);

    DocScoreVec doc_scores = engine.Score(tfidf_store);
    REQUIRE(doc_scores.size() == 3);

    // The score below is produced by ../tools/es_index_docs.py in this
    // same git commit
    // You can reproduce the elasticsearch score by checking out this
    // commit and run `python tools/es_index_docs.py`.
    REQUIRE(utils::format_double(doc_scores[0].score, 3) == "0.149");
    REQUIRE(utils::format_double(doc_scores[1].score, 3) == "0.149");
    REQUIRE(utils::format_double(doc_scores[2].score, 3) == "0.111");
  }

  SECTION("The engine can server two-term queries") {
    TfIdfStore tfidf_store = engine.Query(TermList{"hello", "world"}); 
    REQUIRE(tfidf_store.Size() == 2);
    auto it = tfidf_store.row_cbegin();
    REQUIRE(TfIdfStore::GetCurDocId(it) == 0);
    it++;
    REQUIRE(TfIdfStore::GetCurDocId(it) == 2);

    DocScoreVec doc_scores = engine.Score(tfidf_store);
    REQUIRE(doc_scores.size() == 2);

    for (auto score : doc_scores) {
      std::cout << score.ToStr() ;
    }

    // The scores below are produced by ../tools/es_index_docs.py in this
    // same git commit
    // You can reproduce the elasticsearch scores by checking out this
    // commit and run `python tools/es_index_docs.py`.
    REQUIRE(utils::format_double(doc_scores[0].score, 3) == "0.672");
    REQUIRE(utils::format_double(doc_scores[1].score, 3) == "0.677");

    std::vector<DocIdType> doc_ids = engine.FindTopK(doc_scores, 10);
    REQUIRE(doc_ids.size() == 2);
  }
}


TEST_CASE( "Sorting document works", "[ranking]" ) {
  SECTION("A regular case") {
    DocScoreVec scores{{0, 0.8}, {1, 3.0}, {2, 2.1}};    
    REQUIRE(utils::find_top_k(scores, 4) == std::vector<DocIdType>{1, 2, 0});
    REQUIRE(utils::find_top_k(scores, 3) == std::vector<DocIdType>{1, 2, 0});
    REQUIRE(utils::find_top_k(scores, 2) == std::vector<DocIdType>{1, 2});
    REQUIRE(utils::find_top_k(scores, 1) == std::vector<DocIdType>{1});
    REQUIRE(utils::find_top_k(scores, 0) == std::vector<DocIdType>{});
  }

  SECTION("Empty scores") {
    DocScoreVec scores{};    
    REQUIRE(utils::find_top_k(scores, 4) == std::vector<DocIdType>{});
  }

  SECTION("Identical scores") {
    DocScoreVec scores{{0, 0.8}, {1, 0.8}, {2, 2.1}};    
    REQUIRE(utils::find_top_k(scores, 1) == std::vector<DocIdType>{2});
  }
}


TEST_CASE( "Class Staircase can generate staircase strings", "[utils]" ) {
  SECTION("Simple case") {
    utils::Staircase staircase(1, 1, 2);
    
    REQUIRE(staircase.MaxWidth() == 2);

    REQUIRE(staircase.NextLayer() == "0");
    REQUIRE(staircase.NextLayer() == "0 1");
    REQUIRE(staircase.NextLayer() == "");
  }

  SECTION("Simple case 2") {
    utils::Staircase staircase(2, 1, 2);

    REQUIRE(staircase.MaxWidth() == 2);

    REQUIRE(staircase.NextLayer() == "0");
    REQUIRE(staircase.NextLayer() == "0");
    REQUIRE(staircase.NextLayer() == "0 1");
    REQUIRE(staircase.NextLayer() == "0 1");
    REQUIRE(staircase.NextLayer() == "");
  }

  SECTION("Wide steps") {
    utils::Staircase staircase(2, 3, 2);

    REQUIRE(staircase.MaxWidth() == 6);

    REQUIRE(staircase.NextLayer() == "0 1 2");
    REQUIRE(staircase.NextLayer() == "0 1 2");
    REQUIRE(staircase.NextLayer() == "0 1 2 3 4 5");
    REQUIRE(staircase.NextLayer() == "0 1 2 3 4 5");
    REQUIRE(staircase.NextLayer() == "");
  }

}





