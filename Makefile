PROTOC = protoc
LDFLAGS = $(shell pkg-config --libs grpc++) $(shell pkg-config --libs protobuf) -ldl
CXX = g++
CXXFLAGS = -O0 -std=c++14 -pthread -g
GRPC_CPP_PLUGIN_PATH = $(shell which grpc_cpp_plugin)
all: main
	
main: main.cc ps_service.pb.cc ps_service.grpc.pb.cc
	$(CXX) $(CXXFLAGS) main.cc  ps_service.pb.cc ps_service.grpc.pb.cc $(LDFLAGS) -o main

clean:
	rm -f *pb* main


%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(shell dirname $@) --grpc_out=$(shell dirname $@) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

%.pb.cc: %.proto
	$(PROTOC) -I $(shell dirname $@) --cpp_out=$(shell dirname $@) $<
