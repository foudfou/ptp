# PTP ambitious project

Pet project to study P2P concepts.

## Features

* Some data structures (list, hash, trees).
* Non-blocking IO event loop.
* Kademlia DHT.

## Build

You need [ninja](https://ninja-build.org/) installed first.

    mkdir build
    cd build
    ../tools/meson.py ..
    ninja

Clang may wrongly [complain for missing field initializers](https://llvm.org/bugs/show_bug.cgi?id=21689).
Don't bother.

## Usage

`kldload mqueuefs` on FreeBSD.

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
