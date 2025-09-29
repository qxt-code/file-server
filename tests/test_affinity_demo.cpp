// Simple demo (not a formal test) showing how to create reactors and a thread pool with affinity.
#include "net/io_reactor.h"
#include "net/main_reactor.h"
#include "concurrency/lf_thread_pool.h"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    // Example core mapping (adjust according to available cores)
    net::MainReactor main_reactor(0); // bind to core 0 if exists
    net::IOReactor io1(0, 1);
    net::IOReactor io2(1, 2);
    main_reactor.set_io_reactors({&io1, &io2});
    main_reactor.start();
    io1.start();
    io2.start();

    concurrency::LFThreadPool pool(2, 256, {3,4});
    pool.submit([]{ std::cout << "task1 on pool\n"; });
    pool.submit([]{ std::cout << "task2 on pool\n"; });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    main_reactor.stop();
    io1.stop();
    io2.stop();
    pool.shutdown();
    return 0;
}