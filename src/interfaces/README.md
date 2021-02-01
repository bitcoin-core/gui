# Internal c++ [`src/interfaces/`](../interfaces)

### The following interfaces are defined here:

* [`Chain`](chain.h) — used by wallet to access blockchain and mempool state. Added in [#14437](https://github.com/bitcoin/bitcoin/pull/14437), [#14711](https://github.com/bitcoin/bitcoin/pull/14711), [#15288](https://github.com/bitcoin/bitcoin/pull/15288), and [#10973](https://github.com/bitcoin/bitcoin/pull/10973).
	- Refactored in [#20494](https://github.com/bitcoin/bitcoin/pull/20494/commits)

* [`ChainClient`](chain.h) — used by node to start & stop `Chain` clients. Added in [#14437](https://github.com/bitcoin/bitcoin/pull/14437).

* [`Node`](node.h) — used by GUI to start & stop bitcoin node. Added in [#10244](https://github.com/bitcoin/bitcoin/pull/10244).
	- Refactored in [#20494](https://github.com/bitcoin/bitcoin/pull/20494/commits)

* [`Wallet`](wallet.h) — used by GUI to access wallets. Added in [#10244](https://github.com/bitcoin/bitcoin/pull/10244).
	- Refactored in [#20494](https://github.com/bitcoin/bitcoin/pull/20494/commits)

* [`Handler`](handler.h) — returned by `handleEvent` methods on interfaces above and used to manage lifetimes of event handlers.

---

### Additional Notes:

The [`src/node/`](../node) directory is a new directory introduced in
[#14978](https://github.com/bitcoin/bitcoin/pull/14978) and at the moment is
sparsely populated. Eventually more substantial files like
[`src/validation.cpp`](../validation.cpp) and
[`src/txmempool.cpp`](../txmempool.cpp) may be moved there.

The [`src/node/`](../node) directory contains code that is needed to access node state
(state in `CChain`, `CBlockIndex`, `CCoinsView`, `CTxMemPool`, and similar
classes).

---

* [`Init`](../init.h) — used by multiprocess code to access interfaces above on startup. Added in [#10102](https://github.com/bitcoin/bitcoin/pull/10102).


---

The interfaces above define boundaries between major components of bitcoin code (node, wallet, and gui), making it possible for them to run in different processes, and be tested, developed, and understood independently. These interfaces are not currently designed to be stable or to be used externally.

