# This Makefile is for organizing frequently used commands, not for compilation.
.ONESHELL:

NUM_CORES=8

all:
	./build.sh 2>&1 | tee build.log


indexer:
	python ./tools/indexer.py

hash_benchmark:
	./build.sh
	./build/hash_benchmark

scoring_benchmark:
	rm -f ./build/scoring_bench
	mkdir -p ./build
	cd build
	cmake ../src/
	make -j${NUM_CORES} scoring_bench
	
	cd ..
	./build/scoring_bench

intersect_benchmark:
	./build.sh
	./build/intersect_bench

compression_bench:
	./build.sh
	./build/compression_bench

grpc_benchmark:
	./build.sh
	echo ------------------------------------------------------
	sudo pkill qq_server
	sudo pkill engine_bench
	python ./tools/run_client_server2.py \
		./build/engine_bench '-exp_mode=grpclog -n_threads=16 -use_profiler=true -grpc_server=localhost' \
		./build/qq_server '-sync_type=ASYNC -n_threads=1 -addr=localhost -engine=vacuum:vacuum_dump:/mnt/ssd/vacuum_engine_dump_magic' \
		20	
		#./build/engine_bench '-exp_mode=grpclog -n_threads=16 -use_profiler=true -grpc_server=localhost:50051' \

engine_bench:
	cd build && cmake ../src/ && \
		make -j${NUM_CORES} engine_bench && \
		#CPUPROFILE_FREQUENCY=1000 ./engine_bench -exp_mode=locallog -use_profiler=true
		./engine_bench -exp_mode=locallog -query_path=/mnt/ssd/query_log/wiki/single_term/type_single.docfreq_low.workloadOrig_wiki
		#HEAPPROFILE=/tmp/heapprof ./engine_bench -exp_mode=locallog -use_profiler=true

remote_engine_bench:
	cd build && cmake ../src/ && \
		make -j${NUM_CORES} engine_bench && \
		rsync -avh --progress ./engine_bench node2:/tmp/
		#HEAPPROFILE=/tmp/heapprof ./engine_bench -exp_mode=locallog -use_profiler=true



packing_bench:
	cd build && cmake ../src/ && \
		make -j${NUM_CORES} packing_bench && \
		./packing_bench

heap_check:
	cd build && cmake ../src/ && \
		make -j${NUM_CORES} engine_bench && \
		HEAPCHECK=normal ./engine_bench -exp_mode=locallog -use_profiler=false


engine_bench_build:
	cd build && cmake ../src/ && make -j${NUM_CORES} engine_bench &&
	make -j${NUM_CORES} qq_server

qq_server:
	cd build && cmake ../src/ && make -j${NUM_CORES} qq_server

engine_bench_debug:
	cd build && cmake ../src/ && \
		make -j${NUM_CORES} engine_bench && \
		gdb --args ./engine_bench -exp_mode=locallog -use_profiler=false

convert_qq_to_vacuum:
	cd build && cmake ../src/ && \
		make -j${NUM_CORES} convert_qq_to_vacuum && \
		./convert_qq_to_vacuum \
		-qqdump_dir_path=/mnt/ssd/wiki-May-qq-dump/ \
		-vacuum_dir_path=/mnt/ssd/vacuum-05-23-wiki-2018May

convert_qq_to_vacuum_baseline:
	cd build && cmake ../src/ && \
		make -j${NUM_CORES} convert_qq_to_vacuum && \
		./convert_qq_to_vacuum \
		-use_bloom_filters=false \
		-align_doc_store=false \
		-qqdump_dir_path=/mnt/ssd/wiki/wiki_full.linedoc_tokenized_preprocessed_pre_after \
		-vacuum_dir_path=/mnt/ssd/vacuum-wiki-06-24-baseline

check_posting_list:
	cd build && cmake ../src/ && \
		make -j${NUM_CORES} check_posting_list && \
		./check_posting_list

show_bloom_store_stats:
	cd build && cmake ../src/ && \
		make -j${NUM_CORES} show_bloom_store_stats && \
		./show_bloom_store_stats


trie_bench:
	cd build && cmake ../src/ && \
		make -j${NUM_CORES} trie_bench && \
		./trie_bench \
		-term_path=/mnt/ssd/benchmark_trie/redditaa_tokenized.terms.set:/mnt/ssd/benchmark_trie/redditab_tokenized.terms.set:/mnt/ssd/benchmark_trie/redditac_tokenized.terms.set:/mnt/ssd/benchmark_trie/redditad_tokenized.terms.set
		#./trie_bench -term_path=/mnt/ssd/benchmark_trie/popular_terms_2018-may_wiki.shuffled
		#./trie_bench -term_path=/mnt/ssd/benchmark_trie/popular_terms_2018-may_wiki.shuffled
		#HEAPPROFILE=/tmp/heapprof ./trie_bench

trie_bench_heap:
	google-pprof --text build/trie_bench ${path}

create_qq_mem_dump:
	cd build && cmake ../src/ && \
	make create_qq_mem_dump && \
	./create_qq_mem_dump -line_doc_path=/mnt/ssd/wiki/wiki_full.linedoc_tokenized_preprocessed \
	-dump_dir_path=/mnt/ssd/vacuum-june-wiki-bloom


create_bloom_store:
	cd build && cmake ../src/ && \
	make create_bloom_store && \
	./create_bloom_store -line_doc_path=/users/jhe/flashsearch/src/qq_mem/src/testdata/iter_test_3_docs_tf_bi-bloom \
	-dump_dir_path=/mnt/ssd/vacuum-tmp


vacuum_two_nodes:
	cd build 
	cmake ../src/
	make -j${NUM_CORES} qq_server 
	make -j${NUM_CORES} engine_bench
	cd ..
	python ./tools/run_vacuum_two_nodes.py

elastic_two_nodes:
	python ./tools/run_elastic_two_nodes.py

exp:
	python ./tools/run_exp.py

profile:
	google-pprof --text ./build/engine_bench ./build/my.profile.new

server_profile:
	google-pprof --text ./build/qq_server ./vacuum.profile

server_profile_gv:
	google-pprof --pdf ./build/qq_server ./vacuum.profile > profile.pdf

profile_gv:
	google-pprof --pdf ./build/engine_bench ./build/my.profile.new > profile.pdf

run_qq_mem:
	./build.sh
	python ./tools/run_client_server.py ./build/client "" ./build/qq_server 50051,0

server:
	./build.sh
	./build/qq_server 50051 0

client:
	./build/qq_client

test:
	rm -f ./build/testmain
	mkdir -p ./build
	cd build
	cmake ../src/
	make -j${NUM_CORES} testmain
	cd ..
	./build/testmain ${args}

test_fast:
	rm -f ./build/testmain
	mkdir -p ./build
	cd build
	cmake ../src/
	make -j${NUM_CORES} testmain
	cd ..
	./build/testmain '~[grpc]~[slow]'

test_args:
	rm -f ./build/testmain
	mkdir -p ./build
	cd build
	cmake ../src/
	make -j${NUM_CORES} testmain
	cd ..
	./build/testmain ${args}

test_last:
	rm -f ./build/testlast
	mkdir -p ./build
	cd build
	cmake ../src/
	make -j${NUM_CORES} testlast
	cd ..
	./build/testlast 
	#./build/testlast '[blm]'

test_last_args:
	rm -f ./build/testlast
	mkdir -p ./build
	cd build
	cmake ../src/
	make -j${NUM_CORES} testlast
	cd ..
	./build/testmain ${args}


test_qqflash:
	rm -f ./build/testmain
	mkdir -p ./build
	cd build
	cmake ../src/
	make -j16 testmain
	cd ..
	./build/testmain "[qqflash]"

test_bf:
	rm -f ./build/testmain
	mkdir -p ./build
	cd build
	cmake ../src/
	make -j16 testmain
	cd ..
	./build/testmain "[bloomfilter]"

snippet_bench:
	./build.sh
	./build/snippet_bench
