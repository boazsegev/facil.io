# facil.io - a mini-framework for C web applications

**NOTICE**:

The `master` branch is temporarily outdated.

The development has temporarily moved to the [`reHTTP` branch](https://github.com/boazsegev/facil.io/tree/reHTTP), which started as a small rewrite to the HTTP layer and ended up being a major upgrade and API unification process for the whole framework - the upcoming v.0.6.0 release. 

The latest 0.5.x updates are performed in the [`0.5.x-backports` branch](https://github.com/boazsegev/facil.io/tree/0.5.x-backports).

Please don't push PRs to the `master` branch. Version 0.5.x PRs should go to the [`0.5.x-backports` branch](https://github.com/boazsegev/facil.io/tree/0.5.x-backports) and edge version (0.6.0) PRs should go to the [`reHTTP` branch](https://github.com/boazsegev/facil.io/tree/reHTTP).

---

## Forking, Contributing and all that Jazz

Sure, why not. If you can add Solaris or Windows support to `evio`, that could mean `facil` would become available for use on these platforms as well (as well as the HTTP protocol implementation and all the niceties).

If you encounter any issues, open an issue (or, even better, a pull request with a fix) - that would be great :-)

Hit me up if you want to:

* Help me write HPACK / HTTP2 protocol support.

* Help me design / write a generic HTTP routing helper library for the `http_s` struct (upcoming v.0.6.0).

* If you have an SSL/TLS solution we can fit into `facil` (as source code).

* If you want to help promote the library, that would be great as well. Perhaps publish [benchmarks](https://github.com/TechEmpower/FrameworkBenchmarks)) or share your story.

* Writing documentation into the `facil.io` website would be great. I keep the source code documentation fairly updated, but sometimes I can be a lazy bastard.
