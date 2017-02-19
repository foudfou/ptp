# PTP ambitious project

Pet project to study P2P concepts.

For now, only basic data structures (list, hash, trees) are provided.

## Build

You need `ninja` installed first.

    mkdir build
    cd build
    ../tools/meson.py ..
    ninja

Clang may wrongly [complain for missing field initializers](https://llvm.org/bugs/show_bug.cgi?id=21689).

## Usage

`kldload mqueuefs` on FreeBSD.

## Acknowledgements

* [cjdns](https://github.com/cjdelisle/cjdns/) by Caleb James DeLisle
* [Linux kernel](https://www.kernel.org/) by the Linux community
* [nanomsg](https://github.com/nanomsg/nanomsg) by Martin Sustrik
* [FreeBSD](http://www.FreeBSD.org/) bits by Niels Provos
* [Nadeem Abdul Hamid's P2P Programming Framework](http://cs.berry.edu/~nhamid/p2p/framework-python.html)
