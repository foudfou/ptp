# PTP ambitious project

Pet project to study P2P concepts.

## Features

* Some data structures (list, hash, trees), non-thread-safe.
* Non-blocking IO event loop.
* Kademlia DHT.

## Build

Install [meson](https://mesonbuild.com/) and [ninja](https://ninja-build.org/).
Then

    mkdir build && cd build
    meson
    # meson configure -Db_coverage=true -Db_sanitize=address
    ninja

## Usage

`kldload mqueuefs` on FreeBSD.

    src/ptp -h                    # print help
    src/ptp -a 127.0.0.1 -p 2222  # start binding to 127.0.0.1:2222

## Acknowledgements

### Algorithms

* [Julienne Walker's tutorials](http://www.eternallyconfuzzled.com/)
* [cjdns](https://github.com/cjdelisle/cjdns/) by Caleb James DeLisle
* [Linux kernel](https://www.kernel.org/) by the Linux community
* [nanomsg](https://github.com/nanomsg/nanomsg) by Martin Sustrik
* [FreeBSD](http://www.FreeBSD.org/) bits by Niels Provos

### P2P

* [Nadeem Abdul Hamid's P2P Programming Framework](http://cs.berry.edu/~nhamid/p2p/framework-python.html)
* [Jakob Jenkov's P2P Tutorial](http://tutorials.jenkov.com/p2p/disorganized-network.html)
* [BitTorrent DHT Protocol specifications](http://www.bittorrent.org/beps/bep_0005.html) by
  Andrew Loewenstern and Arvid Norberg
* [Kademlia's paper](http://www.scs.stanford.edu/~dm/home/papers/maymounkov:kademlia.ps.gz)
