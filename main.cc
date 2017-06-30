#include <iostream>
#include <algorithm>
#include <string>
#include <atomic>
#include <thread>
#include <vector>

#include <grpc++/grpc++.h>

#include "ps_service.grpc.pb.h"

using namespace std::chrono_literals;

std::mutex cout_mu;

/* Sync Server */
class PsServiceServer: public PsService::Service
{
public:
    PsServiceServer(size_t num_hosts):
        iterations_(num_hosts, -1) {}

    grpc::Status CheckAlive(grpc::ServerContext* ctx,
            const CheckAliveRequest* req,
            CheckAliveResponse* res) {
        res->set_status(true);
        return grpc::Status::OK;
    }


    grpc::Status Update(grpc::ServerContext* ctx,
            const UpdateRequest* req,
            UpdateResponse* res) {
        {
            std::unique_lock<std::mutex> lock(cout_mu);
            std::cout << "receive from client: " << req->client() << " iteration: " << req->iteration() << std::endl;
        }
        std::unique_lock<std::mutex> lock(mu_);
        auto& iteration = iterations_[req->client()];
        iteration = req->iteration();
        cv_.notify_all();
        int min;
        /* Wait until all updates from clients is received */
        cv_.wait(lock, [this, &min, iteration]{
            min = *std::min_element(iterations_.begin(), iterations_.end());
            return min >= iteration;
        });
        res->set_iteration(min);
        return grpc::Status::OK;
    }
private:
    std::mutex mu_;
    std::condition_variable cv_;
    std::vector<int> iterations_;
};

struct Context
{
    int this_host;
    std::vector<std::string> hosts;
    std::atomic<int> iteration;
    std::vector<int> iterations;

    std::unique_ptr<PsServiceServer> service; 
    std::unique_ptr<grpc::Server> server;
    std::vector<std::unique_ptr<PsService::Stub>> stubs; 
    grpc::CompletionQueue cq;
    std::unique_ptr<std::thread> server_thread_;
    std::unique_ptr<std::thread> client_thread_;

    std::mutex mu;
    std::condition_variable cv;
};

std::unique_ptr<Context> c;

void server_thread_func() {
    c->server->Wait();
}

struct UpdateTag {
    int server;
    int iteration;
    grpc::ClientContext ctx;
    UpdateResponse res;
    grpc::Status status;
};



void client_thread_func_() {
    void* got_tag;
    bool ok = false;
    while(c->cq.Next(&got_tag, &ok)) {
        auto tag = (UpdateTag*)got_tag;
        if (!tag->status.ok()) {
            std::cout << "Update gRPC failed. Error code: " << 
                tag->status.error_code() << std::endl;
            exit(0);
        }
        std::unique_lock<std::mutex> lock(c->mu);
        if (c->iterations[tag->server] < tag->res.iteration()) {
            c->iterations[tag->server] = tag->res.iteration();
        }
        delete tag;
        c->cv.notify_all();
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cout << "usage: " << argv[0] << " this_host hosts..." << std::endl;
        exit(0);
    }


    /* Global context initialize*/
    c = std::make_unique<Context>();
    c ->this_host = std::stoi(argv[1]);
    for (int i = 2; i < argc; ++i) {
        c->hosts.push_back(argv[i]);
    }
    c->iteration = 0;
    c->iterations.resize(c->hosts.size(), -1);

    /* Sever initialize */
    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50101", grpc::InsecureServerCredentials());
    c->service = std::make_unique<PsServiceServer>(c->hosts.size());
    builder.RegisterService(c->service.get());
    c->server = builder.BuildAndStart();
    c->server_thread_ = std::make_unique<std::thread>(server_thread_func);
    std::this_thread::sleep_for(100ms);

    /* Check servers alive */
    for (auto& host: c->hosts){
        while(1) {
            auto stub = PsService::NewStub(
                    grpc::CreateChannel(
                        host+":50101", grpc::InsecureChannelCredentials()));
            grpc::ClientContext ctx;
            CheckAliveRequest req;
            CheckAliveResponse res;
            stub->CheckAlive(&ctx, req, &res);
            if (res.status()) {
                std::cout << host << " is up." << std::endl;
                c->stubs.push_back(std::move(stub));
                break;

            } else {
                std::cout << "Failed to connect to " << host <<"." << std::endl;
                std::this_thread::sleep_for(1s);
            }
        }
    }

    /* Listen to completion queue */
    c->client_thread_ = std::make_unique<std::thread>(client_thread_func_);
    std::this_thread::sleep_for(100ms);

    while(1) {
        /* Client update request */
        for(size_t server = 0; server < c->hosts.size(); server++) {
            UpdateTag* tag = new UpdateTag();
            tag->server = server;
            tag->iteration = c->iteration;
            UpdateRequest req;
            req.set_client(c->this_host);
            req.set_iteration(c->iteration);

            {
                std::unique_lock<std::mutex> lock(cout_mu);
                std::cout << "send to server: " << server << " iteration: " << c->iteration << std::endl;
            }
            c->stubs[server]->AsyncUpdate(&tag->ctx, req, &c->cq)->
                Finish(&tag->res, &tag->status, (void*) tag);
        }        
        /* Update complete. Next iteration */
        c->iteration++;

        /* Sync data */
        std::unique_lock<std::mutex> lock(c->mu);
        c->cv.wait(lock, []{
            int min = *std::min_element(c->iterations.begin(), c->iterations.end());
            return min >= c->iteration - 1;
        });
    }
    return 0;
}
